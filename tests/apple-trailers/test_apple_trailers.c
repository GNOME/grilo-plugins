/*
 * Copyright (C) 2013 Igalia S.L.
 *
 * Author: Juan A. Suarez Romero <jasuarez@igalia.com>
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

#include <grilo.h>

#define APPLE_TRAILERS_ID "grl-apple-trailers"

static void
test_setup (void)
{
  GError *error = NULL;
  GrlRegistry *registry;

  registry = grl_registry_get_default ();
  grl_registry_load_all_plugins (registry, &error);
  g_assert_no_error (error);
}

static void
verify (GrlData *data,
        guint n)
{
  static GHashTable *expected[3] = { NULL };
  static gboolean initialized = FALSE;

  g_return_if_fail (n < 3);

  if (!initialized) {
    initialized = TRUE;

    expected[0] = g_hash_table_new (NULL, NULL);
    g_hash_table_insert (expected[0], GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ID), "7501");
    g_hash_table_insert (expected[0], GRLKEYID_TO_POINTER (GRL_METADATA_KEY_TITLE), "2 Guns");
    g_hash_table_insert (expected[0], GRLKEYID_TO_POINTER (GRL_METADATA_KEY_DURATION), GINT_TO_POINTER (11100));

    expected[1] = g_hash_table_new (NULL, NULL);
    g_hash_table_insert (expected[1], GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ID), "7863");
    g_hash_table_insert (expected[1], GRLKEYID_TO_POINTER (GRL_METADATA_KEY_TITLE), "20 Feet from Stardom");
    g_hash_table_insert (expected[1], GRLKEYID_TO_POINTER (GRL_METADATA_KEY_DURATION), GINT_TO_POINTER (7980));

    expected[2] = g_hash_table_new (NULL, NULL);
    g_hash_table_insert (expected[2], GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ID), "6466");
    g_hash_table_insert (expected[2], GRLKEYID_TO_POINTER (GRL_METADATA_KEY_TITLE), "300: Rise of an Empire");
    g_hash_table_insert (expected[2], GRLKEYID_TO_POINTER (GRL_METADATA_KEY_DURATION), GINT_TO_POINTER (9000));
  }

  g_assert_cmpstr (grl_data_get_string (data, GRL_METADATA_KEY_ID),
                   ==,
                   g_hash_table_lookup (expected[n], GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ID)));
  g_assert_cmpstr (grl_data_get_string (data, GRL_METADATA_KEY_TITLE),
                   ==,
                   g_hash_table_lookup (expected[n], GRLKEYID_TO_POINTER (GRL_METADATA_KEY_TITLE)));
  g_assert_cmpint (grl_data_get_int (data, GRL_METADATA_KEY_DURATION),
                   ==,
                   GPOINTER_TO_INT (g_hash_table_lookup (expected[n], GRLKEYID_TO_POINTER (GRL_METADATA_KEY_DURATION))));
}

static void
test_browse_count (void)
{
  GError *error = NULL;
  GList *medias;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, "grl-apple-trailers");
  g_assert (source);
  options = grl_operation_options_new (NULL);
  grl_operation_options_set_count (options, 2);
  g_assert (options);

  medias = grl_source_browse_sync (source,
                                   NULL,
                                   grl_source_supported_keys (source),
                                   options,
                                   &error);
  g_assert_cmpint (g_list_length(medias), ==, 2);
  g_assert_no_error (error);

  verify (GRL_DATA (g_list_nth_data (medias, 0)), 0);
  verify (GRL_DATA (g_list_nth_data (medias, 1)), 1);

  g_list_free_full (medias, g_object_unref);
  g_object_unref (options);
}

static void
test_browse_skip (void)
{
  GError *error = NULL;
  GList *medias;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, "grl-apple-trailers");
  g_assert (source);
  options = grl_operation_options_new (NULL);
  grl_operation_options_set_count (options, 2);
  grl_operation_options_set_skip (options, 1);
  g_assert (options);

  medias = grl_source_browse_sync (source,
                                   NULL,
                                   grl_source_supported_keys (source),
                                   options,
                                   &error);
  g_assert_cmpint (g_list_length(medias), ==, 2);
  g_assert_no_error (error);

  verify (GRL_DATA (g_list_nth_data (medias, 0)), 1);
  verify (GRL_DATA (g_list_nth_data (medias, 1)), 2);

  g_list_free_full (medias, g_object_unref);
  g_object_unref (options);
}

int
main(int argc, char **argv)
{
  g_setenv ("GRL_PLUGIN_PATH", APPLE_TRAILERS_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", APPLE_TRAILERS_ID, TRUE);
  g_setenv ("GRL_NET_MOCKED", APPLE_TRAILERS_DATA_PATH "network-data.ini", TRUE);

  grl_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

#if !GLIB_CHECK_VERSION(2,32,0)
  g_thread_init (NULL);
#endif

  test_setup ();

  g_test_add_func ("/apple-trailers/browse/count", test_browse_count);
  g_test_add_func ("/apple-trailers/browse/skip", test_browse_skip);

  return g_test_run ();
}
