/*
 * Copyright (C) 2014. All rights reserved.
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

#include <glib/gstdio.h>
#include <grilo.h>
#include "test_thetvdb_utils.h"

GrlSource *source = NULL;
gchar *tmp_dir = NULL;

void
test_setup_thetvdb (void)
{
  GrlConfig *config;
  GrlRegistry *registry;
  GError *error = NULL;

  if (tmp_dir == NULL) {
    /* Only create tmp dir and set the XDG_DATA_HOME once */
    tmp_dir = g_build_filename (g_get_tmp_dir (), "test-thetvdb-XXXXXX", NULL);
    tmp_dir = g_mkdtemp (tmp_dir);
    g_assert_nonnull (tmp_dir);

    g_setenv ("XDG_DATA_HOME", tmp_dir, TRUE);
  }

  config = grl_config_new (THETVDB_ID, NULL);
  grl_config_set_api_key (config, "THETVDB_TEST_MOCK_API_KEY");

  registry = grl_registry_get_default ();
  grl_registry_add_config (registry, config, &error);
  g_assert_no_error (error);

  grl_registry_load_plugin (registry,
                            THETVDB_PLUGIN_PATH "/libgrlthetvdb.so",
                            &error);
  g_assert_no_error (error);
  grl_registry_activate_plugin_by_id (registry, THETVDB_ID, &error);
  g_assert_no_error (error);

  source = GRL_SOURCE (grl_registry_lookup_source (registry, THETVDB_ID));
  g_assert (source != NULL);

  g_assert (grl_source_supported_operations (source) & GRL_OP_RESOLVE);
}

GrlSource* test_get_source (void)
{
  return source;
}

static void
test_unload (void)
{
  GrlRegistry *registry;
  GError *error = NULL;
  gchar *db_path;

  registry = grl_registry_get_default ();
  grl_registry_unload_plugin (registry, THETVDB_ID, &error);
  g_assert_no_error (error);

  /* Remove grl-thetvdb database to avoid unecessary grow of tmpdir */
  db_path = g_build_filename (tmp_dir, "grilo-plugins", "grl-thetvdb.db", NULL);
  g_remove (db_path);
  g_free (db_path);
}

void
test_reset_thetvdb (void)
{
  test_unload ();
  test_setup_thetvdb ();
}

void
test_shutdown_thetvdb (void)
{
  test_unload ();
  g_clear_pointer (&tmp_dir, g_free);
}
