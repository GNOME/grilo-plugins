/*
 * Copyright (C) 2016. All rights reserved.
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
#define LOCAL_CONTENT       TEST_URL_XML_PARSER "simple.xml"

#define LUA_FACTORY_ID "grl-lua-factory"
#define FAKE_SOURCE_ID "test-source-lua-errors"

#define TEST_NOT_CALLBACK_SIMPLE      "test-not-calback-simple"
#define TEST_NOT_CALLBACK_ASYNC       "test-not-callback-on-async"
#define TEST_CALLBACK_ON_FINISHED_OP  "test-callback-after-finished"
#define TEST_MULTIPLE_FETCH           "multiple-fetch-with-callback"

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

static void
set_test_data (GrlSource **source,
               GrlMedia **media,
               GrlOperationOptions **options,
               GList **keys,
               GrlSupportedOps source_op)
{
  *source = test_lua_factory_get_source (FAKE_SOURCE_ID, source_op);
  g_assert_nonnull (*source);

  *media = grl_media_new ();
  grl_data_add_string (GRL_DATA (*media), GRL_METADATA_KEY_URL, LOCAL_CONTENT);

  *keys = grl_metadata_key_list_new (GRL_METADATA_KEY_TITLE, NULL);
  *options = grl_operation_options_new (NULL);
  grl_operation_options_set_resolution_flags (*options, GRL_RESOLVE_NORMAL);
}

static void
free_test_data (GrlMedia **media,
                GrlOperationOptions **options,
                GList **keys)
{
  g_object_unref (*media);
  g_list_free (*keys);
  g_object_unref (*options);
}

static void
execute_resolve_test (const gchar *test_id)
{
  GrlSource *source;
  GrlMedia *media;
  GrlOperationOptions *options;
  GList *keys;

  set_test_data (&source, &media, &options, &keys, GRL_OP_RESOLVE);
  grl_media_set_id (media, test_id);
  grl_source_resolve_sync (source, media, keys, options, NULL);
  free_test_data (&media, &options, &keys);
}

static void
execute_search_test (const gchar *test_id)
{
  GrlSource *source;
  GrlMedia *media;
  GrlOperationOptions *options;
  GList *keys, *list_medias;

  set_test_data (&source, &media, &options, &keys, GRL_OP_RESOLVE);
  list_medias = grl_source_search_sync (source, test_id, keys, options, NULL);
  g_list_free_full (list_medias, g_object_unref);
  free_test_data (&media, &options, &keys);
}

static void
test_correct_state_on_multiple_fetch (void)
{
  execute_search_test (TEST_MULTIPLE_FETCH);
}

static void
test_callback_after_end_of_operation (void)
{
  g_test_expect_message("Grilo", G_LOG_LEVEL_WARNING, "*Can't retrieve current operation*");
  execute_resolve_test (TEST_CALLBACK_ON_FINISHED_OP);
}

static void
test_not_callback_on_async (void)
{
  g_test_expect_message("Grilo", G_LOG_LEVEL_WARNING,
        "*Source 'test-source-lua-errors' is broken, as the finishing callback was not called for*");
  execute_resolve_test (TEST_NOT_CALLBACK_ASYNC);
}

static void
test_not_callback_simple (void)
{
  g_test_expect_message("Grilo", G_LOG_LEVEL_WARNING,
        "*Source 'test-source-lua-errors' is broken, as the finishing callback was not called for*");
  execute_resolve_test (TEST_NOT_CALLBACK_SIMPLE);
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

  g_test_add_func ("/lua-factory/lua-library/not-callback-simple", test_not_callback_simple);
  g_test_add_func ("/lua-factory/lua-library/not-callback-after-async", test_not_callback_on_async);
  g_test_add_func ("/lua-factory/lua-library/callback-after-end", test_callback_after_end_of_operation);
  g_test_add_func ("/lua-factory/lua-library/multiple-fetch", test_correct_state_on_multiple_fetch);

  gint result = g_test_run ();

  test_lua_factory_shutdown ();
  grl_deinit ();

  return result;
}
