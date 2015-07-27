/*
 * Copyright (C) 2013 Victor Toso.
 *
 * Contact: Victor Toso <me@victortoso.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include "config.h"

#include <net/grl-net.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <archive.h>
#include <archive_entry.h>

#include "grl-lua-common.h"
#include "grl-lua-library.h"
#include "lua-library/lua-libraries.h"
#include "lua-library/htmlentity.h"

#ifdef HAVE_TOTEM_PLPARSER_MINI
#include <totem-pl-parser-mini.h>
#endif

#define GRL_LOG_DOMAIN_DEFAULT lua_library_log_domain
GRL_LOG_DOMAIN_STATIC (lua_library_log_domain);

typedef struct {
  lua_State *L;
  guint operation_id;
  gchar *lua_cb;
  guint index;
  gchar *url;
  guint num_urls;
  gboolean is_table;
  gchar **results;
} FetchOperation;

typedef struct {
  lua_State *L;
  guint operation_id;
  gchar *lua_cb;
  gchar *url;
  gchar **filenames;
} UnzipOperation;

/* ================== Lua-Library utils/helpers ============================ */

static gchar *
char_str (gunichar c,
          gchar   *buf)
{
  memset (buf, 0, 8);
  g_unichar_to_utf8 (c, buf);
  return buf;
}

/* ANSI HTML entities
 * http://www.w3schools.com/charsets/ref_html_ansi.asp */
static gchar *
ansi_char_str (gunichar c,
               gchar   *buf)
{
  gchar from_c[2], *tmp;

  memset (buf, 0, 8);
  from_c[0] = c;
  from_c[1] = '\0';
  tmp = g_convert (from_c, 2, "UTF-8", "Windows-1252", NULL, NULL, NULL);
  strcpy (buf, tmp);
  g_free (tmp);

  return buf;
}

/* Adapted from unescape_gstring_inplace() in gmarkup.c in glib */
static char *
unescape_string (const char *orig_from)
{
  char *to, *from, *ret;

  /*
   * Meeks' theorem: unescaping can only shrink text.
   * for &lt; etc. this is obvious, for &#xffff; more
   * thought is required, but this is patently so.
   */
  ret = g_strdup (orig_from);

  for (from = to = ret; *from != '\0'; from++, to++) {
    *to = *from;

    if (*to == '\r') {
      *to = '\n';
      if (from[1] == '\n')
        from++;
    }
    if (*from == '&') {
      from++;
      if (*from == '#') {
        gboolean is_hex = FALSE;
        gulong l;
        gchar *end = NULL;

        from++;

        if (*from == 'x') {
          is_hex = TRUE;
          from++;
        }

        /* digit is between start and p */
        errno = 0;
        if (is_hex)
          l = strtoul (from, &end, 16);
        else
          l = strtoul (from, &end, 10);

        if (end == from || errno != 0)
          continue;
        if (*end != ';')
          continue;
        /* characters XML 1.1 permits */
        if ((0 < l && l <= 0xD7FF) ||
            (0xE000 <= l && l <= 0xFFFD) ||
            (0x10000 <= l && l <= 0x10FFFF)) {
          gchar buf[8];
          if (l >= 128 && l <= 255)
            ansi_char_str (l, buf);
          else
            char_str (l, buf);
          strcpy (to, buf);
          to += strlen (buf) - 1;
          from = end;
        } else {
          continue;
        }
      } else {
        gchar *end = NULL;

        end = strstr (from, ";");
        if (!end)
          continue;

        *to = html_entity_parse (from, end - from);
        from = end;
      }
    }
  }

  *to = '\0';

  return ret;
}

static void
grl_data_set_lua_string (GrlData    *data,
                         GrlKeyID    key_id,
                         const char *key_name,
                         const char *str)
{
  char *fixed = NULL;

  /* Check for UTF-8 or ISO8859-1 string */
  if (g_utf8_validate (str, -1, NULL) == FALSE) {
    fixed = g_convert (str, -1, "UTF-8", "ISO8859-1", NULL, NULL, NULL);
    if (fixed == NULL) {
      GRL_WARNING ("Ignored non-UTF-8 and non-ISO8859-1 string for field '%s'", key_name);
      return;
    }
  }

  if (fixed) {
    grl_data_set_string (data, key_id, fixed);
    g_free (fixed);
  } else {
    grl_data_set_string (data, key_id, str);
  }
}

static void
grl_data_add_lua_string (GrlData    *data,
                         GrlKeyID    key_id,
                         const char *key_name,
                         const char *str)
{
  char *fixed = NULL;

  /* Check for UTF-8 or ISO8859-1 string */
  if (g_utf8_validate (str, -1, NULL) == FALSE) {
    fixed = g_convert (str, -1, "UTF-8", "ISO8859-1", NULL, NULL, NULL);
    if (fixed == NULL) {
      GRL_WARNING ("Ignored non-UTF-8 and non-ISO8859-1 string for field '%s'", key_name);
      return;
    }
  }

  if (fixed) {
    grl_data_add_string (data, key_id, fixed);
    g_free (fixed);
  } else {
    grl_data_add_string (data, key_id, str);
  }
}

static gboolean
verify_plaintext_fetch (GrlSource  *source,
                        char      **urls,
                        guint       num_urls)
{
  const char **tags;
  gboolean has_plaintext_tag;
  guint i;

  tags = grl_source_get_tags (source);
  has_plaintext_tag = (tags && g_strv_contains (tags, "net:plaintext"));

  /* No need to verify the URLs, the source is saying that they do
   * plaintext queries, so nothing for us to block */
  if (has_plaintext_tag)
    return TRUE;

  for (i = 0; i < num_urls; i++) {
    if (g_str_has_prefix (urls[i], "http:"))
      return FALSE;
  }

  return TRUE;
}

/* Top of the stack must be a table */
static void
grl_util_add_table_to_media (lua_State *L,
                             GrlMedia *media,
                             GrlKeyID key_id,
                             const gchar *key_name,
                             GType type)
{
  gint i;
  gint array_len = luaL_len (L, -1);

  /* Remove all current values of this key, if any */
  while (grl_data_length (GRL_DATA (media), key_id) > 0) {
    grl_data_remove (GRL_DATA (media), key_id);
  }

  /* Insert new values */
  for (i = 0; i < array_len; i++) {
    lua_pushinteger (L, i + 1);
    lua_gettable (L, -2);
    switch (type) {
    case G_TYPE_INT:
      if (lua_isnumber (L, -1))
        grl_data_add_int (GRL_DATA (media), key_id, lua_tointeger (L, -1));
      break;

    case G_TYPE_INT64:
      if (lua_isnumber (L, -1))
        grl_data_add_int64 (GRL_DATA (media), key_id, lua_tointeger (L, -1));
      break;

    case G_TYPE_FLOAT:
      if (lua_isnumber (L, -1))
        grl_data_add_float (GRL_DATA (media), key_id, lua_tointeger (L, -1));
      break;

    case G_TYPE_STRING:
      if (lua_isstring (L, -1))
        grl_data_add_lua_string (GRL_DATA (media), key_id, key_name, lua_tostring (L, -1));
      break;

    default:
        GRL_DEBUG ("'%s' is being ignored when value is a table object",
                   key_name);
    }
    lua_pop (L, 1);
  }
}

static GrlMedia *
grl_util_build_media (lua_State *L,
                      GrlMedia *user_media)
{
  GrlRegistry *registry;
  GrlMedia *media = user_media;

  if (!lua_istable (L, 1)) {
    if (!lua_isnil (L, 1))
      GRL_DEBUG ("Media in wrong format (neither nil or table).");

    return user_media;
  }

  if (media == NULL) {
    lua_getfield (L, 1, "type");
    if (lua_isstring (L, -1)) {
      const gchar *media_type = lua_tostring (L, -1);

      if (g_strcmp0 (media_type, "box") == 0)
        media = grl_media_box_new ();
      else if (g_strcmp0 (media_type, "image") == 0)
        media = grl_media_image_new ();
      else if (g_strcmp0 (media_type, "audio") == 0)
        media = grl_media_audio_new ();
      else if (g_strcmp0 (media_type, "video") == 0)
        media = grl_media_video_new ();
    }
    media = (media == NULL) ? grl_media_new () : media;
    lua_pop (L, 1);
  }

  registry = grl_registry_get_default ();
  lua_pushnil (L);
  while (lua_next (L, 1) != 0) {
    GrlKeyID key_id = GRL_METADATA_KEY_INVALID;
    gchar *key_name = g_strdup (lua_tostring (L, -2));
    gchar *ptr = NULL;
    GType type = G_TYPE_NONE;

    /* Handled above */
    if (g_strcmp0 (key_name, "type") == 0) {
      goto next_key;
    }

    /* Replace '_' to '-': convenient for the developer */
    while ((ptr = strstr (key_name, "_")) != NULL) {
      *ptr = '-';
    }

    key_id = grl_registry_lookup_metadata_key (registry, key_name);
    if (key_id != GRL_METADATA_KEY_INVALID) {
      type = grl_registry_lookup_metadata_key_type (registry, key_id);

      switch (type) {
      case G_TYPE_INT:
        if (lua_isnumber (L, -1)) {
          grl_data_set_int (GRL_DATA (media), key_id, lua_tointeger (L, -1));
        } else if (lua_istable (L, -1)) {
          grl_util_add_table_to_media (L, media, key_id, key_name, type);
        } else if (!lua_isnil (L, -1)) {
          GRL_WARNING ("'%s' is not compatible for '%s'",
                       lua_typename (L, -1), key_name);
        }
        break;

      case G_TYPE_FLOAT:
        if (lua_isnumber (L, -1)) {
          grl_data_set_float (GRL_DATA (media), key_id, lua_tonumber (L, -1));
        } else if (lua_istable (L, -1)) {
          grl_util_add_table_to_media (L, media, key_id, key_name, type);
        } else if (!lua_isnil (L, -1)) {
          GRL_WARNING ("'%s' is not compatible for '%s'",
                       lua_typename (L, -1), key_name);
        }
        break;

      case G_TYPE_INT64:
        if (lua_isnumber (L, -1)) {
          grl_data_set_int64 (GRL_DATA (media), key_id, lua_tointeger (L, -1));
        } else if (lua_istable (L, -1)) {
          grl_util_add_table_to_media (L, media, key_id, key_name, type);
        } else if (!lua_isnil (L, -1)) {
          GRL_WARNING ("'%s' is not compatible for '%s'",
                       lua_typename (L, -1), key_name);
        }
        break;

      case G_TYPE_STRING:
        if (lua_isstring (L, -1)) {
          grl_data_set_lua_string (GRL_DATA (media), key_id, key_name, lua_tostring (L, -1));
        } else if (lua_istable (L, -1)) {
          grl_util_add_table_to_media (L, media, key_id, key_name, type);
        } else if (!lua_isnil (L, -1)) {
          GRL_WARNING ("'%s' is not compatible for '%s'",
                       lua_typename (L, -1), key_name);
        }
        break;
      case G_TYPE_BOOLEAN:
        if (lua_isboolean (L, -1)) {
          grl_data_set_boolean (GRL_DATA (media), key_id, lua_toboolean (L, -1));
        } else if (!lua_isnil (L, -1)) {
          GRL_WARNING ("'%s' is not compatible for '%s'",
                       lua_typename (L, -1), key_name);
        }
        break;

      default:
        /* Non-fundamental types don't reduce to ints, so can't be
         * in the switch statement */
        if (type == G_TYPE_DATE_TIME) {
          GDateTime *date;
          const char *date_str = lua_tostring (L, -1);
          date = grl_date_time_from_iso8601 (date_str);
          /* Try a number of seconds since Epoch */
          if (!date) {
            gint64 date_int = g_ascii_strtoll (date_str, NULL, 0);
            if (date_int)
              date = g_date_time_new_from_unix_utc (date_int);
          }
          if (date) {
            grl_data_set_boxed (GRL_DATA (media), key_id, date);
            g_date_time_unref (date);
          } else {
            GRL_WARNING ("'%s' is not a valid ISO-8601 or Epoch date", date_str);
          }
        } else if (type == G_TYPE_BYTE_ARRAY) {
           gsize size = luaL_len (L, -1);
           const guint8 *binary = (const guint8 *) lua_tostring (L, -1);
           grl_data_set_binary (GRL_DATA (media), key_id, binary, size);
        } else if (!lua_isnil (L, -1)) {
          GRL_WARNING ("'%s' is being ignored as G_TYPE is not being handled.",
                       key_name);
        }
      }
    } else {
      GRL_WARNING ("'%s' is not a valid keyword", key_name);
    }

next_key:
    g_free (key_name);
    lua_pop (L, 1);
  }
  return media;
}

static void
grl_util_fetch_done (GObject *source_object,
                     GAsyncResult *res,
                     gpointer user_data)
{
  gchar *data;
  gsize len;
  guint i;
  GError *err = NULL;
  OperationSpec *os;
  FetchOperation *fo = (FetchOperation *) user_data;
  lua_State *L = fo->L;
  gchar *fixed = NULL;

  if (!grl_net_wc_request_finish (GRL_NET_WC (source_object),
                                  res, &data, &len, &err)) {
    data = NULL;
  } else if (!g_utf8_validate(data, len, NULL)) {
    fixed = g_convert (data, len, "UTF-8", "ISO8859-1", NULL, NULL, NULL);
    if (!fixed) {
      g_set_error_literal (&err, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                           "Fetched item is not valid UTF-8 or ISO8859-1");
      data = NULL;
    } else {
      data = fixed;
    }
  }

  fo->results[fo->index] = (err == NULL) ? g_strdup (data) : g_strdup ("");
  g_free (fixed);

  if (err != NULL) {
    GRL_WARNING ("Can't fetch element %d (URL: %s): '%s'", fo->index + 1, fo->url, err->message);
    g_error_free (err);
  } else {
    GRL_DEBUG ("fetch_done element %d of %d urls", fo->index + 1, fo->num_urls);
  }

  /* Check if we finished fetching URLs */
  for (i = 0; i < fo->num_urls; i++) {
    if (fo->results[i] == NULL) {
      /* Clean up this operation, and wait for
       * other operations to complete */
      g_free (fo->lua_cb);
      g_free (fo);
      return;
    }
  }

  grl_lua_library_set_current_operation (L, fo->operation_id);
  os = grl_lua_library_get_current_operation (L);
  os->pending_ops--;

  lua_getglobal (L, fo->lua_cb);

  if (!fo->is_table) {
    lua_pushlstring (L, fo->results[0], strlen (fo->results[0]));
  } else {
    lua_newtable (L);
    for (i = 0; i < fo->num_urls; i++) {
      lua_pushnumber (L, i + 1);
      lua_pushlstring (L, fo->results[i], strlen (fo->results[i]));
      lua_settable (L, -3);
    }
  }

  if (lua_pcall (L, 1, 0, 0)) {
    GRL_WARNING ("%s (%s) '%s'", "calling source callback function fail",
                 fo->lua_cb, lua_tolstring (L, -1, NULL));
  }

  grl_lua_library_set_current_operation (L, 0);

  for (i = 0; i < fo->num_urls; i++)
    g_free (fo->results[i]);
  g_free (fo->url);
  g_free (fo->results);
  g_free (fo->lua_cb);
  g_free (fo);
}

static gboolean
str_in_strv_at_index (const char **filenames,
                      const char  *name,
                      guint       *idx)
{
  guint i;

  for (i = 0; filenames[i] != NULL; i++) {
    if (g_strcmp0 (name, filenames[i]) == 0) {
      *idx = i;
      return TRUE;
    }
  }

  return FALSE;
}

static char **
get_zipped_contents (guchar        *data,
                     gsize          size,
                     const char   **filenames)
{
  GPtrArray *results;
  struct archive *a;
  struct archive_entry *entry;
  int r;

  a = archive_read_new ();
  archive_read_support_format_zip (a); //FIXME more formats?
  r = archive_read_open_memory (a, data, size);
  if (r != ARCHIVE_OK) {
    g_print ("Failed to open archive\n");
    return NULL;
  }

  results = g_ptr_array_new ();
  g_ptr_array_set_size (results, g_strv_length ((gchar **) filenames) + 1);

  while (1) {
    const char *name;
    guint idx;

    r = archive_read_next_header(a, &entry);

    if (r != ARCHIVE_OK) {
      if (r != ARCHIVE_EOF && r == ARCHIVE_FATAL)
        GRL_WARNING ("Fatal error handling archive: %s", archive_error_string (a));
      break;
    }

    name = archive_entry_pathname (entry);
    if (str_in_strv_at_index (filenames, name, &idx) != FALSE) {
      size_t size = archive_entry_size (entry);
      char *buf;
      ssize_t read;

      buf = g_malloc (size + 1);
      buf[size] = 0;
      read = archive_read_data (a, buf, size);
      if (read <= 0) {
        g_free (buf);
        if (read < 0)
          GRL_WARNING ("Fatal error reading '%s' in archive: %s", name, archive_error_string (a));
        else
          GRL_WARNING ("Read an empty file from the archive");
      } else {
        GRL_DEBUG ("Setting content for %s at %d", name, idx);
        /* FIXME check for validity? */
        results->pdata[idx] = buf;
      }
    }
    archive_read_data_skip(a);
  }
  archive_read_free(a);

  return (gchar **) g_ptr_array_free (results, FALSE);
}

static void
grl_util_unzip_done (GObject *source_object,
                     GAsyncResult *res,
                     gpointer user_data)
{
  gchar *data;
  gsize len;
  guint i;
  GError *err = NULL;
  OperationSpec *os;
  UnzipOperation *uo = (UnzipOperation *) user_data;
  lua_State *L = uo->L;
  char **results;

  grl_net_wc_request_finish (GRL_NET_WC (source_object),
                             res, &data, &len, &err);

  if (err != NULL) {
    guint len, i;
    GRL_WARNING ("Can't fetch zip file (URL: %s): '%s'", uo->url, err->message);
    g_error_free (err);
    len = g_strv_length (uo->filenames);
    results = g_new0 (gchar *, len + 1);
    for (i = 0; i < len; i++)
      results[i] = g_strdup("");
  } else {
    GRL_DEBUG ("fetch_done element (URL: %s)", uo->url);
    results = get_zipped_contents ((guchar *) data, len, (const gchar **) uo->filenames);
  }

  grl_lua_library_set_current_operation (L, uo->operation_id);
  os = grl_lua_library_get_current_operation (L);
  os->pending_ops--;

  lua_getglobal (L, uo->lua_cb);

  lua_newtable (L);
  for (i = 0; results[i] != NULL; i++) {
    lua_pushnumber (L, i + 1);
    lua_pushlstring (L, results[i], strlen (results[i]));
    lua_settable (L, -3);
  }

  if (lua_pcall (L, 1, 0, 0)) {
    GRL_WARNING ("%s (%s) '%s'", "calling source callback function fail",
                 uo->lua_cb, lua_tolstring (L, -1, NULL));
  }

  grl_lua_library_set_current_operation (L, 0);

  g_strfreev (results);

  g_strfreev (uo->filenames);
  g_free (uo->lua_cb);
  g_free (uo->url);
  g_free (uo);
}

static GrlNetWc *
net_wc_new_with_options(lua_State *L,
                        guint      arg_offset)
{
  GrlNetWc *wc;

  wc = grl_net_wc_new ();
  if (arg_offset < lua_gettop (L) && lua_istable (L, arg_offset)) {
    /* Set GrlNetWc options */
    lua_pushnil (L);
    while (lua_next (L, arg_offset) != 0) {
      const gchar *key = lua_tostring (L, -2);
      if (g_strcmp0 (key, "user-agent") == 0 ||
          g_strcmp0 (key, "user_agent") == 0) {
        const gchar *user_agent = lua_tostring (L, -1);
        g_object_set (wc, "user-agent", user_agent, NULL);

      } else if (g_strcmp0 (key, "cache-size") == 0 ||
                 g_strcmp0 (key, "cache_size") == 0) {
        guint size = lua_tonumber (L, -1);
        grl_net_wc_set_cache_size (wc, size);

      } else if (g_strcmp0 (key, "cache") == 0) {
        gboolean use_cache = lua_toboolean (L, -1);
        grl_net_wc_set_cache (wc, use_cache);

      } else if (g_strcmp0 (key, "throttling") == 0) {
        guint throttling = lua_tonumber (L, -1);
        grl_net_wc_set_throttling (wc, throttling);

      } else if (g_strcmp0 (key, "loglevel") == 0) {
        guint level = lua_tonumber (L, -1);
        grl_net_wc_set_log_level (wc, level);

      } else {
        GRL_DEBUG ("GrlNetWc property not know: '%s'", key);
      }
      lua_pop (L, 1);
    }
  }

  return wc;
}

/* ================== Lua-Library methods ================================== */

/**
* grl.get_options
*
* @option: (string) Name of the option you want (e.g. count, flags).
* @key: (string) Name of the key when option request it.
* @return: The option or nil if none;
*/
static gint
grl_l_operation_get_options (lua_State *L)
{
  OperationSpec *os;
  const gchar *option;

  luaL_argcheck (L, lua_isstring (L, 1), 1, "expecting option (string)");

  os = grl_lua_library_get_current_operation (L);
  g_return_val_if_fail (os != NULL, 0);
  option = lua_tostring (L, 1);

  if (g_strcmp0 (option, "type") == 0) {
    const char *type;
    switch (os->op_type) {
    case LUA_SEARCH:
      type = "search";
      break;
    case LUA_BROWSE:
      type = "browse";
      break;
    case LUA_QUERY:
      type = "query";
      break;
    case LUA_RESOLVE:
      type = "resolve";
      break;
    default:
      g_assert_not_reached ();
    }

    lua_pushstring (L, type);
    return 1;
  }

  if (g_strcmp0 (option, "count") == 0) {
    gint count = grl_operation_options_get_count (os->options);

    lua_pushnumber (L, count);
    return 1;
  }

  if (g_strcmp0 (option, "skip") == 0) {
    guint skip = grl_operation_options_get_skip (os->options);

    lua_pushnumber (L, skip);
    return 1;
  }

  if (g_strcmp0 (option, "flags") == 0) {
    GrlResolutionFlags flags = grl_operation_options_get_resolution_flags (os->options);

    lua_pushnumber (L, (gint) flags);
    return 1;
  }

  if (g_strcmp0 (option, "key-filter") == 0) {
    GrlKeyID key;
    GValue *value = NULL;
    const gchar *key_name = NULL;
    GrlRegistry *registry = grl_registry_get_default ();

    luaL_argcheck (L, lua_isstring (L, 2), 2, "expecting key name");
    key_name = lua_tostring (L, 2);

    key = grl_registry_lookup_metadata_key (registry, key_name);
    value = grl_operation_options_get_key_filter (os->options, key);
    switch (grl_registry_lookup_metadata_key_type (registry, key)) {
    case G_TYPE_INT:
      (value) ? (void) lua_pushnumber (L, g_value_get_int (value)) : lua_pushnil (L);
      break;

    case G_TYPE_FLOAT:
      (value) ? (void) lua_pushnumber (L, g_value_get_float (value)) : lua_pushnil (L);
      break;

    case G_TYPE_STRING:
      (value) ? (void) lua_pushstring (L, g_value_get_string (value)) : lua_pushnil (L);
      break;

    case G_TYPE_BOOLEAN:
      (value) ? (void) lua_pushboolean (L, g_value_get_boolean (value)) : lua_pushnil (L);
      break;

    default:
      GRL_DEBUG ("'%s' is being ignored as G_TYPE is not being handled.",
                 key_name);
    }
    return 1;
  }

  if (g_strcmp0 (option, "range-filter") == 0) {
    GValue *min = NULL;
    GValue *max = NULL;
    GrlKeyID key;
    const gchar *key_name = NULL;
    GrlRegistry *registry = grl_registry_get_default ();

    luaL_argcheck (L, lua_isstring (L, 3), 3, "expecting key name");
    key_name = lua_tostring (L, 3);

    key = grl_registry_lookup_metadata_key (registry, key_name);
    if (key != GRL_METADATA_KEY_INVALID) {
      grl_operation_options_get_key_range_filter (os->options, key, &min, &max);
      switch (grl_registry_lookup_metadata_key_type (registry, key)) {
      case G_TYPE_INT:
        (min) ? (void) lua_pushnumber (L, g_value_get_int (min)) : lua_pushnil (L);
        (max) ? (void) lua_pushnumber (L, g_value_get_int (max)) : lua_pushnil (L);
        break;

      case G_TYPE_FLOAT:
        (min) ? (void) lua_pushnumber (L, g_value_get_float (min)) : lua_pushnil (L);
        (max) ? (void) lua_pushnumber (L, g_value_get_float (max)) : lua_pushnil (L);
        break;

      case G_TYPE_STRING:
        (min) ? (void) lua_pushstring (L, g_value_get_string (min)) : lua_pushnil (L);
        (max) ? (void) lua_pushstring (L, g_value_get_string (max)) : lua_pushnil (L);
        break;

      default:
        GRL_DEBUG ("'%s' is being ignored as G_TYPE is not being handled.",
                   key_name);
      }
    }
    return 2;
  }

  if (g_strcmp0 (option, "operation-id") == 0) {
    lua_pushnumber (L, (gint) os->operation_id);
    return 1;
  }

  if (g_strcmp0 (option, "media-id") == 0 &&
      os->op_type == LUA_BROWSE) {
    lua_pushstring (L, os->string);
    return 1;
  }

  if (g_strcmp0 (option, "query") == 0 &&
      os->op_type == LUA_QUERY) {
    lua_pushstring (L, os->string);
    return 1;
  }

  if (g_strcmp0 (option, "search") == 0 &&
      os->op_type == LUA_SEARCH) {
    lua_pushstring (L, os->string);
    return 1;
  }

  luaL_error (L, "'%s' is not available nor implemented.", option);
  return 0;
}

/**
* grl.get_media_keys
*
* @return: array of all requested keys from application (may be empty);
*/
static gint
grl_l_operation_get_keys (lua_State *L)
{
  OperationSpec *os;
  GrlRegistry *registry;
  GList *it;
  gint i = 0;

  os = grl_lua_library_get_current_operation (L);
  g_return_val_if_fail (os != NULL, 0);

  registry = grl_registry_get_default ();
  lua_newtable (L);
  for (it = os->keys; it; it = g_list_next (it)) {
    GrlKeyID key_id;
    const gchar *key_name;

    key_id = GRLPOINTER_TO_KEYID (it->data);
    key_name = grl_registry_lookup_metadata_key_name (registry, key_id);
    if (key_id != GRL_METADATA_KEY_INVALID) {
      lua_pushinteger (L, i + 1);
      lua_pushstring (L, key_name);
      lua_settable (L, -3);
      i++;
    }
  }
  return 1;
}

/**
* grl.get_media_keys
*
* @return: table with all keys/values of media (may be empty);
*/
static gint
grl_l_media_get_keys (lua_State *L)
{
  OperationSpec *os;
  GrlRegistry *registry;
  GList *it;
  GList *list_keys;

  os = grl_lua_library_get_current_operation (L);
  g_return_val_if_fail (os != NULL, 0);

  registry = grl_registry_get_default ();
  lua_newtable (L);
  list_keys = grl_data_get_keys (GRL_DATA (os->media));
  for (it = list_keys; it; it = g_list_next (it)) {
    GrlKeyID key_id;
    gchar *key_name;
    gchar *ptr = NULL;
    GType type = G_TYPE_NONE;

    key_id = GRLPOINTER_TO_KEYID (it->data);
    key_name = g_strdup (grl_registry_lookup_metadata_key_name (registry,
                                                                key_id));
    key_id = grl_registry_lookup_metadata_key (registry, key_name);

    /* Replace '-' to '_': as a convenience for the developer */
    while ((ptr = strstr (key_name, "-")) != NULL) {
      *ptr = '_';
    }

    lua_pushstring (L, key_name);
    if (key_id != GRL_METADATA_KEY_INVALID) {
      type = grl_registry_lookup_metadata_key_type (registry, key_id);
      switch (type) {
      case G_TYPE_INT:
        lua_pushnumber (L, grl_data_get_int (GRL_DATA (os->media), key_id));
        break;
      case G_TYPE_FLOAT:
        lua_pushnumber (L, grl_data_get_float (GRL_DATA (os->media), key_id));
        break;
      case G_TYPE_STRING:
        lua_pushstring (L, grl_data_get_string (GRL_DATA (os->media), key_id));
        break;
      case G_TYPE_INT64:
        lua_pushnumber (L, grl_data_get_int64 (GRL_DATA (os->media), key_id));
        break;
      case G_TYPE_BOOLEAN:
        lua_pushboolean (L, grl_data_get_boolean (GRL_DATA (os->media), key_id));
        break;
      default:
        if (type == G_TYPE_DATE_TIME) {
          GDateTime *date = grl_data_get_boxed (GRL_DATA (os->media), key_id);
          gchar *date_str = g_date_time_format (date, "%F %T");
          lua_pushstring (L, date_str);
          g_free(date_str);

        } else {
          GRL_DEBUG ("'%s' is being ignored as G_TYPE is not being handled.",
                     key_name);
          g_free (key_name);
          lua_pop (L, 1);
          continue;
        }
      }
      lua_settable (L, -3);
    }
    g_free (key_name);
  }
  g_list_free (list_keys);
  return 1;
}

/**
* grl.fetch
*
* @url: (string or array) The http url to GET the content.
* @callback: (string) The function to be called after fetch is complete.
* @netopts: (table) Options to set the GrlNetWc object.
* @return: Nothing.;
*/
static gint
grl_l_fetch (lua_State *L)
{
  guint i;
  guint num_urls;
  gchar **urls;
  gchar **results;
  const gchar *lua_callback;
  GrlNetWc *wc;
  gboolean is_table = FALSE;
  OperationSpec *os;

  luaL_argcheck (L, (lua_isstring (L, 1) || lua_istable (L, 1)), 1,
                 "expecting url as string or an array of urls");
  luaL_argcheck (L, lua_isstring (L, 2), 2,
                 "expecting callback function as string");

  os = grl_lua_library_get_current_operation (L);
  g_return_val_if_fail (os != NULL, 0);
  os->pending_ops++;

  num_urls = (lua_isstring (L, 1)) ? 1 : luaL_len (L, 1);
  urls = g_new0 (gchar *, num_urls);

  if (lua_isstring (L, 1)) {
    *urls = (gchar *) lua_tolstring (L, 1, NULL);
    GRL_DEBUG ("grl.fetch() -> '%s'", *urls);
  } else {
    is_table = TRUE;
    for (i = 0; i < num_urls; i++) {
      lua_pushinteger (L, i + 1);
      lua_gettable (L, 1);
      if (lua_isstring (L, -1) && !lua_isnumber (L, -1)) {
        urls[i] = (gchar *) lua_tostring (L, -1);
      } else {
        luaL_error (L, "Array of urls expect strings only: at index %d is %s",
                    i + 1, luaL_typename (L, -1));
      }
      GRL_DEBUG ("grl.fetch() -> urls[%d]: '%s'", i, urls[i]);
      lua_pop (L, 1);
    }
  }

  if (!verify_plaintext_fetch (os->source, urls, num_urls)) {
    GRL_WARNING ("Source '%s' is broken, it makes plaintext network queries but "
                 "does not set the 'net:plaintext' tag", grl_source_get_id (os->source));
    g_free (urls);
    os->pending_ops--;
    return 1;
  }

  lua_callback = lua_tolstring (L, 2, NULL);

  wc = net_wc_new_with_options(L, 3);

  /* shared data between urls */
  results = g_new0 (gchar *, num_urls);
  for (i = 0; i < num_urls; i++) {
    FetchOperation *fo;

    fo = g_new0 (FetchOperation, 1);
    fo->L = L;
    fo->operation_id = os->operation_id;
    fo->lua_cb = g_strdup (lua_callback);
    fo->index = i;
    fo->url = g_strdup (urls[i]);
    fo->num_urls = num_urls;
    fo->is_table = is_table;
    fo->results = results;

    grl_net_wc_request_async (wc, urls[i], NULL, grl_util_fetch_done, fo);
  }
  g_object_unref (wc);
  g_free (urls);
  return 1;
}

/**
* grl.callback
*
* @media: (table) The media content to be returned.
* @count: (number) Number of media remaining to the application.
* @return: Nothing;
*/
static gint
grl_l_callback (lua_State *L)
{
  gint nparam;
  gint count = 0;
  OperationSpec *os;
  GrlMedia *media;

  GRL_DEBUG ("grl.callback()");

  nparam = lua_gettop (L);
  os = grl_lua_library_get_current_operation (L);
  if (os == NULL) {
    luaL_error (L, "Source is broken as grl.callback() was called "
                "after last operation has been finalized");
    return 0;
  }

  media = (os->op_type == LUA_RESOLVE) ? os->media : NULL;
  if (nparam > 0) {
    media = grl_util_build_media (L, media);
    count = (lua_isnumber (L, 2)) ? lua_tonumber (L, 2) : 0;
  }

  switch (os->op_type) {
  case LUA_RESOLVE:
    os->cb.resolve (os->source, os->operation_id, media, os->user_data, NULL);
    break;

  default:
    os->cb.result (os->source, os->operation_id, media,
                   count, os->user_data, NULL);
  }

  /* Free Operation Spec */
  if (count == 0) {
    g_list_free (os->keys);
    g_object_unref (os->options);
    os->callback_done = TRUE;
    grl_lua_library_remove_operation_data (L, os->operation_id);
    grl_lua_library_set_current_operation (L, 0);
    g_free (os->string);
    g_slice_free (OperationSpec, os);
  }

  return 0;
}

/**
 * grl.debug
 *
 * @str: (string) the debug output to generate
 */
static gint
grl_l_debug (lua_State *L)
{
  const gchar *str;

  luaL_argcheck (L, lua_isstring (L, 1), 1, "expecting debug output as string");

  str = lua_tolstring (L, 1, NULL);
  GRL_DEBUG ("%s", str);

  return 0;
}

/**
 * grl.warning
 *
 * @str: (string) the debug output to generate
 */
static gint
grl_l_warning (lua_State *L)
{
  const gchar *str;

  luaL_argcheck (L, lua_isstring (L, 1), 1, "expecting warning output as string");

  str = lua_tolstring (L, 1, NULL);
  GRL_WARNING ("%s", str);

  return 0;
}

/**
 * grl.dgettext
 *
 * @domain: (string) the domain to use for the translation
 * @str: (string) the string to translate
 * @return: the translated string
 */
static gint
grl_l_dgettext (lua_State *L)
{
  const gchar *domain;
  const gchar *str;
  gchar *output;

  luaL_argcheck (L, lua_isstring (L, 1), 1, "expecting domain name as string");
  luaL_argcheck (L, lua_isstring (L, 2), 2, "expecting string to translate as string");

  domain = lua_tolstring (L, 1, NULL);
  str = lua_tolstring (L, 2, NULL);

  bind_textdomain_codeset (domain, "UTF-8");
  output = dgettext (domain, str);
  lua_pushstring (L, output);

  return 1;
}

/**
 * grl.encode
 *
 * @text: (string) the string to %-encode
 * @return: the %-encoded version of @text
 */
static gint
grl_l_encode (lua_State *L)
{
  const gchar *text;
  gchar *output;

  luaL_argcheck (L, lua_isstring (L, 1), 1, "expecting part as string");

  text = lua_tolstring (L, 1, NULL);

  output = g_uri_escape_string (text, NULL, FALSE);
  lua_pushstring (L, output);
  g_free (output);

  return 1;
}

/**
 * grl.decode
 *
 * @part: (string) the %-encoded part string to decode
 * @return: the decoded string
 */
static gint
grl_l_decode (lua_State *L)
{
  const gchar *part;
  gchar *output;

  luaL_argcheck (L, lua_isstring (L, 1), 1, "expecting part as string");

  part = lua_tolstring (L, 1, NULL);

  output = g_uri_unescape_string (part, NULL);
  lua_pushstring (L, output);
  g_free (output);

  return 1;
}

/**
 * grl.unescape
 *
 * @html: (string) the HTML string to unescape
 * @return: the unescaped string
 */
static gint
grl_l_unescape (lua_State *L)
{
  const gchar *html;
  gchar *output;

  luaL_argcheck (L, lua_isstring (L, 1), 1, "expecting html as string");

  html = lua_tolstring (L, 1, NULL);

  output = unescape_string (html);
  lua_pushstring (L, output);
  g_free (output);

  return 1;
}

/**
 * grl.unzip
 *
 * @url: (string) the URL of the zip file to fetch
 * @filenames: (table) a table of filenames to get inside the zip file
 * @callback: (string) The function to be called after fetch is complete.
 * @netopts: (table) Options to set the GrlNetWc object.
 * @return: Nothing.;
 */
static gint
grl_l_unzip (lua_State *L)
{
  const gchar *lua_callback;
  const gchar *url;
  GrlNetWc *wc;
  OperationSpec *os;
  UnzipOperation *uo;
  guint num_filenames, i;
  gchar **filenames;

  luaL_argcheck (L, lua_isstring (L, 1), 1,
                 "expecting url as string");
  luaL_argcheck (L, lua_istable (L, 2), 2,
                 "expecting filenames as an array of filenames");
  luaL_argcheck (L, lua_isstring (L, 3), 3,
                 "expecting callback function as string");

  os = grl_lua_library_get_current_operation (L);
  g_return_val_if_fail (os != NULL, 0);
  os->pending_ops++;

  url = lua_tolstring (L, 1, NULL);
  num_filenames = luaL_len(L, 2);
  filenames = g_new0 (gchar *, num_filenames + 1);
  for (i = 0; i < num_filenames; i++) {
    lua_pushinteger (L, i + 1);
    lua_gettable (L, 2);
    if (lua_isstring (L, -1)) {
      filenames[i] = g_strdup (lua_tostring (L, -1));
    } else {
      luaL_error (L, "Array of urls expect strings only: at index %d is %s",
                  i + 1, luaL_typename (L, -1));
    }
    GRL_DEBUG ("grl.unzip() -> filenames[%d]: '%s'", i, filenames[i]);
    lua_pop (L, 1);
  }
  GRL_DEBUG ("grl.unzip() -> '%s'", url);

  lua_callback = lua_tolstring (L, 3, NULL);
  wc = net_wc_new_with_options (L, 4);

  uo = g_new0 (UnzipOperation, 1);
  uo->L = L;
  uo->operation_id = os->operation_id;
  uo->lua_cb = g_strdup (lua_callback);
  uo->url = g_strdup (url);
  uo->filenames = filenames;

  grl_net_wc_request_async (wc, url, NULL, grl_util_unzip_done, uo);
  g_object_unref (wc);
  return 1;
}

/**
 * grl.goa_consumer_key
 *
 * Only available for gnome-online-accounts sources.
 *
 * @return: The consumer key for the configured account or
 * nil if gnome-online-accounts isn't available.
 */
static gint
grl_l_goa_consumer_key (lua_State *L)
{
#ifndef GOA_ENABLED
  GRL_WARNING ("Source '%s' is broken as it tries to access gnome-online-accounts "
               "information, but it should not have been created");
  return 0;
#else
  {
    GoaObject *object;
    GoaOAuth2Based *oauth2 = NULL;

    object = grl_lua_library_load_goa_data (L);
    if (object != NULL) {
      /* FIXME handle other types of object? */
      oauth2 = goa_object_get_oauth2_based (object);
    }
    if (oauth2 == NULL) {
      GRL_WARNING ("Source is broken as it tries to access gnome-online-accounts "
                   "information, but it doesn't declare what account data it needs, or"
                   "the account type is not supported.");
      lua_pushnil (L);
      return 1;
    } else {
      lua_pushstring (L, goa_oauth2_based_get_client_id (GOA_OAUTH2_BASED (oauth2)));
      return 1;
    }
  }
#endif
}

/**
 * grl.goa_access_token
 *
 * Only available for gnome-online-accounts sources.
 *
 * @return: The access token for the configured account or
 * nil if gnome-online-accounts isn't available.
 */
static gint
grl_l_goa_access_token (lua_State *L)
{
#ifndef GOA_ENABLED
  GRL_WARNING ("Source '%s' is broken as it tries to access gnome-online-accounts "
               "information, but it should not have been created",
               grl_source_get_id (os->source));
  return 0;
#else
  {
    GoaObject *object;
    GoaOAuth2Based *oauth2 = NULL;

    object = grl_lua_library_load_goa_data (L);

    if (object != NULL) {
      /* FIXME handle other types of object? */
      oauth2 = goa_object_get_oauth2_based (object);
    }
    if (oauth2 == NULL) {
      GRL_WARNING ("Source is broken as it tries to access gnome-online-accounts "
                   "information, but it doesn't declare what account data it needs, or "
                   "the account type is not supported.");
      lua_pushnil (L);
      return 1;
    } else {
      gchar *access_token;

      goa_oauth2_based_call_get_access_token_sync (oauth2,
                                                   &access_token,
                                                   NULL, NULL, NULL);
      lua_pushstring (L, access_token);
      g_free (access_token);
      return 1;
    }
  }
#endif
}

static gint
grl_l_is_video_site (lua_State *L)
{
  const char *url;
  gboolean ret = FALSE;

  luaL_argcheck (L, lua_isstring (L, 1), 1,
                 "expecting url as string");

  url = lua_tolstring (L, 1, NULL);

#ifdef HAVE_TOTEM_PLPARSER_MINI
  ret = totem_pl_parser_can_parse_from_uri (url, FALSE);
#else
  GRL_DEBUG ("Return FALSE for whether '%s' is a video site, as compiled without totem-plparser-mini support", url);
#endif

  lua_pushboolean (L, ret);

  return 1;
}

/* ================== Lua-Library initialization =========================== */

/** Load library included as GResource and run it with lua_pcall.
  * Caller should handle the stack aftwards.
 **/
static gboolean
load_gresource_library (lua_State   *L,
                        const gchar *uri)
{
  GFile *file;
  gchar *data;
  gsize size;
  GError *error = NULL;
  gboolean ret = TRUE;

  file = g_file_new_for_uri (uri);
  g_file_load_contents (file, NULL, &data, &size, NULL, &error);
  g_assert_no_error (error);
  g_clear_pointer (&file, g_object_unref);

  if (luaL_dostring (L, data)) {
    GRL_WARNING ("Failed to load %s due %s", uri, lua_tostring (L, -1));
    ret = FALSE;
  }
  g_free (data);
  return ret;
}

gint
luaopen_grilo (lua_State *L)
{
  static const luaL_Reg library_fn[] = {
    {"get_options", &grl_l_operation_get_options},
    {"get_requested_keys", &grl_l_operation_get_keys},
    {"get_media_keys", &grl_l_media_get_keys},
    {"callback", &grl_l_callback},
    {"fetch", &grl_l_fetch},
    {"debug", &grl_l_debug},
    {"warning", &grl_l_warning},
    {"dgettext", &grl_l_dgettext},
    {"encode", &grl_l_encode},
    {"decode", &grl_l_decode},
    {"unescape", &grl_l_unescape},
    {"unzip", &grl_l_unzip},
    {"goa_access_token", &grl_l_goa_access_token},
    {"goa_consumer_key", &grl_l_goa_consumer_key},
    {"is_video_site", &grl_l_is_video_site},
    {NULL, NULL}
  };

  GRL_LOG_DOMAIN_INIT (lua_library_log_domain, "lua-library");

  GRL_DEBUG ("Loading grilo lua-library");
  luaL_newlib (L, library_fn);

  /* The following modules are restrict to Lua sources */
  lua_pushstring (L, LUA_MODULES_NAME);
  lua_newtable (L);

  lua_pushstring (L, GRILO_LUA_LIBRARY_JSON);
  luaopen_json (L);
  lua_settable (L, -3);

  /* Load inspect.lua and save object in global environment table */
  lua_getglobal (L, LUA_ENV_TABLE);
  if (load_gresource_library (L, URI_LUA_LIBRARY_INSPECT) &&
      lua_istable (L, -1)) {
      /* Top of the stack is inspect table from inspect.lua */
      lua_getfield (L, -1, "inspect");
      /* Top of the stack is inspect.inspect */
      lua_setfield (L, -4, GRILO_LUA_LIBRARY_INSPECT);
      /* grl.lua.inspect points to inspect.inspect */

      /* Save inspect table in LUA_ENV_TABLE */
      lua_setfield (L, -2, GRILO_LUA_INSPECT_INDEX);
  } else {
      GRL_WARNING ("Failed to load inspect.lua");
  }
  lua_pop (L, 1);

  /* Those modules are called in 'lua' table, inside 'grl' */
  lua_settable (L, -3);
  return 1;
}

/* ======= Lua-Library and Lua-Factory utilities ============= */

/**
 * grl_lua_library_save_operation_data
 *
 * @L : LuaState where the data will be stored.
 * @os: The Operation Data to store.
 * @return: Nothing.
 *
 * Stores the OperationSpec from Lua-Factory in the global environment of
 * lua_State.
 **/
void
grl_lua_library_save_operation_data (lua_State *L, OperationSpec *os)
{
  char *op_id;

  g_return_if_fail (os != NULL);

  op_id = g_strdup_printf (GRILO_LUA_OPERATION_INDEX "-%i", os->operation_id);
  lua_getglobal (L, LUA_ENV_TABLE);
  lua_pushstring (L, op_id);
  lua_pushlightuserdata (L, os);
  lua_settable (L, -3);
  lua_pop (L, 1);
  g_free (op_id);
}

/**
 * grl_lua_library_remove_operation_data
 *
 * @L: LuaState where the data will be removed.
 * @operation_id: The operation ID to remove.
 * @return: Nothing.
 *
 * Remove the OperationSpec with this ID from the the global environment of
 * lua_State.
 **/
void
grl_lua_library_remove_operation_data (lua_State *L, guint operation_id)
{
  char *op_id;

  op_id = g_strdup_printf (GRILO_LUA_OPERATION_INDEX "-%i", operation_id);
  lua_getglobal (L, LUA_ENV_TABLE);
  lua_pushstring (L, op_id);
  lua_pushlightuserdata (L, NULL);
  lua_settable (L, -3);
  lua_pop (L, 1);
  g_free (op_id);
}

/**
 * grl_lua_library_load_operation_data
 *
 * @L : LuaState where the data is stored.
 * @operation_id: The operation ID to load Operation Data for.
 * to load the Operation Data for the current call.
 * @return: The Operation Data.
 **/
OperationSpec *
grl_lua_library_load_operation_data (lua_State *L, guint operation_id)
{
  OperationSpec *os = NULL;
  char *op_id;

  g_return_val_if_fail (operation_id > 0, NULL);

  op_id = g_strdup_printf (GRILO_LUA_OPERATION_INDEX "-%i", operation_id);
  lua_getglobal (L, LUA_ENV_TABLE);
  lua_pushstring (L, op_id);
  lua_gettable (L, -2);
  os = (lua_islightuserdata(L, -1)) ? lua_touserdata(L, -1) : NULL;
  lua_pop(L, 1);
  g_free (op_id);

  return os;
}

/**
 * grl_lua_library_set_current_operation
 *
 * @L: LuaState where the data is stored.
 * @operation_id: The current operation ID.
 * @return: Nothing:
 **/
void
grl_lua_library_set_current_operation (lua_State *L, guint operation_id)
{
  OperationSpec *os;

  /* Verify that either grl.callback was called, or that there
   * are pending operations */
  os = grl_lua_library_get_current_operation (L);
  if (os) {
    if (os->pending_ops == 0 && !os->callback_done) {
      GRL_WARNING ("Source '%s' is broken, as there are no pending operations "
                   "and grl.callback() was not called", grl_source_get_id (os->source));
      switch (os->op_type) {
      case LUA_RESOLVE:
        os->cb.resolve (os->source, os->operation_id, NULL, os->user_data, NULL);
        break;

      default:
        os->cb.result (os->source, os->operation_id, NULL,
                       0, os->user_data, NULL);
      }
    }
  }

  if (operation_id > 0)
    os = grl_lua_library_load_operation_data (L, operation_id);
  else
    os = NULL;

  lua_getglobal (L, LUA_ENV_TABLE);
  lua_pushstring (L, GRILO_LUA_OPERATION_INDEX);
  lua_pushlightuserdata (L, os);
  lua_settable (L, -3);
  lua_pop (L, 1);
}

/**
 * grl_lua_library_get_current_operation
 *
 * @L: LuaState where the data is stored.
 * @return: The Operation Data for the current operation.
 **/
OperationSpec *
grl_lua_library_get_current_operation (lua_State *L)
{
  OperationSpec *os = NULL;

  lua_getglobal (L, LUA_ENV_TABLE);
  lua_pushstring (L, GRILO_LUA_OPERATION_INDEX);
  lua_gettable (L, -2);
  os = (lua_islightuserdata(L, -1)) ? lua_touserdata(L, -1) : NULL;
  lua_pop(L, 1);

  return os;
}

/**
 * grl_lua_library_save_goa_data
 *
 * @L: LuaState where the data will be stored.
 * @goa_object: #GoaObject to store.
 * @return: Nothing.
 *
 * Stores the GoaObject from Lua-Factory in the global environment of
 * lua_State.
 **/
void
grl_lua_library_save_goa_data (lua_State *L, gpointer goa_object)
{
  g_return_if_fail (goa_object != NULL);

#ifdef GOA_ENABLED
  lua_pushlightuserdata (L, goa_object);
  lua_setglobal (L, GOA_LUA_NAME);
#else
  GRL_WARNING ("grl_lua_library_save_goa_data() called but GOA support disabled.");
#endif /* GOA_ENABLED */
}

/**
 * grl_lua_library_load_goa_data
 *
 * @L: LuaState where the data is stored.
 * @return: The #GoaObject.
 **/
gpointer
grl_lua_library_load_goa_data (lua_State *L)
{
#ifdef GOA_ENABLED
  GoaObject *goa_object;

  lua_getglobal (L, GOA_LUA_NAME);
  goa_object = (lua_islightuserdata(L, -1)) ? lua_touserdata(L, -1) : NULL;
  lua_pop(L, 1);

  return goa_object;
#else
  GRL_WARNING ("grl_lua_library_load_goa_data() called but GOA support disabled.");
  return NULL;
#endif /* GOA_ENABLED */
}
