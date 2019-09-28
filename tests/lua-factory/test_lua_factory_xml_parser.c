/*
 * Copyright (C) 2015. All rights reserved.
 *
 * Author: Victor Toso <me@victortoso.com>
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

#include <locale.h>
#include <grilo.h>
#include <string.h>

#define TEST_URL_XML_PARSER "http://xml.parser.test/lua-factory/"

#define LUA_FACTORY_ID "grl-lua-factory"
#define FAKE_SOURCE_ID "test-source-xml-parser"

static void
test_lua_factory_setup (GrlConfig *config)
{
  GrlRegistry *registry;
  GError *error = NULL;

  registry = grl_registry_get_default ();

  if (config != NULL) {
    grl_registry_add_config (registry, config, &error);
    g_assert_no_error (error);
  }

  grl_registry_load_all_plugins (registry, FALSE, NULL);
  grl_registry_activate_plugin_by_id (registry, LUA_FACTORY_ID, &error);
  g_assert_no_error (error);
}

static GrlSource*
test_lua_factory_get_source (gchar *source_id,
                             GrlSupportedOps source_ops)
{
  GrlRegistry *registry = grl_registry_get_default ();
  GrlSource *source = grl_registry_lookup_source (registry, source_id);
  g_assert_nonnull (source);
  g_assert (grl_source_supported_operations (source) & source_ops);
  return source;
}

static void
test_lua_factory_shutdown (void)
{
  GrlRegistry *registry;
  GError *error = NULL;

  registry = grl_registry_get_default ();
  grl_registry_unload_plugin (registry, LUA_FACTORY_ID, &error);
  g_assert_no_error (error);
}

static GrlMedia *
get_media (GrlSource *source,
           GrlMedia *media)
{
  GrlOperationOptions *options;
  GList *keys;

  /* Keys can't be NULL, by default we ask for title */
  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_TITLE, NULL);

  options = grl_operation_options_new (NULL);
  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_NORMAL);

  grl_source_resolve_sync (source, media, keys, options, NULL);

  g_list_free (keys);
  g_object_unref (options);
  return media;
}

static void
resolve_fake_src (const gchar *xml_url,
                  const gchar *lua_url)
{
  GrlSource *source = NULL;
  GrlMedia *media = NULL;

  source = test_lua_factory_get_source (FAKE_SOURCE_ID, GRL_OP_RESOLVE);
  g_assert_nonnull (source);

  media = grl_media_new ();
  grl_data_add_string (GRL_DATA (media), GRL_METADATA_KEY_URL, xml_url);
  grl_data_add_string (GRL_DATA (media), GRL_METADATA_KEY_URL, lua_url);
  media = get_media (source, media);
  g_assert_nonnull (media);
  g_object_unref (media);
}

static void
test_xml_parser (void)
{
  guint i;

  struct {
    gchar *xml_url;
    gchar *lua_url;
  } xml_tests[] = {
    { TEST_URL_XML_PARSER "simple.xml",
      TEST_URL_XML_PARSER "simple-table.lua" }
  };

  for (i = 0; i < G_N_ELEMENTS (xml_tests); i++) {
    resolve_fake_src (xml_tests[i].xml_url, xml_tests[i].lua_url);
  }
}

gint
main (gint argc, gchar **argv)
{
  setlocale (LC_ALL, "");

  g_setenv ("GRL_PLUGIN_PATH", LUA_FACTORY_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", LUA_FACTORY_ID, TRUE);
  g_setenv ("GRL_NET_MOCKED", LUA_FACTORY_DATA_PATH "config.ini", TRUE);
  g_setenv ("GRL_LUA_SOURCES_PATH", LUA_FACTORY_DATA_PATH, TRUE);

  grl_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

  test_lua_factory_setup (NULL);

  g_test_add_func ("/lua-factory/lua-library/xml-parser", test_xml_parser);

  gint result = g_test_run ();

  test_lua_factory_shutdown ();
  grl_deinit ();

  return result;
}
