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

#ifndef _GRL_DLEYNA_TEST_UTILS_H_
#define _GRL_DLEYNA_TEST_UTILS_H_

#include <gio/gio.h>
#include <grilo.h>

#define DLEYNA_PLUGIN_ID "grl-dleyna"

typedef struct {
  GTestDBus       *dbus;
  GDBusConnection *connection;
  GrlRegistry     *registry;
  GMainLoop       *main_loop;
  guint            timeout_id;

  GPtrArray       *results; /* used to store results from callbacks */
} TestDleynaFixture;

void     test_dleyna_setup                 (TestDleynaFixture *fixture,
                                            gconstpointer      user_data);

void     test_dleyna_shutdown              (TestDleynaFixture *fixture,
                                            gconstpointer      user_data);

void     test_dleyna_main_loop_run         (TestDleynaFixture *fixture,
                                            gint               seconds);

void     test_dleyna_main_loop_quit        (TestDleynaFixture *fixture);

void     test_dleyna_add_server            (TestDleynaFixture *fixture);

void     test_dleyna_drop_server           (TestDleynaFixture *fixture,
                                            gchar             *object_path);
void     test_dleyna_queue_changes         (TestDleynaFixture *fixture,
                                            gchar             *device_path,
                                            gboolean           enabled,
                                            gboolean           detailed);
void     test_dleyna_flush_changes         (TestDleynaFixture *fixture,
                                            gchar             *device_path);

#endif /* _GRL_DLEYNA_TEST_UTILS_H_ */
