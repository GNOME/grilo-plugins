/*
 * Copyright © 2013 Intel Corporation. All rights reserved.
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "test_dleyna_utils.h"

void
test_dleyna_setup (TestDleynaFixture *fixture,
                   gconstpointer      user_data)
{
  GError *error = NULL;

  fixture->main_loop = g_main_loop_new (NULL, FALSE);

  fixture->dbus = g_test_dbus_new (G_TEST_DBUS_NONE);
  g_test_dbus_add_service_dir (fixture->dbus, GRILO_PLUGINS_TESTS_DLEYNA_SERVICES_PATH);
  g_test_dbus_up (fixture->dbus);

  fixture->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);

  fixture->registry = grl_registry_get_default ();
  grl_registry_load_plugin (fixture->registry, GRILO_PLUGINS_TESTS_DLEYNA_PLUGIN_PATH "/libgrldleyna.so", &error);
  g_assert_no_error (error);
  grl_registry_activate_plugin_by_id (fixture->registry, DLEYNA_PLUGIN_ID, &error);
  g_assert_no_error (error);

  /* Do not print warning messages, we want to test error conditions without
   * generating too much noise. */
  if (g_getenv ("GRL_DEBUG") == NULL) {
    grl_log_configure ("dleyna:error");
  }
}

void
test_dleyna_shutdown (TestDleynaFixture *fixture,
                      gconstpointer      user_data)
{
  GError *error = NULL;

  g_clear_pointer (&fixture->main_loop, g_main_loop_unref);
  g_clear_pointer (&fixture->results, g_ptr_array_unref);

  grl_registry_unload_plugin (fixture->registry, DLEYNA_PLUGIN_ID, &error);
  g_assert_no_error (error);

  g_signal_handlers_disconnect_by_data (fixture->registry, fixture);

  g_clear_object (&fixture->connection);

  g_test_dbus_down (fixture->dbus);
  g_clear_object (&fixture->dbus);
}

static gboolean
timeout (gpointer user_data)
{
    g_assert_not_reached ();
}

void
test_dleyna_main_loop_run (TestDleynaFixture *fixture,
                           gint seconds)
{
  fixture->timeout_id = g_timeout_add_seconds (5, timeout, NULL);
  g_main_loop_run (fixture->main_loop);
}

void
test_dleyna_main_loop_quit (TestDleynaFixture *fixture)
{
  if (fixture->timeout_id != 0) {
    g_source_remove (fixture->timeout_id);
    fixture->timeout_id = 0;
  }
  g_main_loop_quit (fixture->main_loop);
}

void
test_dleyna_add_server (TestDleynaFixture *fixture)
{
  GError *error = NULL;

  g_dbus_connection_call_sync (fixture->connection, "com.intel.dleyna-server", "/com/intel/dLeynaServer",
                               "org.freedesktop.DBus.Mock", "AddServer", NULL, NULL,
                               G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
  g_assert_no_error (error);
}

void
test_dleyna_drop_server (TestDleynaFixture *fixture,
                         gchar             *object_path)
{
  GError *error = NULL;

  g_dbus_connection_call_sync (fixture->connection, "com.intel.dleyna-server", "/com/intel/dLeynaServer",
                               "org.freedesktop.DBus.Mock", "DropServer", g_variant_new ("(s)", object_path),
                               NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
  g_assert_no_error (error);
}

void
test_dleyna_queue_changes (TestDleynaFixture *fixture,
                           gchar             *device_path,
                           gboolean           enabled,
                           gboolean           detailed)
{
  GVariant *params;
  GError *error = NULL;

  params = g_variant_new ("(bb)", enabled, detailed);
  g_dbus_connection_call_sync (fixture->connection, "com.intel.dleyna-server", device_path,
                               "org.freedesktop.DBus.Mock", "QueueChanges", params, NULL,
                               G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
  g_assert_no_error (error);
}

void
test_dleyna_flush_changes (TestDleynaFixture *fixture,
                           gchar             *device_path)
{
  GError *error = NULL;

  g_dbus_connection_call_sync (fixture->connection, "com.intel.dleyna-server", device_path,
                               "org.freedesktop.DBus.Mock", "FlushChanges", NULL, NULL,
                               G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
  g_assert_no_error (error);
}
