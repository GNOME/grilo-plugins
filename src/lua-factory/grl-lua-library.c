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

#include <net/grl-net.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "grl-lua-common.h"
#include "grl-lua-library.h"
#include "lua-library/lua-libraries.h"

#define GRL_LOG_DOMAIN_DEFAULT lua_library_log_domain
GRL_LOG_DOMAIN_STATIC (lua_library_log_domain);

typedef struct _FetchOperation {
  lua_State *L;
  guint operation_id;
  gchar *lua_cb;
  guint index;
  gchar *url;
  guint num_urls;
  gboolean is_table;
  gchar **results;
} FetchOperation;

/* ================== Lua-Library utils/helpers ============================ */

static gchar *
char_str (gunichar c,
          gchar   *buf)
{
  memset (buf, 0, 8);
  g_unichar_to_utf8 (c, buf);
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
          char_str (l, buf);
          strcpy (to, buf);
          to += strlen (buf) - 1;
          from = end;
        } else {
          continue;
        }
      } else if (strncmp (from, "lt;", 3) == 0) {
        *to = '<';
        from += 2;
      } else if (strncmp (from, "gt;", 3) == 0) {
        *to = '>';
        from += 2;
      } else if (strncmp (from, "amp;", 4) == 0) {
        *to = '&';
        from += 3;
      } else if (strncmp (from, "quot;", 5) == 0) {
        *to = '"';
        from += 4;
      } else if (strncmp (from, "apos;", 5) == 0) {
        *to = '\'';
        from += 4;
      } else {
        continue;
      }
    }
  }

  return ret;
}

/* Top of the stack must be a table */
static void
grl_util_add_table_to_media (lua_State *L,
                             GrlMedia *media,
                             GrlKeyID key_id,
                             const gchar *key_name,
                             GType type)
{
  gint i = 0;
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

    case G_TYPE_FLOAT:
      if (lua_isnumber (L, -1))
        grl_data_add_float (GRL_DATA (media), key_id, lua_tointeger (L, -1));
      break;

    case G_TYPE_STRING:
      if (lua_isstring (L, -1))
        grl_data_add_string (GRL_DATA (media), key_id, lua_tostring (L, -1));
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
  GrlRegistry *registry = NULL;
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

      case G_TYPE_STRING:
        if (lua_isstring (L, -1)) {
          grl_data_set_string (GRL_DATA (media), key_id, lua_tostring (L, -1));
        } else if (lua_istable (L, -1)) {
          grl_util_add_table_to_media (L, media, key_id, key_name, type);
        } else if (!lua_isnil (L, -1)) {
          GRL_WARNING ("'%s' is not compatible for '%s'",
                       lua_typename (L, -1), key_name);
        }
        break;

      default:
        /* Non-fundamental types don't reduce to ints, so can't be
         * in the switch statement */
        if (type == G_TYPE_DATE_TIME) {
          GDateTime *date = grl_date_time_from_iso8601 (lua_tostring (L, -1));
          grl_data_set_boxed (GRL_DATA (media), key_id, date);
          g_date_time_unref (date);
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
  gchar *data = NULL;
  gsize len;
  guint i = 0;
  GError *err = NULL;
  OperationSpec *os;
  FetchOperation *fo = (FetchOperation *) user_data;
  lua_State *L = fo->L;

  grl_net_wc_request_finish (GRL_NET_WC (source_object),
                             res, &data, &len, &err);
  if (!g_utf8_validate(data, len, NULL)) {
    data = NULL;
    g_set_error_literal (&err, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                         "Fetched item is not valid UTF-8");
  }

  fo->results[fo->index] = (err == NULL) ? g_strdup (data) : g_strdup ("");
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
  OperationSpec *os = NULL;
  const gchar *option = NULL;

  luaL_argcheck (L, lua_isstring (L, 1), 1, "expecting option (string)");

  os = grl_lua_library_get_current_operation (L);
  g_return_val_if_fail (os != NULL, 0);
  option = lua_tostring (L, 1);

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
    GrlResolutionFlags flags = grl_operation_options_get_flags (os->options);

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
      (value) ? lua_pushnumber (L, g_value_get_int (value)) : lua_pushnil (L);
      break;

    case G_TYPE_FLOAT:
      (value) ? lua_pushnumber (L, g_value_get_float (value)) : lua_pushnil (L);
      break;

    case G_TYPE_STRING:
      (value) ? lua_pushstring (L, g_value_get_string (value)) : lua_pushnil (L);
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
        (min) ? lua_pushnumber (L, g_value_get_int (min)) : lua_pushnil (L);
        (max) ? lua_pushnumber (L, g_value_get_int (max)) : lua_pushnil (L);
        break;

      case G_TYPE_FLOAT:
        (min) ? lua_pushnumber (L, g_value_get_float (min)) : lua_pushnil (L);
        (max) ? lua_pushnumber (L, g_value_get_float (max)) : lua_pushnil (L);
        break;

      case G_TYPE_STRING:
        (min) ? lua_pushstring (L, g_value_get_string (min)) : lua_pushnil (L);
        (max) ? lua_pushstring (L, g_value_get_string (max)) : lua_pushnil (L);
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
  OperationSpec *os = NULL;
  GrlRegistry *registry = NULL;
  GList *it = NULL;
  GrlKeyID key_id;
  const gchar *key_name = NULL;
  gint i = 0;

  os = grl_lua_library_get_current_operation (L);
  g_return_val_if_fail (os != NULL, 0);

  registry = grl_registry_get_default ();
  lua_newtable (L);
  for (it = os->keys; it; it = g_list_next (it)) {
    key_id = GRLPOINTER_TO_KEYID (it->data);
    key_name = grl_registry_lookup_metadata_key_name (registry, key_id);
    if (key_id != GRL_METADATA_KEY_INVALID) {
      lua_pushinteger (L, i + 1);
      lua_pushstring (L, key_name);
      lua_settable (L, -3);
      i = i + 1;
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
  OperationSpec *os = NULL;
  GrlRegistry *registry = NULL;
  GList *it = NULL;
  GList *list_keys = NULL;
  GrlKeyID key_id;
  gchar *key_name = NULL;

  os = grl_lua_library_get_current_operation (L);
  g_return_val_if_fail (os != NULL, 0);

  registry = grl_registry_get_default ();
  lua_newtable (L);
  list_keys = grl_data_get_keys (GRL_DATA (os->media));
  for (it = list_keys; it; it = g_list_next (it)) {
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
    g_free (key_name);
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
      default:
        if (type == G_TYPE_DATE_TIME) {
          GDateTime *date = grl_data_get_boxed (GRL_DATA (os->media), key_id);
          gchar *date_str = g_date_time_format (date, "%F %T");
          lua_pushstring (L, date_str);
          g_free(date_str);

        } else {
          GRL_DEBUG ("'%s' is being ignored as G_TYPE is not being handled.",
                     key_name);
          lua_pop (L, 1);
          continue;
        }
      }
      lua_settable (L, -3);
    }
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
  guint i = 0;
  guint num_urls = 0;
  gchar **urls = NULL;
  gchar **results = NULL;
  const gchar *lua_callback = NULL;
  GrlNetWc *wc = NULL;
  FetchOperation *fo = NULL;
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

  lua_callback = lua_tolstring (L, 2, NULL);

  wc = grl_net_wc_new ();
  if (lua_istable (L, 3)) {
    /* Set GrlNetWc options */
    lua_pushnil (L);
    while (lua_next (L, 3) != 0) {
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

  /* shared data between urls */
  results = g_new0 (gchar *, num_urls);
  for (i = 0; i < num_urls; i++) {
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
  gint nparam = 0;
  gint count = 0;
  OperationSpec *os = NULL;
  GrlMedia *media = NULL;

  GRL_DEBUG ("grl.callback()");

  nparam = lua_gettop (L);
  os = grl_lua_library_get_current_operation (L);
  g_return_val_if_fail (os != NULL, 0);

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

/* ================== Lua-Library initialization =========================== */

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
    {"decode", &grl_l_decode},
    {"unescape", &grl_l_unescape},
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
      g_warning ("Source '%s' is broken, as there are no pending operations "
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
