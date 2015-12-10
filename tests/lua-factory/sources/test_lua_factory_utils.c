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

#include "test_lua_factory_utils.h"

void
test_lua_factory_init (gint *p_argc,
                       gchar ***p_argv,
                       gboolean net_mocked)
{
  g_setenv ("GRL_PLUGIN_PATH", LUA_FACTORY_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", LUA_FACTORY_ID, TRUE);
  g_setenv ("GRL_LUA_SOURCES_PATH", LUA_FACTORY_SOURCES_PATH, TRUE);

  /* For some sources it doesn't make sense to mock the network request,
   * mainly because is hard to track changes in the provider of data.
   * (e.g. if source rely on a website) */
  if (net_mocked) {
    g_setenv ("GRL_NET_MOCKED", LUA_FACTORY_PLUGIN_TEST_DATA_PATH "config.ini", TRUE);
  }

  grl_init (p_argc, p_argv);
  g_test_init (p_argc, p_argv, NULL);
}

void
test_lua_factory_setup (GrlConfig *config)
{
  GrlRegistry *registry;
  GError *error = NULL;

  registry = grl_registry_get_default ();

  if (config != NULL) {
    grl_registry_add_config (registry, config, &error);
    g_assert_no_error (error);
  }

  grl_registry_load_plugin (registry, LUA_FACTORY_PLUGIN_PATH "/libgrlluafactory.so", &error);
  g_assert_no_error (error);
  grl_registry_activate_plugin_by_id (registry, LUA_FACTORY_ID, &error);
  g_assert_no_error (error);
}

GrlSource*
test_lua_factory_get_source (gchar *source_id,
                             GrlSupportedOps source_ops)
{
  GrlRegistry *registry = grl_registry_get_default ();
  GrlSource *source = grl_registry_lookup_source (registry, source_id);
  g_assert_nonnull (source);
  g_assert (grl_source_supported_operations (source) & source_ops);
  return source;
}

void
test_lua_factory_shutdown (void)
{
  GrlRegistry *registry;
  GError *error = NULL;

  registry = grl_registry_get_default ();
  grl_registry_unload_plugin (registry, LUA_FACTORY_ID, &error);
  g_assert_no_error (error);
}

void
test_lua_factory_deinit (void)
{
  grl_deinit ();
}

