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

#include "lua-library/lua-proxy-call.h"

#ifdef HAVE_TOTEM_PLPARSER_MINI
#include <totem-pl-parser-mini.h>
#endif

#define LUA_SOURCE_TABLE            "source"
#define LUA_SOURCE_TAGS             "tags"

#define GRL_LOG_DOMAIN_DEFAULT lua_library_log_domain
GRL_LOG_DOMAIN_STATIC (lua_library_log_domain);

typedef struct {
  lua_State *L;
  gint lua_userdata;
  gint lua_callback;
  guint index;
  gchar *url;
  guint num_urls;
  gboolean is_table;
  gchar **results;
  GCancellable *cancellable;
  OperationSpec *os;
} FetchOperation;

typedef struct {
  lua_State *L;
  int lua_userdata;
  int lua_callback;
  GCancellable *cancellable;
  OperationSpec *os;
} RequestOperation;

typedef struct {
  lua_State *L;
  gint lua_userdata;
  gint lua_callback;
  gchar *url;
  gchar **filenames;
  GCancellable *cancellable;
  OperationSpec *os;
} UnzipOperation;

#ifdef GOA_ENABLED
static gpointer grl_lua_library_load_goa_data (lua_State *L);
#endif

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
verify_plaintext_fetch (lua_State  *L,
                        gchar     **urls,
                        guint       num_urls)
{
  guint i;

  lua_getglobal (L, LUA_SOURCE_TABLE);
  if (!lua_istable (L, -1)) {
    lua_pop (L, 1);
    return FALSE;
  }
  lua_getfield (L, -1, LUA_SOURCE_TAGS);
  if (!lua_istable (L, -1)) {
    lua_pop (L, 2);
    return FALSE;
  }

  lua_pushnil (L);
  while (lua_next (L, -2) != 0) {
    if (g_strcmp0 (lua_tostring (L, -1), "net:plaintext") == 0) {
      /* No need to verify the URLs, the source is saying that they do
       * plaintext queries, so nothing for us to block */
      lua_pop (L, 4);
      return TRUE;
    }
    lua_pop (L, 1);
  }

  lua_pop (L, 2);

  for (i = 0; i < num_urls; i++) {
    if (g_str_has_prefix (urls[i], "http:"))
      return FALSE;
  }

  return TRUE;
}

static gboolean
grl_util_init_value (lua_State *L,
                     GValue *value,
                     const gchar *key_name)
{
  if (lua_isinteger (L, -1)) {
    g_value_init (value, G_TYPE_INT64);
    g_value_set_int64 (value, lua_tointeger (L, -1));
  } else if (lua_isstring (L, -1)) {
    g_value_init (value, G_TYPE_STRING);
    g_value_set_string (value, lua_tostring (L, -1));
  } else if (lua_isnumber (L, -1)) {
    g_value_init (value, G_TYPE_FLOAT);
    g_value_set_float (value, lua_tonumber (L, -1));
  } else if (lua_isboolean (L, -1)) {
    g_value_init (value, G_TYPE_BOOLEAN);
    g_value_set_boolean (value, lua_toboolean (L, -1));
  } else {
    GDateTime *date;
    const char *date_str = lua_tostring (L, -1);
    date = grl_date_time_from_iso8601 (date_str);
    /* Try a number of seconds since Epoch */
    if (date == NULL) {
      gint64 date_int = g_ascii_strtoll (date_str, NULL, 0);
      if (date_int != 0) {
        date = g_date_time_new_from_unix_utc (date_int);
      }
    }
    if (date != NULL) {
      g_value_init (value, G_TYPE_DATE_TIME);
      g_value_set_boxed (value, date);
      g_date_time_unref (date);
    } else {
      GRL_WARNING ("'%s' is being ignored as '%s' is not being handled.",
                   key_name, lua_typename (L, -1));
      return FALSE;
    }
  }
  return TRUE;
}

/* Top of the stack must be a table */
static void
grl_util_add_table_to_unregistered_media (lua_State *L,
                                          GrlMedia *media,
                                          const gchar *key_name)
{
  GValue value = { 0, };
  gint i;
  gint array_len = luaL_len (L, -1);
  GRL_DEBUG ("Adding %u elements under %s metadata-key", array_len, key_name);

  /* Insert new values */
  for (i = 0; i < array_len; i++) {
    lua_pushinteger (L, i + 1);
    lua_gettable (L, -2);
    grl_util_init_value (L, &value, key_name);

    if (!grl_data_add_for_id (GRL_DATA (media), key_name, &value)) {
      GRL_DEBUG("Failed to add element at index %d to %s", i+1, key_name);
    }

    g_value_unset (&value);
    lua_pop (L, 1);
  }
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
    case G_TYPE_INT64:
      if (lua_isnumber (L, -1)) {
        gint success;
        gint64 value = lua_tointegerx (L, -1, &success);
        if (success) {
          if (type == G_TYPE_INT)
            grl_data_add_int (GRL_DATA (media), key_id, value);
          else
            grl_data_add_int64 (GRL_DATA (media), key_id, value);
        }
      }
      break;

    case G_TYPE_FLOAT:
      if (lua_isnumber (L, -1))
        grl_data_add_float (GRL_DATA (media), key_id, lua_tonumber (L, -1));
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

static void
grl_util_add_key(lua_State *L,
		 GObject *object,
		 gboolean is_media)
{
  GrlRegistry *registry = grl_registry_get_default ();
  GrlKeyID key_id = GRL_METADATA_KEY_INVALID;
  gchar *key_name = g_strdup (lua_tostring (L, -2));
  gchar *ptr = NULL;
  GType type = G_TYPE_NONE;
  GValue value = { 0, };

  /* Handled above */
  if (g_strcmp0 (key_name, "type") == 0) {
    g_free (key_name);
    return;
  }

  /* Replace '_' to '-': convenient for the developer */
  while ((ptr = strstr (key_name, "_")) != NULL) {
    *ptr = '-';
  }

  key_id = grl_registry_lookup_metadata_key (registry, key_name);
  if (key_id == GRL_METADATA_KEY_INVALID) {
    GRL_DEBUG ("'%s' isn't a registered key.", key_name);
    if (lua_istable (L, -1)){
      grl_util_add_table_to_unregistered_media (L, GRL_MEDIA (object), key_name);
    } else {
      gboolean success = grl_util_init_value (L, &value, key_name);
      if (success) {
        if (is_media)
          success = grl_data_set_for_id (GRL_DATA (object), key_name, &value);
        else if (GRL_IS_RELATED_KEYS (object))
          success = grl_related_keys_set_for_id (GRL_RELATED_KEYS (object), key_name, &value);
	g_value_unset (&value);
      }
      if (!success)
	GRL_DEBUG ("Value for %s was not set", key_name);
    }
    g_free (key_name);
    return;
  }

  type = grl_registry_lookup_metadata_key_type (registry, key_id);

  switch (type) {
  case G_TYPE_INT:
  case G_TYPE_INT64:
    if (lua_isnumber (L, -1)) {
      gint success;
      gint64 value = lua_tointegerx (L, -1, &success);
      if (success) {
	if (type == G_TYPE_INT) {
	  if (is_media) {
	    grl_data_set_int (GRL_DATA (object), key_id, value);
	  } else {
	    grl_related_keys_set_int (GRL_RELATED_KEYS (object), key_id, value);
	  }
	} else {
	  if (is_media) {
	    grl_data_set_int64 (GRL_DATA (object), key_id, value);
	  } else {
	    grl_related_keys_set_int64 (GRL_RELATED_KEYS (object), key_id, value);
	  }
	}
      } else {
	GRL_WARNING ("'%s' requires an INT type, while a value '%s' was provided",
		     key_name, lua_tostring(L, -1));
      }
    } else if (lua_istable (L, -1)) {
      if (is_media) {
	grl_util_add_table_to_media (L, GRL_MEDIA (object), key_id, key_name, type);
      } else {
	GRL_WARNING ("GrlRelatedKeys does not support tables");
      }
    } else if (!lua_isnil (L, -1)) {
      GRL_WARNING ("'%s' is not compatible for '%s'",
		   lua_typename (L, lua_type(L, -1)), key_name);
    }
    break;

  case G_TYPE_FLOAT:
    if (lua_isnumber (L, -1)) {
      if (is_media) {
	grl_data_set_float (GRL_DATA (object), key_id, lua_tonumber (L, -1));
      } else {
	grl_related_keys_set_float ( GRL_RELATED_KEYS (object), key_id, lua_tonumber (L, -1));
      }
    } else if (lua_istable (L, -1)) {
      if (is_media) {
	grl_util_add_table_to_media (L, GRL_MEDIA (object), key_id, key_name, type);
      } else {
	GRL_WARNING ("GrlRelatedKeys does not support tables");
      }
    } else if (!lua_isnil (L, -1)) {
      GRL_WARNING ("'%s' is not compatible for '%s'",
		   lua_typename (L, lua_type(L, -1)), key_name);
    }
    break;

  case G_TYPE_STRING:
    if (lua_isstring (L, -1)) {
      if (is_media) {
	grl_data_set_lua_string (GRL_DATA (object), key_id, key_name, lua_tostring (L, -1));
      } else {
	grl_related_keys_set_string (GRL_RELATED_KEYS (object), key_id, lua_tostring (L, -1));
      }
    } else if (lua_istable (L, -1)) {
      if (is_media) {
	grl_util_add_table_to_media (L, GRL_MEDIA (object), key_id, key_name, type);
      } else {
	GRL_WARNING ("GrlRelatedKeys does not support tables");
      }
    } else if (!lua_isnil (L, -1)) {
      GRL_WARNING ("'%s' is not compatible for '%s'",
		   lua_typename (L, lua_type(L, -1)), key_name);
    }
    break;

  case G_TYPE_BOOLEAN:
    if (lua_isboolean (L, -1)) {
      if (is_media) {
	grl_data_set_boolean (GRL_DATA (object), key_id, lua_toboolean (L, -1));
      } else {
	grl_related_keys_set_boolean (GRL_RELATED_KEYS (object), key_id, lua_toboolean (L, -1));
      }
    } else if (!lua_isnil (L, -1)) {
      GRL_WARNING ("'%s' is not compatible for '%s'",
		   lua_typename (L, lua_type(L, -1)), key_name);
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
	if (is_media) {
	  grl_data_set_boxed (GRL_DATA (object), key_id, date);
	} else {
	  grl_related_keys_set_boxed (GRL_RELATED_KEYS (object), key_id, date);
	}
	g_date_time_unref (date);
      } else {
	GRL_WARNING ("'%s' is not a valid ISO-8601 or Epoch date", date_str);
      }
    } else if (type == G_TYPE_BYTE_ARRAY) {
      gsize size = luaL_len (L, -1);
      const guint8 *binary = (const guint8 *) lua_tostring (L, -1);
      if (is_media) {
	grl_data_set_binary (GRL_DATA (object), key_id, binary, size);
      } else {
	grl_related_keys_set_binary (GRL_RELATED_KEYS (object), key_id, binary, size);
      }
    } else if (!lua_isnil (L, -1)) {
      GRL_WARNING ("'%s' is being ignored as G_TYPE is not being handled.",
		   key_name);
    }
  }
}

static GrlMedia *
grl_util_build_media (lua_State *L,
                      GrlMedia *user_media)
{
  GrlMedia *media = user_media;
  gint array_len;

  if (!lua_istable (L, 1)) {
    if (!lua_isnil (L, 1))
      GRL_DEBUG ("Media in wrong format (neither nil or table).");

    return user_media;
  }

  if (media == NULL) {
    lua_getfield (L, 1, "type");
    if (lua_isstring (L, -1)) {
      const gchar *media_type = lua_tostring (L, -1);

      if (g_strcmp0 (media_type, "container") == 0)
        media = grl_media_container_new ();
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

  lua_len (L, 1);
  array_len = lua_tointeger (L, -1);
  lua_pop (L, 1);

  lua_pushnil (L);
  while (lua_next (L, 1) != 0) {
    if (lua_type (L, -2) == LUA_TNUMBER &&
	lua_tointeger (L, -2) >= 1 && lua_tointeger (L, -2) <= array_len) {
      if (lua_type (L, -1) != LUA_TTABLE) {
	GRL_WARNING ("Array index should map to a table to be resolved to \
                      GrlRelatedKeys");
	lua_pop (L, 1);
	continue;
      }
      GrlRelatedKeys *related_keys = grl_related_keys_new ();
      lua_pushnil (L);
      while (lua_next (L, -2) != 0) {
        grl_util_add_key (L, G_OBJECT (related_keys), FALSE);
        lua_pop (L, 1);
      }
      grl_data_add_related_keys (GRL_DATA (media), related_keys);
    } else {
      grl_util_add_key (L, G_OBJECT (media), TRUE);
    }
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
  FetchOperation *fo = (FetchOperation *) user_data;
  lua_State *L = fo->L;
  OperationSpec *os = fo->os;
  gchar *fixed = NULL;
  gboolean cancelled = FALSE;

  if (!grl_net_wc_request_finish (GRL_NET_WC (source_object),
                                  res, &data, &len, &err)) {
    if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      GRL_DEBUG ("fetch operation was cancelled");
      cancelled = TRUE;
    }
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
    if (!cancelled)
      GRL_DEBUG ("Can't fetch element %d (URL: %s): '%s'", fo->index + 1, fo->url, err->message);
    g_clear_error (&err);
  } else {
    GRL_DEBUG ("fetch_done element %d of %d urls", fo->index + 1, fo->num_urls);
  }

  /* Check if we finished fetching URLs */
  for (i = 0; i < fo->num_urls; i++) {
    if (fo->results[i] == NULL) {
      /* Clean up this operation, and wait for
       * other operations to complete */
      g_object_unref (fo->cancellable);
      g_free (fo->url);
      g_free (fo);
      return;
    }
  }

  if (cancelled)
    goto free_fetch_op;

  /* get the callback from the registry */
  lua_rawgeti (L, LUA_REGISTRYINDEX, fo->lua_callback);

  if (!fo->is_table) {
    lua_pushlstring (L, fo->results[0], strlen (fo->results[0]));
  } else {
    lua_newtable (L);
    for (i = 0; i < fo->num_urls; i++) {
      lua_pushinteger (L, i + 1);
      lua_pushlstring (L, fo->results[i], strlen (fo->results[i]));
      lua_settable (L, -3);
    }
  }

  /* get userdata from the registry */
  lua_rawgeti (L, LUA_REGISTRYINDEX, fo->lua_userdata);

  if (!grl_lua_operations_pcall (L, 2, os, &err)) {
    if (err != NULL) {
      GRL_WARNING ("calling source callback function fail: %s", err->message);
      g_clear_error (&err);
    }
  }

free_fetch_op:
  g_object_unref (fo->cancellable);
  luaL_unref (L, LUA_REGISTRYINDEX, fo->lua_userdata);
  luaL_unref (L, LUA_REGISTRYINDEX, fo->lua_callback);

  for (i = 0; i < fo->num_urls; i++)
    g_free (fo->results[i]);
  g_free (fo->url);
  g_free (fo->results);
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
  UnzipOperation *uo = (UnzipOperation *) user_data;
  lua_State *L = uo->L;
  OperationSpec *os = uo->os;
  char **results;

  if (!grl_net_wc_request_finish (GRL_NET_WC (source_object),
                                  res, &data, &len, &err)) {
    guint len, i;
    if (g_error_matches (err, GRL_NET_WC_ERROR, GRL_NET_WC_ERROR_CANCELLED)) {
      GRL_DEBUG ("unzip operation was cancelled");
      goto free_unzip_op;
    } else if (err != NULL) {
      GRL_WARNING ("Can't fetch zip file (URL: %s): '%s'", uo->url, err->message);
      g_error_free (err);
    }
    len = g_strv_length (uo->filenames);
    results = g_new0 (gchar *, len + 1);
    for (i = 0; i < len; i++)
      results[i] = g_strdup("");
  } else {
    GRL_DEBUG ("fetch_done element (URL: %s)", uo->url);
    results = get_zipped_contents ((guchar *) data, len, (const gchar **) uo->filenames);
  }

  /* get the callback from the registry */
  lua_rawgeti(L, LUA_REGISTRYINDEX, uo->lua_callback);

  lua_newtable (L);
  for (i = 0; results[i] != NULL; i++) {
    lua_pushinteger (L, i + 1);
    lua_pushlstring (L, results[i], strlen (results[i]));
    lua_settable (L, -3);
  }

  /* get userdata from the registry */
  lua_rawgeti (L, LUA_REGISTRYINDEX, uo->lua_userdata);

  if (!grl_lua_operations_pcall (L, 2, os, &err)) {
    if (err != NULL) {
      GRL_WARNING ("calling source callback function fail: %s", err->message);
      g_error_free (err);
    }
  }

  g_strfreev (results);

free_unzip_op:
  g_object_unref (uo->cancellable);
  luaL_unref (L, LUA_REGISTRYINDEX, uo->lua_userdata);
  luaL_unref (L, LUA_REGISTRYINDEX, uo->lua_callback);
  g_strfreev (uo->filenames);
  g_free (uo->url);
  g_free (uo);
}

static GrlNetWc *
net_wc_new_with_options (lua_State *L,
                         guint      arg_offset)
{
  GrlNetWc *wc;

  wc = grl_lua_operations_get_grl_net_wc (L);
  if (arg_offset <= lua_gettop (L) && lua_istable (L, arg_offset)) {
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
        guint size = lua_tointeger (L, -1);
        grl_net_wc_set_cache_size (wc, size);

      } else if (g_strcmp0 (key, "cache") == 0) {
        gboolean use_cache = lua_toboolean (L, -1);
        grl_net_wc_set_cache (wc, use_cache);

      } else if (g_strcmp0 (key, "throttling") == 0) {
        guint throttling = lua_tointeger (L, -1);
        grl_net_wc_set_throttling (wc, throttling);

      } else if (g_strcmp0 (key, "loglevel") == 0) {
        guint level = lua_tointeger (L, -1);
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

  os = grl_lua_operations_get_current_op (L);
  if (os == NULL) {
    luaL_error (L, "grl.get_options() failed: Can't retrieve current operation. "
                   "Source is broken as grl.callback() has been called but source "
                   "is still active");
    return 0;
  }

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

    lua_pushinteger (L, count);
    return 1;
  }

  if (g_strcmp0 (option, "skip") == 0) {
    guint skip = grl_operation_options_get_skip (os->options);

    lua_pushinteger (L, skip);
    return 1;
  }

  if (g_strcmp0 (option, "flags") == 0) {
    GrlResolutionFlags flags = grl_operation_options_get_resolution_flags (os->options);

    lua_pushinteger (L, (gint) flags);
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
      (value) ? (void) lua_pushinteger (L, g_value_get_int (value)) : lua_pushnil (L);
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

    luaL_argcheck (L, lua_isstring (L, 2), 2, "expecting key name");
    key_name = lua_tostring (L, 2);

    key = grl_registry_lookup_metadata_key (registry, key_name);
    if (key != GRL_METADATA_KEY_INVALID) {
      grl_operation_options_get_key_range_filter (os->options, key, &min, &max);
      switch (grl_registry_lookup_metadata_key_type (registry, key)) {
      case G_TYPE_INT:
        (min) ? (void) lua_pushinteger (L, g_value_get_int (min)) : lua_pushnil (L);
        (max) ? (void) lua_pushinteger (L, g_value_get_int (max)) : lua_pushnil (L);
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
    lua_pushinteger (L, (gint) os->operation_id);
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
* grl.get_requested_keys
*
* @return: array of all requested keys from application (may be empty);
*/
static gint
grl_l_operation_get_keys (lua_State *L)
{
  OperationSpec *os;
  GrlRegistry *registry;
  GList *it;

  os = grl_lua_operations_get_current_op (L);
  if (os == NULL) {
    luaL_error (L, "grl.get_requested_keys() failed: Can't retrieve current operation. "
                   "Source is broken as grl.callback() has been called but source "
                   "is still active");
    return 0;
  }

  registry = grl_registry_get_default ();
  lua_newtable (L);
  for (it = os->keys; it != NULL; it = it->next) {
    GrlKeyID key_id;
    char *key_name, *ptr;

    key_id = GRLPOINTER_TO_KEYID (it->data);
    if (key_id == GRL_METADATA_KEY_INVALID)
      continue;

    key_name = g_strdup (grl_registry_lookup_metadata_key_name (registry, key_id));
    /* Replace '-' to '_': convenient for the developer */
    while ((ptr = strstr (key_name, "-")) != NULL) {
      *ptr = '_';
    }

    lua_pushstring (L, key_name);
    lua_pushboolean (L, 1);
    lua_settable (L, -3);
    g_free (key_name);
  }
  return 1;
}

static gboolean
push_grl_media_key (lua_State *L,
                    GrlMedia *media,
                    GrlKeyID key_id)
{
  GrlRegistry *registry;
  GType type;
  const gchar *key_name;
  guint i, num_values;
  gboolean is_array = FALSE;

  registry = grl_registry_get_default ();
  type = grl_registry_lookup_metadata_key_type (registry, key_id);
  key_name = grl_registry_lookup_metadata_key_name (registry, key_id);

  num_values = grl_data_length (GRL_DATA (media), key_id);
  if (num_values  == 0)  {
    GRL_DEBUG ("Key %s has no data", key_name);
    return FALSE;
  } else if (num_values > 1) {
    is_array = TRUE;
    lua_createtable (L, num_values, 0);
  }

  for (i = 0; i < num_values; i++) {
    GrlRelatedKeys *relkeys;
    const GValue *val;

    relkeys = grl_data_get_related_keys (GRL_DATA (media), key_id, i);
    if (!relkeys) {
      GRL_DEBUG ("Key %s failed to retrieve data at index %d due NULL GrlRelatedKeys",
                 key_name, i);
      continue;
    }

    val = grl_related_keys_get (relkeys, key_id);
    if (!val) {
      GRL_DEBUG ("Key %s failed to retrieve data at index %d due NULL GValue",
                 key_name, i);
      continue;
    }

    if (is_array)
      lua_pushinteger (L, i + 1);

    switch (type) {
    case G_TYPE_INT:
      lua_pushinteger (L, g_value_get_int (val));
      break;
    case G_TYPE_FLOAT:
      lua_pushnumber (L, g_value_get_float (val));
      break;
    case G_TYPE_STRING:
      lua_pushstring (L, g_value_get_string (val));
      break;
    case G_TYPE_INT64:
      lua_pushinteger (L, g_value_get_int64 (val));
      break;
    case G_TYPE_BOOLEAN:
      lua_pushboolean (L, g_value_get_boolean (val));
      break;
    default:
      if (type == G_TYPE_DATE_TIME) {
        GDateTime *date = g_value_get_boxed (val);
        gchar *date_str = g_date_time_format (date, "%F %T");
        lua_pushstring (L, date_str);
        g_free(date_str);
      } else {
        GRL_DEBUG ("Key %s has unhandled G_TYPE. Lua source will miss this data",
                   key_name);
        goto fail;
      }
    }

    if (is_array)
      lua_settable (L, -3);
  }
  return TRUE;

fail:
  if (is_array)
    lua_pop (L, 1);

  return FALSE;
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
  const gchar *media_type = NULL;
  GrlMedia *media;

  os = grl_lua_operations_get_current_op (L);
  if (os == NULL) {
    luaL_error (L, "grl.get_media_keys() failed: Can't retrieve current operation. "
                   "Source is broken as grl.callback() has been called but source "
                   "is still active");
    return 0;
  }
  media = os->media;

  if (media == NULL) {
    lua_pushnil (L);
    return 1;
  }

  registry = grl_registry_get_default ();
  lua_newtable (L);

  if (grl_media_is_audio (media)) {
    media_type = "audio";
  } else if (grl_media_is_video (media)) {
    media_type = "video";
  } else if (grl_media_is_image (media)) {
    media_type = "image";
  } else if (grl_media_is_container (media)) {
    media_type = "container";
  }

  if (media_type) {
    lua_pushstring (L, "type");
    lua_pushstring (L, media_type);
    lua_settable (L, -3);
  }

  list_keys = grl_data_get_keys (GRL_DATA (media));
  for (it = list_keys; it != NULL; it = it->next) {
    GrlKeyID key_id;
    gchar *key_name;
    gchar *ptr = NULL;

    key_id = GRLPOINTER_TO_KEYID (it->data);
    if (key_id == GRL_METADATA_KEY_INVALID)
      continue;

    key_name = g_strdup (grl_registry_lookup_metadata_key_name (registry,
                                                                key_id));
    /* Replace '-' to '_': as a convenience for the developer */
    while ((ptr = strstr (key_name, "-")) != NULL) {
      *ptr = '_';
    }

    lua_pushstring (L, key_name);
    if (push_grl_media_key (L, media, key_id))
      lua_settable (L, -3);
    else
      lua_pop (L, 1);

    g_free (key_name);
  }
  g_list_free (list_keys);
  return 1;
}

/**
* grl.fetch
*
* @url: (string or array) The http URL(s) to GET the content.
* @netopts: [optional] (table) Options to set the GrlNetWc object.
* @callback: (function) The function to be called after fetch is complete.
* @userdata: [optional] User data to be passed to the @callback.
* @return: Nothing.;
*/
static gint
grl_l_fetch (lua_State *L)
{
  guint i;
  guint num_urls;
  gchar **urls;
  gchar **results;
  gint lua_userdata;
  gint lua_callback;
  GrlNetWc *wc;
  gboolean is_table = FALSE;
  OperationSpec *os;

  luaL_argcheck (L, (lua_isstring (L, 1) || lua_istable (L, 1)), 1,
                 "expecting url as string or an array of urls");
  luaL_argcheck (L, (lua_isfunction (L, 2) || lua_istable (L, 2)), 2,
                 "expecting callback function or network parameters");

  luaL_argcheck (L, (lua_isfunction (L, 2) ||
                     (lua_istable (L, 2) && lua_isfunction (L, 3))), 3,
                 "expecting callback function after network parameters");

  os = grl_lua_operations_get_current_op (L);
  if (os == NULL) {
    luaL_error (L, "grl.fetch() failed: Can't retrieve current operation. "
                   "Source is broken as grl.callback() has been called but source "
                   "is still active");
    return 0;
  }

  /* keep arguments aligned */
  if (lua_isfunction (L, 2)) {
    lua_pushnil (L);
    lua_insert (L, 2);
  }

  if (lua_gettop (L) > 4)
    luaL_error (L, "too many arguments to 'fetch' function");

  /* add nil if userdata is omitted */
  lua_settop (L, 4);

  /* pop the userdata and store it in registry */
  lua_userdata = luaL_ref (L, LUA_REGISTRYINDEX);
  /* pop the callback and store it in registry */
  lua_callback = luaL_ref (L, LUA_REGISTRYINDEX);

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

  if (!verify_plaintext_fetch (L, urls, num_urls)) {
    GRL_WARNING ("Source '%s' is broken, it makes plaintext network queries but "
                 "does not set the 'net:plaintext' tag", grl_source_get_id (os->source));

    luaL_unref (L, LUA_REGISTRYINDEX, lua_userdata);
    luaL_unref (L, LUA_REGISTRYINDEX, lua_callback);
    lua_gc (L, LUA_GCCOLLECT, 0);
    g_free (urls);
    return 0;
  }

  wc = net_wc_new_with_options (L, 2);

  /* shared data between urls */
  results = g_new0 (gchar *, num_urls);
  for (i = 0; i < num_urls; i++) {
    FetchOperation *fo;

    fo = g_new0 (FetchOperation, 1);
    fo->L = L;
    fo->os = os;
    fo->cancellable = g_object_ref (os->cancellable);
    fo->lua_userdata = lua_userdata;
    fo->lua_callback = lua_callback;
    fo->index = i;
    fo->url = g_strdup (urls[i]);
    fo->num_urls = num_urls;
    fo->is_table = is_table;
    fo->results = results;

    grl_net_wc_request_async (wc, urls[i], os->cancellable, grl_util_fetch_done, fo);
  }
  g_free (urls);

  /* Set the state as wating for this async operation */
  grl_lua_operations_set_source_state (L, LUA_SOURCE_WAITING, os);
  return 0;
}

static void
grl_l_request_set_headers(lua_State *L, RestProxyCall *proxy_call, uint arg_offset)
{
  /* Set seaders */
  if (arg_offset <= lua_gettop (L) && lua_istable (L, arg_offset)) {
    lua_pushnil (L);

    while (lua_next (L, arg_offset) != 0) {
      const char *key = lua_tostring (L, -2);
      const char *value = lua_tostring (L, -1);

      rest_proxy_call_add_header (proxy_call, key, value);

      lua_pop (L, 1);
    }
  }
}

static void
grl_l_request_set_params(lua_State *L, RestProxyCall *proxy_call, uint arg_offset)
{
  /* Set params */
  if (arg_offset <= lua_gettop (L) && lua_istable (L, arg_offset)) {
    lua_pushnil (L);

    while (lua_next (L, arg_offset) != 0) {
      const char *key = lua_tostring (L, -2);
      const char *value = lua_tostring (L, -1);

      rest_proxy_call_add_param (proxy_call, key, value);

      lua_pop (L, 1);
    }
  }
}

static void
grl_util_request_done_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  RequestOperation *request_op = user_data;
  g_autoptr (GError) error = NULL;
  gssize len_results = 0;
  const char *payload;

  if (!rest_proxy_call_invoke_finish (REST_PROXY_CALL (source_object), res, &error)) {
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      GRL_DEBUG ("request operation was cancelled");
    else
      GRL_DEBUG ("The request has failed: '%s'", error->message);
  } else {
    payload = rest_proxy_call_get_payload (REST_PROXY_CALL (source_object));
    len_results = rest_proxy_call_get_payload_length (REST_PROXY_CALL (source_object));

    GRL_DEBUG ("request_done %ld elements retrieved", len_results);

    /* get the callback from the registry */
    lua_rawgeti (request_op->L, LUA_REGISTRYINDEX, request_op->lua_callback);

    /* push the results from the request */
    lua_pushlstring (request_op->L, len_results > 0 ? payload : "", len_results);

    /* get userdata from the registry */
    lua_rawgeti (request_op->L, LUA_REGISTRYINDEX, request_op->lua_userdata);

    if (!grl_lua_operations_pcall (request_op->L, 2, request_op->os, &error)) {
      if (error != NULL) {
        GRL_WARNING ("calling source callback function fail: %s", error->message);
      }
    }
  }

  luaL_unref (request_op->L, LUA_REGISTRYINDEX, request_op->lua_userdata);
  luaL_unref (request_op->L, LUA_REGISTRYINDEX, request_op->lua_callback);

  g_clear_object (&request_op->cancellable);
  g_clear_pointer (&request_op, g_free);
}

/**
* grl.request
*
* @url: (string) The http URL to GET/POST the content.
* @method: (string) 'GET' or 'POST' method.
* @header: (table) header of the request.
* @params: (table) parameters of the request.
* @callback: (function) The function to be called after request is complete.
* @userdata: [optional] User data to be passed to the @callback.
* @return: Nothing.;
*/
static int
grl_l_request (lua_State *L)
{
  g_autoptr(RestProxy) rest_proxy = NULL;
  g_autoptr(RestProxyCall) proxy_call = NULL;
  RequestOperation *request_op;
  OperationSpec *os;
  const char *url = NULL;
  const char *method = NULL;
  int lua_userdata;
  int lua_callback;

  luaL_argcheck (L, lua_isstring (L, 1), 1,
                 "expecting url as string");
  luaL_argcheck (L, lua_isstring (L, 2), 2,
                 "expecting 'GET' or 'POST' as string");
  luaL_argcheck (L, lua_istable (L, 3), 3,
                 "expecting the header table");
  luaL_argcheck (L, lua_istable (L, 4), 4,
                 "expecting the parameters table");
  luaL_argcheck (L, lua_isfunction (L, 5), 5,
                 "expecting callback function");

  os = grl_lua_operations_get_current_op (L);
  if (os == NULL) {
    luaL_error (L, "grl.request() failed: Can't retrieve current operation. "
                   "Source is broken as grl.callback() has been called but source "
                   "is still active");
    return 0;
  }

  /* keep arguments aligned */
  if (lua_isfunction (L, 2)) {
    lua_pushnil (L);
    lua_insert (L, 2);
  }

  if (lua_gettop (L) > 6)
    luaL_error (L, "too many arguments to 'request' function");

  /* add nil if userdata is omitted */
  lua_settop (L, 6);

  /* pop the userdata and store it in registry */
  lua_userdata = luaL_ref (L, LUA_REGISTRYINDEX);
  /* pop the callback and store it in registry */
  lua_callback = luaL_ref (L, LUA_REGISTRYINDEX);

  url = lua_tolstring (L, 1, NULL);
  method = lua_tolstring (L, 2, NULL);

  rest_proxy = rest_proxy_new (url, FALSE);

  // special key to determine if we use our ProxyCall or the default
  if (lua_getfield(L, 4, "grl-json") != LUA_TNIL) {
    proxy_call = lua_rest_proxy_call_new (rest_proxy);
    lua_pop(L, 1);
  } else {
    proxy_call = rest_proxy_new_call (rest_proxy);
  }

  rest_proxy_call_set_method (proxy_call, method);
  grl_l_request_set_headers(L, proxy_call, 3);
  grl_l_request_set_params(L, proxy_call, 4);

  request_op = g_new0 (RequestOperation, 1);
  request_op->L = L;
  request_op->lua_userdata = lua_userdata;
  request_op->lua_callback = lua_callback;
  request_op->cancellable = g_object_ref (os->cancellable);
  request_op->os = os;

  rest_proxy_call_invoke_async (proxy_call, os->cancellable, grl_util_request_done_cb, request_op);

  /* Set the state as wating for this async operation */
  grl_lua_operations_set_source_state (L, LUA_SOURCE_WAITING, os);

  return 0;
}

/**
* grl.callback
*
* @media: (table) The media content to be returned.
* @count: (integer) Number of media remaining to the application.
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
  os = grl_lua_operations_get_current_op (L);
  if (os == NULL) {
    luaL_error (L, "grl.callback() failed: Can't retrieve current operation. "
                   "Source is broken as grl.callback() has been called but source "
                   "is still active");
    return 0;
  }

  media = (os->op_type == LUA_RESOLVE) ? os->media : NULL;

  if (nparam > 0) {
    media = grl_util_build_media (L, media);
    count = (lua_isinteger (L, 2)) ? lua_tointeger (L, 2) : 0;
  }

  switch (os->op_type) {
  case LUA_RESOLVE:
    os->cb.resolve (os->source, os->operation_id, media, os->user_data, NULL);
    break;

  default:
    os->cb.result (os->source, os->operation_id, media,
                   count, os->user_data, NULL);
  }

  /* finishing callback */
  if (count == 0)
    grl_lua_operations_set_source_state (L, LUA_SOURCE_FINALIZED, os);

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
 * @netopts: [optional] (table) Options to set the GrlNetWc object.
 * @callback: (function) The function to be called after fetch is complete.
 * @userdata: [optional] User data to be passed to the @callback.
 * @return: Nothing.;
 */
static gint
grl_l_unzip (lua_State *L)
{
  gint lua_userdata;
  gint lua_callback;
  const gchar *url;
  GrlNetWc *wc;
  UnzipOperation *uo;
  guint num_filenames, i;
  gchar **filenames;
  OperationSpec *os;

  luaL_argcheck (L, lua_isstring (L, 1), 1,
                 "expecting url as string");
  luaL_argcheck (L, lua_istable (L, 2), 2,
                 "expecting filenames as an array of filenames");
  luaL_argcheck (L, (lua_isfunction (L, 3) || lua_istable (L, 3)), 3,
                 "expecting callback function or network parameters");
  luaL_argcheck (L, (lua_isfunction (L, 3) ||
                     (lua_istable (L, 3) && lua_isfunction (L, 4))), 4,
                 "expecting callback function after network parameters");

  /* keep arguments aligned */
  if (lua_isfunction (L, 3)) {
    lua_pushnil (L);
    lua_insert (L, 3);
  }

  if (lua_gettop (L) > 5)
    luaL_error (L, "too many arguments to 'unzip' function");

  /* add nil if userdata is omitted */
  lua_settop (L, 5);

  /* pop the userdata and store it in registry */
  lua_userdata = luaL_ref (L, LUA_REGISTRYINDEX);
  /* pop the callback and store it in registry */
  lua_callback = luaL_ref (L, LUA_REGISTRYINDEX);

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

  os = grl_lua_operations_get_current_op (L);
  if (os == NULL) {
    luaL_error (L, "grl.unzip() failed: Can't retrieve current operation. "
                   "Source is broken as grl.callback() has been called but source "
                   "is still active");
    return 0;
  }
  wc = net_wc_new_with_options (L, 3);

  uo = g_new0 (UnzipOperation, 1);
  uo->L = L;
  uo->cancellable = g_object_ref (os->cancellable);
  uo->lua_userdata = lua_userdata;
  uo->lua_callback = lua_callback;
  uo->url = g_strdup (url);
  uo->filenames = filenames;
  uo->os = os;

  grl_net_wc_request_async (wc, url, os->cancellable, grl_util_unzip_done, uo);

  grl_lua_operations_set_source_state (L, LUA_SOURCE_WAITING, os);
  return 0;
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
  GRL_WARNING ("Source is broken as it tries to access gnome-online-accounts "
               "information, but it should not have been created");
  return 0;
#else
  {
    GoaObject *object;
    GoaOAuth2Based *oauth2 = NULL;

    object = grl_lua_library_load_goa_data (L);
    if (object != NULL) {
      /* FIXME handle other types of object? */
      oauth2 = goa_object_peek_oauth2_based (object);
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
  GRL_WARNING ("Source is broken as it tries to access gnome-online-accounts "
               "information, but it should not have been created");
  return 0;
#else
  {
    GoaObject *object;
    GoaOAuth2Based *oauth2 = NULL;

    object = grl_lua_library_load_goa_data (L);

    if (object != NULL) {
      /* FIXME handle other types of object? */
      oauth2 = goa_object_peek_oauth2_based (object);
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
    {"request", &grl_l_request},
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

  lua_pushstring (L, GRILO_LUA_LIBRARY_XML);
  luaopen_xml (L);
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

  /* grl.lua will be read-only from now on for Lua sources */
  grl_lua_operations_set_proxy_table (L, -1);

  /* Those modules are called in 'lua' table, inside 'grl' */
  lua_settable (L, -3);

  grl_lua_operations_init_priv_state (L);

  /* grl will be read-only from now on for Lua sources */
  grl_lua_operations_set_proxy_table (L, -1);

  return 1;
}

/* ======= Lua-Library and Lua-Factory utilities ============= */

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
#ifdef GOA_ENABLED
static gpointer
grl_lua_library_load_goa_data (lua_State *L)
{
  GoaObject *goa_object;

  lua_getglobal (L, GOA_LUA_NAME);
  goa_object = (lua_islightuserdata(L, -1)) ? lua_touserdata(L, -1) : NULL;
  lua_pop(L, 1);

  return goa_object;
}
#endif /* GOA_ENABLED */
