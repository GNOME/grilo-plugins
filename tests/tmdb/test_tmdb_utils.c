/*
 * Copyright (C) 2012 Openismus GmbH
 *
 * Author: Jens Georg <jensg@openismus.com>
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

#include "test_tmdb_utils.h"
#include <math.h>
#include <float.h>

#define TMDB_PLUGIN_ID "grl-tmdb"
#define TEST_PATH "data/"

GrlSource *source = NULL;

void
test_setup_tmdb (void)
{
  GrlConfig *config;
  GrlRegistry *registry;
  GError *error = NULL;

  config = grl_config_new (TMDB_PLUGIN_ID, NULL);
  /* It does not matter what we set this to. It just needs to be non-empty because we're
   * going to fake the network responses. */
  grl_config_set_api_key (config, "TMDB_TEST_API_KEY");

  registry = grl_registry_get_default ();
  grl_registry_add_config (registry, config, &error);
  g_assert_no_error (error);

  grl_registry_load_plugin (registry,
                            GRILO_PLUGINS_TESTS_TMDB_PLUGIN_PATH "/libgrltmdb.so",
                            &error);
  g_assert_no_error (error);
  grl_registry_activate_plugin_by_id (registry, TMDB_PLUGIN_ID, &error);
  g_assert_no_error (error);

  source = GRL_SOURCE (grl_registry_lookup_source (registry, TMDB_PLUGIN_ID));
  g_assert (source != NULL);

  g_assert (grl_source_supported_operations (source) & GRL_OP_RESOLVE);
}

GrlSource* test_get_source (void)
{
  return source;
}

void
test_shutdown_tmdb (void)
{
  GrlRegistry *registry;
  GError *error = NULL;

  registry = grl_registry_get_default ();
  grl_registry_unload_plugin (registry, TMDB_PLUGIN_ID, &error);
  g_assert_no_error (error);
}
