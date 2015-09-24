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

#include "lua-libraries.h"

#include <json-glib/json-glib.h>
#include <glib-object.h>

/* ================== Lua-Library Json Handlers ============================ */

/* Save key/values on the table in the stack if the value is an
 * object or an array, it calls recursively the function again.
 *
 * @param L, pointer to the L with nil on top of it;
 * @param reader, pointed to the first element of main object;
 *
 * returns: the table in the stack with all json values
 */
static void
build_table_from_json_reader (lua_State *L,
                              JsonReader *reader)
{
  const GError *err = json_reader_get_error (reader);
  if (err != NULL) {
    GRL_WARNING ("Error when building json: %s", err->message);
    return;
  }

  if (lua_isnil (L, -1)) {
    /* In the first execution of this recursive call, the main json object
     * does not have a member name. The nil is in the top of the stack and
     * it shall be converted to the table with json content */
    lua_pop (L, 1);

  } else if (lua_istable (L, -1)) {
    const gchar *member_name = json_reader_get_member_name (reader);
    if (member_name)
      lua_pushstring (L, member_name);

  } else if (!lua_isnumber (L, -1)) {
    GRL_DEBUG ("getting value to either table or array");
    return;
  }

  if (json_reader_is_object (reader)) {
    guint index_member = 0;
    guint num_members = json_reader_count_members (reader);

    lua_createtable (L, num_members, 0);
    for (index_member = 0; index_member < num_members; index_member++) {
      json_reader_read_element (reader, index_member);
      build_table_from_json_reader (L, reader);
      json_reader_end_element (reader);
    }

  } else if (json_reader_is_array (reader)) {
    guint index_element = 0;
    guint num_elements = json_reader_count_elements (reader);

    lua_createtable (L, num_elements, 0);
    for (index_element = 0; index_element < num_elements; index_element++) {
      json_reader_read_element (reader, index_element);
      lua_pushinteger (L, index_element + 1);
      build_table_from_json_reader (L, reader);
      json_reader_end_element (reader);
    }

  } else if (json_reader_is_value (reader)) {
    if (json_reader_get_null_value (reader)) {
      lua_pushnil (L);
    } else {
      /* value of the element */
      JsonNode *value = json_reader_get_value (reader);
      switch (json_node_get_value_type (value)) {
      case G_TYPE_STRING:
        lua_pushstring (L, json_reader_get_string_value (reader));
        break;
      case G_TYPE_INT64:
        lua_pushinteger (L, json_reader_get_int_value (reader));
        break;
      case G_TYPE_DOUBLE:
        lua_pushnumber (L, json_reader_get_double_value (reader));
        break;
      case G_TYPE_BOOLEAN:
        lua_pushboolean (L, json_reader_get_boolean_value (reader));
        break;
      default:
        GRL_DEBUG ("'%d' (json-node-type) is not being handled",
                   (gint) json_node_get_value_type (value));
        lua_pushnil (L);
      }
    }
  }

  if (lua_gettop (L) > 3) {
    /* save this key/value on previous table */
    lua_settable (L, -3);
  }
}

/* grl.lua.json.string_to_table
 *
 * @json_str: (string) A Json object as a string.
 *
 * @return: All json content as a table.
 */
static gint
grl_json_parse_string (lua_State *L)
{
  JsonParser *parser = NULL;
  JsonReader *reader = NULL;
  const gchar *json_str = NULL;
  GError *err = NULL;

  luaL_argcheck (L, lua_isstring (L, 1), 1, "json string expected");
  json_str = lua_tostring (L, 1);

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, json_str, -1, &err)) {
    GRL_DEBUG ("Can't parse json string: '%s'", err->message);
    g_error_free (err);
    g_object_unref (parser);
    return 0;
  }

  reader = json_reader_new (json_parser_get_root (parser));

  /* The return of recursive function will be a table with all
   * json content in it */
  lua_pushnil (L);
  build_table_from_json_reader (L, reader);

  g_object_unref (reader);
  g_object_unref (parser);

  return 1;
}

gint
luaopen_json (lua_State *L)
{
  static const luaL_Reg json_library_fn[] = {
    {"string_to_table", &grl_json_parse_string},
    {NULL, NULL}
  };

  luaL_newlib (L, json_library_fn);
  return 1;
}
