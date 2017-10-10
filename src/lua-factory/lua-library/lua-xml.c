/*
 * Copyright (C) 2015 Bastien Nocera <hadess@hadess.net>
 *
 * Contact: Victor Toso <me@victortoso.com>
 * Bastien Nocera <hadess@hadess.net>
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

#include <string.h>
#include <libxml/tree.h>
#include <libxml/parser.h>

/* ================== Lua-Library Json Handlers ============================ */
static void build_table_from_xml_reader (lua_State  *L,
                                         xmlDocPtr   doc,
                                         xmlNodePtr  parent);

static void
build_table_recursively (lua_State  *L,
                         xmlDocPtr   doc,
                         xmlNodePtr  parent)
{
  gboolean is_root_node;
  xmlNodePtr node;
  GHashTable *ht;
  GHashTableIter iter;
  gpointer key, value;

  if (parent == NULL) {
    is_root_node = TRUE;
    node = xmlDocGetRootElement (doc);
  } else {
    is_root_node = FALSE;
    node = parent->children;
  }

  /* We need to iterate in the xml nodes by its name so we can attach
   * nodes with same name to the same array (table). */
  ht = g_hash_table_new (g_str_hash, g_str_equal);
  for (; node != NULL; node = node->next) {
    GList *list;

    if (node->name == NULL || g_str_equal (node->name, "text"))
      continue;

    list = g_hash_table_lookup (ht, (gchar *) node->name);
    list = g_list_prepend (list, node);
    g_hash_table_insert (ht, (gchar *) node->name, list);
  }

  g_hash_table_iter_init (&iter, ht);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    GList *it = g_list_reverse (value);
    guint len = g_list_length (it);
    guint i;
    xmlNodePtr node;
    GList *list = it;

    if (len == 1) {
      /* Only one node, no array needed */
      node = it->data;
      node = (is_root_node) ? node : node->children;
      lua_pushstring (L, (gchar *) key);
      lua_newtable (L);
      build_table_from_xml_reader (L, doc, it->data);
    } else {
      /* node-name will have an array of tables here */
      lua_pushstring (L, (gchar *) key);
      lua_createtable (L, len, 0);
      for (i = 0; i < len; i++) {
        node = it->data;
        lua_pushinteger (L, i + 1);
        lua_newtable (L);
        build_table_from_xml_reader (L, doc, node);
        lua_settable(L, -3);
        it = it->next;
      }
    }
    lua_settable(L, -3);
    g_list_free (list);
  }
  g_hash_table_destroy (ht);
}

/* Save key/values on the table in the stack if the value is an
 * object or an array, it calls recursively the function again.
 *
 * @param L, pointer to the L with nil on top of it;
 * @param reader, pointed to the first element of main object;
 *
 * returns: the table in the stack with all xml values
 */
static void
build_table_from_xml_reader (lua_State  *L,
                             xmlDocPtr   doc,
                             xmlNodePtr  parent)
{
  xmlChar *str;
  xmlAttrPtr attr;

  if (parent == NULL) {
    /* In the root element we check for sibilings and start the recursive
     * procedure to map the lua table. */
    build_table_recursively (L, doc, NULL);
    return;
  }

  str = xmlNodeListGetString (doc, parent->xmlChildrenNode, 1);
  if (str) {
    /* We use xml to the value as it is a forbidden node name */
    lua_pushstring (L, "xml");
    lua_pushstring (L, (gchar *) str);
    lua_settable(L, -3);
    xmlFree (str);
  }

  for (attr = parent->properties; attr != NULL; attr = attr->next) {
    xmlChar *val;

    if (!attr->name)
      continue;

    val = xmlGetProp (parent, (const xmlChar *) attr->name);
    if (val) {
      lua_pushstring (L, (gchar *) attr->name);
      lua_pushstring (L, (gchar *) val);
      lua_settable(L, -3);
      xmlFree (val);
    } else {
      GRL_WARNING ("xml-parser not handling empty properties as %s", attr->name);
    }
  }

  build_table_recursively (L, doc, parent);
}

/* grl.lua.xml.string_to_table
 *
 * @xml_str: (string) XML as a string.
 *
 * @return: All XML content as a table.
 */
static gint
grl_xml_parse_string (lua_State *L)
{
  xmlDocPtr doc;
  const gchar *xml_str = NULL;
  int len;

  luaL_argcheck (L, lua_isstring (L, 1), 1, "xml string expected");
  xml_str = lua_tostring (L, 1);

  len = strlen (xml_str);
  doc = xmlParseMemory (xml_str, len);
  if (!doc)
    doc = xmlRecoverMemory (xml_str, len);

  if (!doc) {
    GRL_DEBUG ("Can't parse XML string");
    return 0;
  }

  /* See "Traversing the tree" at:
   * http://www.xmlsoft.org/library.html */
  lua_newtable (L);
  build_table_from_xml_reader (L, doc, NULL);

  xmlFreeDoc (doc);

  return 1;
}

gint
luaopen_xml (lua_State *L)
{
  static const luaL_Reg xml_library_fn[] = {
    {"string_to_table", &grl_xml_parse_string},
    {NULL, NULL}
  };

  luaL_newlib (L, xml_library_fn);
  return 1;
}
