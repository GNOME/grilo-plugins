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

#include <locale.h>
#include "test_dleyna_utils.h"

typedef struct {
  GPtrArray          *changed_medias;
  GrlSourceChangeType change_type;
  gboolean            location_unknown;
} ChangedContentData;

static ChangedContentData *
changed_content_data_new (GPtrArray          *changed_medias,
                          GrlSourceChangeType change_type,
                          gboolean            location_unknown)
{
  ChangedContentData *ccd = g_new0 (ChangedContentData, 1);
  ccd->changed_medias = g_ptr_array_ref (changed_medias);
  ccd->change_type = change_type;
  ccd->location_unknown = location_unknown;
  return ccd;
}

static void
changed_content_data_free (gpointer data)
{
  ChangedContentData *ccd = data;
  g_ptr_array_unref (ccd->changed_medias);
  g_free (ccd);
}

static void
main_loop_quit_on_source_cb (GrlRegistry       *registry,
                             GrlSource         *source,
                             TestDleynaFixture *fixture)
{
  g_assert (source != NULL);
  g_assert (GRL_IS_SOURCE (source));

  test_dleyna_main_loop_quit(fixture);
}

static void
assert_source (TestDleynaFixture *fixture,
               gchar             *id,
               gchar             *name)
{
  GrlSource *source;

  source = grl_registry_lookup_source (fixture->registry, id);

  g_assert (source != NULL);
  g_assert (GRL_IS_SOURCE (source));
  g_assert_cmpstr (grl_source_get_name (GRL_SOURCE (source)), ==, name);
}

/**
 * test_discovery:
 *
 * Test that sources are correctly added/removed when DLNA servers are detected.
 */
static void
test_discovery (TestDleynaFixture *fixture,
                gconstpointer      user_data)
{
  g_signal_connect (fixture->registry, "source-added", G_CALLBACK (main_loop_quit_on_source_cb), fixture);
  g_signal_connect (fixture->registry, "source-removed", G_CALLBACK (main_loop_quit_on_source_cb), fixture);

  test_dleyna_add_server (fixture);
  test_dleyna_main_loop_run (fixture, 5);
  assert_source (fixture, "grl-dleyna-c50bf388-042a-5326-af4b-6969e1bbc860", "Mock Server <#0>");

  test_dleyna_add_server (fixture);
  test_dleyna_main_loop_run (fixture, 5);
  assert_source (fixture, "grl-dleyna-bcc9541d-7ccb-5fa7-93bc-0fe9038bc333", "Mock Server <#1>");

  test_dleyna_drop_server (fixture, "/com/intel/dLeynaServer/server/1");
  test_dleyna_main_loop_run (fixture, 5);
  g_assert (grl_registry_lookup_source (fixture->registry, "grl-dleyna-bcc9541d-7ccb-5fa7-93bc-0fe9038bc333") == NULL);
  g_assert (grl_registry_lookup_source (fixture->registry, "grl-dleyna-c50bf388-042a-5326-af4b-6969e1bbc860") != NULL);

  test_dleyna_drop_server (fixture, "/com/intel/dLeynaServer/server/0");
  test_dleyna_main_loop_run (fixture, 5);
  g_assert (grl_registry_lookup_source (fixture->registry, "grl-dleyna-c50bf388-042a-5326-af4b-6969e1bbc860") == NULL);
}

static void
test_browse (TestDleynaFixture *fixture,
             gconstpointer      user_data)
{
  GrlSource *source;
  GrlOperationOptions *options;
  GrlCaps *caps;
  GrlMedia *media, *container;
  GList *keys, *results;
  GError *error = NULL;

  g_signal_connect (fixture->registry, "source-added", G_CALLBACK (main_loop_quit_on_source_cb), fixture);
  test_dleyna_add_server (fixture);
  test_dleyna_main_loop_run (fixture, 5);

  source = grl_registry_lookup_source (fixture->registry, "grl-dleyna-c50bf388-042a-5326-af4b-6969e1bbc860");

  caps = grl_source_get_caps (source, GRL_OP_BROWSE);
  g_assert (grl_caps_get_type_filter (caps) & GRL_TYPE_FILTER_VIDEO);
  g_assert (grl_caps_get_type_filter (caps) & GRL_TYPE_FILTER_AUDIO);
  g_assert (grl_caps_get_type_filter (caps) & GRL_TYPE_FILTER_IMAGE);

  /* Try with default options and no keys */
  options = grl_operation_options_new (NULL);
  results = grl_source_browse_sync (source, NULL, NULL, options, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_list_length (results), ==, 4);
  g_list_free_full (results, g_object_unref);

  /* Ask for title, URL and child count */
  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_TITLE, GRL_METADATA_KEY_URL, GRL_METADATA_KEY_CHILDCOUNT, NULL);
  results = grl_source_browse_sync (source, NULL, keys, options, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_list_length (results), ==, 4);
  media = GRL_MEDIA (results->data);
  g_assert (grl_media_is_container (media));
  g_assert_cmpstr (grl_media_get_id (media), ==, "dleyna:/com/intel/dLeynaServer/server/0");
  g_assert_cmpstr (grl_media_get_title (media), ==, "The Root");
  g_assert_cmpstr (grl_media_get_url (media), ==, "http://127.0.0.1:4242/root/DIDL_S.xml");
  g_assert_cmpint (grl_media_get_childcount (media), ==, 3);
  media = GRL_MEDIA (results->next->next->next->data);
  g_assert (grl_media_is_container (media));
  g_assert_cmpstr (grl_media_get_id (media), ==, "dleyna:/com/intel/dLeynaServer/server/0/3");
  g_assert_cmpstr (grl_media_get_title (media), ==, "Stuff");
  g_assert_cmpstr (grl_media_get_url (media), ==, "http://127.0.0.1:4242/stuff/DIDL_S.xml");
  g_assert_cmpint (grl_media_get_childcount (media), ==, 5);
  container = g_object_ref (media); /* Keep a ref for the subsequent test */
  g_list_free_full (results, g_object_unref);

  /* Try from a container */
  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_TITLE, GRL_METADATA_KEY_URL, GRL_METADATA_KEY_CHILDCOUNT, NULL);
  results = grl_source_browse_sync (source, container, keys, options, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_list_length (results), ==, 5);
  media = GRL_MEDIA (results->next->next->data);
  g_assert (grl_media_is_image (media));
  g_assert_cmpstr (grl_media_get_id (media), ==, "dleyna:/com/intel/dLeynaServer/server/0/33");
  g_assert_cmpstr (grl_media_get_title (media), ==, "A picture.jpg");
  g_assert_cmpstr (grl_media_get_url (media), ==, "http://127.0.0.1:4242/stuff/picture.jpg");
  g_list_free (keys);
  g_object_unref (container);
  g_list_free_full (results, g_object_unref);

  g_object_unref (caps);
  g_object_unref (options);
}

static void
test_store (TestDleynaFixture *fixture,
            gconstpointer      user_data)
{
  GrlSource *source;
  GrlMedia *media, *container;
  GError *error = NULL;

  g_signal_connect (fixture->registry, "source-added", G_CALLBACK (main_loop_quit_on_source_cb), fixture);
  test_dleyna_add_server (fixture);
  test_dleyna_main_loop_run (fixture, 5);

  source = grl_registry_lookup_source (fixture->registry, "grl-dleyna-c50bf388-042a-5326-af4b-6969e1bbc860");

  media = grl_media_new ();
  grl_media_set_url (media, "file://" GRILO_PLUGINS_TESTS_DLEYNA_DATA_PATH "/helloworld.txt");

  /* Let the DMS choose the right container */
  grl_source_store_sync (source, NULL, media, GRL_WRITE_NORMAL, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (grl_media_get_id (media), ==, "dleyna:/com/intel/dLeynaServer/server/0/any000");

  /* Try again explicitly choosing a container */
  container = grl_media_container_new ();
  grl_media_set_id (container, "dleyna:/com/intel/dLeynaServer/server/0/3");

  grl_source_store_sync (source, container, media, GRL_WRITE_NORMAL, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (grl_media_get_id (media), ==, "dleyna:/com/intel/dLeynaServer/server/0/3/up000");

  g_object_unref (media);

  /* Create a container, letting the DMS to choose the parent */
  media = GRL_MEDIA (grl_media_container_new ());
  grl_media_set_title (media, "New container");

  grl_source_store_sync (source, NULL, media, GRL_WRITE_NORMAL, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (grl_media_get_id (media), ==, "dleyna:/com/intel/dLeynaServer/server/0/any001");

  /* Again, but explictly choosing the parent */
  grl_source_store_sync (source, container, media, GRL_WRITE_NORMAL, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (grl_media_get_id (media), ==, "dleyna:/com/intel/dLeynaServer/server/0/3/up001");

  g_object_unref (media);
  g_object_unref (container);
}

static void
test_store_metadata (TestDleynaFixture *fixture,
                     gconstpointer      user_data)
{
  GrlSource *source;
  GrlMedia *media, *container;
  GList *keys;
  GError *error = NULL;

  g_signal_connect (fixture->registry, "source-added", G_CALLBACK (main_loop_quit_on_source_cb), fixture);
  test_dleyna_add_server (fixture);
  test_dleyna_main_loop_run (fixture, 5);

  source = grl_registry_lookup_source (fixture->registry, "grl-dleyna-c50bf388-042a-5326-af4b-6969e1bbc860");

  media = grl_media_new ();
  grl_media_set_url (media, "file://" GRILO_PLUGINS_TESTS_DLEYNA_DATA_PATH "/helloworld.txt");
  grl_media_set_author (media, "Tizio Caio Sempronio");

  container = grl_media_container_new ();
  grl_media_set_id (container, "dleyna:/com/intel/dLeynaServer/server/0/3");

  grl_source_store_sync (source, container, media, GRL_WRITE_FULL, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (grl_media_get_id (media), ==, "dleyna:/com/intel/dLeynaServer/server/0/3/up000");

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_TITLE, NULL);
  grl_media_set_title (media, "Hello World");
  grl_source_store_metadata_sync (source, media, keys, GRL_WRITE_NORMAL, &error);
  g_assert_no_error (error);
  g_list_free (keys);

  g_object_unref (media);
  g_object_unref (container);
}

static void
test_resolve (TestDleynaFixture *fixture,
              gconstpointer      user_data)
{
  GrlSource *source;
  GrlOperationOptions *options;
  GrlMedia *media;
  GList *keys;
  GError *error = NULL;

  g_signal_connect (fixture->registry, "source-added", G_CALLBACK (main_loop_quit_on_source_cb), fixture);
  test_dleyna_add_server (fixture);
  test_dleyna_main_loop_run (fixture, 5);

  source = grl_registry_lookup_source (fixture->registry, "grl-dleyna-c50bf388-042a-5326-af4b-6969e1bbc860");

  options = grl_operation_options_new (NULL);
  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID, GRL_METADATA_KEY_TITLE, GRL_METADATA_KEY_URL, GRL_METADATA_KEY_CHILDCOUNT, NULL);

  media = grl_media_new ();
  grl_source_resolve_sync (source, media, keys, options, &error);
  g_assert_no_error (error);

  g_assert_cmpstr (grl_media_get_id (media), ==, "dleyna:/com/intel/dLeynaServer/server/0");
  g_assert_cmpstr (grl_media_get_title (media), ==, "The Root");
  g_assert_cmpstr (grl_media_get_url (media), ==, "http://127.0.0.1:4242/root/DIDL_S.xml");
  g_object_unref (media);

  media = grl_media_new ();
  grl_media_set_id (media, "dleyna:/com/intel/dLeynaServer/server/0/33");

  grl_source_resolve_sync (source, media, keys, options, &error);
  g_assert_no_error (error);

  g_assert_cmpstr (grl_media_get_title (media), ==, "A picture.jpg");
  g_assert_cmpstr (grl_media_get_url (media), ==, "http://127.0.0.1:4242/stuff/picture.jpg");
  g_object_unref (media);

  media = grl_media_new ();
  grl_media_set_id (media, "dleyna:/com/intel/dLeynaServer/server/0/does_not_exists");

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_TITLE, GRL_METADATA_KEY_URL, GRL_METADATA_KEY_CHILDCOUNT, NULL);
  grl_source_resolve_sync (source, media, keys, options, &error);
  g_assert_error (error, GRL_CORE_ERROR, GRL_CORE_ERROR_RESOLVE_FAILED);

  g_object_unref (media);
  g_list_free (keys);
}

static void
test_remove (TestDleynaFixture *fixture,
             gconstpointer      user_data)
{
  GrlSource *source;
  GrlMedia *media;
  GError *error = NULL;

  g_signal_connect (fixture->registry, "source-added", G_CALLBACK (main_loop_quit_on_source_cb), fixture);
  test_dleyna_add_server (fixture);
  test_dleyna_main_loop_run (fixture, 5);

  source = grl_registry_lookup_source (fixture->registry, "grl-dleyna-c50bf388-042a-5326-af4b-6969e1bbc860");

  media = grl_media_new ();
  grl_media_set_id (media, "dleyna:/com/intel/dLeynaServer/server/0/33");

  grl_source_remove_sync (source, media, &error);
  g_assert_no_error (error);

  g_object_unref (media);
}

static void
main_loop_quit_on_content_changed_cb (GrlSource          *source,
                                      GPtrArray          *changed_medias,
                                      GrlSourceChangeType change_type,
                                      gboolean            location_unknown,
                                      TestDleynaFixture  *fixture)
{
  ChangedContentData *ccd;

  ccd = changed_content_data_new (changed_medias, change_type, location_unknown);
  g_ptr_array_add (fixture->results, ccd);

  test_dleyna_main_loop_quit(fixture);
}

static void
test_notifications (TestDleynaFixture *fixture,
                    gconstpointer      user_data)
{
  GrlSource *source;
  GrlMedia *media, *container;
  GList *updated_keys, *failing_keys;
  ChangedContentData *ccd;
  GError *error = NULL;

  g_signal_connect (fixture->registry, "source-added", G_CALLBACK (main_loop_quit_on_source_cb), fixture);
  fixture->results = g_ptr_array_new_with_free_func (changed_content_data_free);

  test_dleyna_add_server (fixture);
  test_dleyna_main_loop_run (fixture, 5);

  source = grl_registry_lookup_source (fixture->registry, "grl-dleyna-c50bf388-042a-5326-af4b-6969e1bbc860");

  test_dleyna_queue_changes (fixture, "/com/intel/dLeynaServer/server/0", TRUE, TRUE);
  grl_source_notify_change_start (source, &error);
  g_assert_no_error (error);

  g_signal_connect (source, "content-changed", G_CALLBACK (main_loop_quit_on_content_changed_cb), fixture);

  media = grl_media_new ();
  grl_media_set_id (media, "dleyna:/com/intel/dLeynaServer/server/0/33");

  grl_source_remove_sync (source, media, &error);
  g_assert_no_error (error);
  g_object_unref (media);

  media = grl_media_new ();
  grl_media_set_url (media, "file://" GRILO_PLUGINS_TESTS_DLEYNA_DATA_PATH "/helloworld.txt");

  container = grl_media_container_new ();
  grl_media_set_id (container, "dleyna:/com/intel/dLeynaServer/server/0/32");

  grl_source_store_sync (source, container, media, GRL_WRITE_NORMAL, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (grl_media_get_id (media), ==, "dleyna:/com/intel/dLeynaServer/server/0/32/up000");
  grl_source_store_sync (source, container, media, GRL_WRITE_NORMAL, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (grl_media_get_id (media), ==, "dleyna:/com/intel/dLeynaServer/server/0/32/up001");

  grl_media_set_title (media, "helloworld-2.txt");
  updated_keys = grl_metadata_key_list_new (GRL_METADATA_KEY_TITLE, NULL);
  failing_keys = grl_source_store_metadata_sync (source, media, updated_keys, GRL_WRITE_NORMAL, &error);
  g_assert_no_error (error);
  g_list_free (updated_keys);
  g_assert (failing_keys == NULL);

  g_object_unref (media);
  g_object_unref (container);

  test_dleyna_flush_changes (fixture, "/com/intel/dLeynaServer/server/0");

  test_dleyna_main_loop_run (fixture, 5);

  g_assert_cmpint (fixture->results->len, ==, 3);

  ccd = g_ptr_array_index (fixture->results, 0);
  g_assert_cmpint (ccd->changed_medias->len, ==, 1);
  g_assert_cmpint (ccd->change_type, ==, GRL_CONTENT_REMOVED);
  g_assert (!ccd->location_unknown);
  media = g_ptr_array_index (ccd->changed_medias, 0);
  g_assert_cmpstr (grl_media_get_id (media), ==, "dleyna:/com/intel/dLeynaServer/server/0/33");

  ccd = g_ptr_array_index (fixture->results, 1);

  g_assert_cmpint (ccd->changed_medias->len, ==, 2);
  g_assert_cmpint (ccd->change_type, ==, GRL_CONTENT_ADDED);
  g_assert (!ccd->location_unknown);
  media = g_ptr_array_index (ccd->changed_medias, 0);
  g_assert_cmpstr (grl_media_get_id (media), ==, "dleyna:/com/intel/dLeynaServer/server/0/32/up000");
  media = g_ptr_array_index (ccd->changed_medias, 1);
  g_assert_cmpstr (grl_media_get_id (media), ==, "dleyna:/com/intel/dLeynaServer/server/0/32/up001");

  ccd = g_ptr_array_index (fixture->results, 2);

  g_assert_cmpint (ccd->changed_medias->len, ==, 1);
  g_assert_cmpint (ccd->change_type, ==, GRL_CONTENT_CHANGED);
  g_assert (!ccd->location_unknown);
  media = g_ptr_array_index (ccd->changed_medias, 0);
  g_assert_cmpstr (grl_media_get_id (media), ==, "dleyna:/com/intel/dLeynaServer/server/0/32/up001");

  grl_source_notify_change_stop (source, &error);
  g_assert_no_error (error);
}

int
main(int argc, char **argv)
{
  setlocale (LC_ALL, "");

  g_setenv ("GRL_PLUGIN_PATH", GRILO_PLUGINS_TESTS_DLEYNA_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", DLEYNA_PLUGIN_ID, TRUE);

  grl_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

  /* Since we want to test error conditions we have to make sure that
   * G_LOG_LEVEL_WARNING is not considered fatal. In test_dleyna_setup() we
   * also make sure that warning messages are not printed by default. */
  g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL | G_LOG_FATAL_MASK);

  g_test_add ("/dleyna/discovery", TestDleynaFixture, NULL,
      test_dleyna_setup, test_discovery, test_dleyna_shutdown);
  g_test_add ("/dleyna/browse", TestDleynaFixture, NULL,
      test_dleyna_setup, test_browse, test_dleyna_shutdown);
  g_test_add ("/dleyna/resolve", TestDleynaFixture, NULL,
      test_dleyna_setup, test_resolve, test_dleyna_shutdown);
  g_test_add ("/dleyna/store", TestDleynaFixture, NULL,
      test_dleyna_setup, test_store, test_dleyna_shutdown);
  g_test_add ("/dleyna/store-metadata", TestDleynaFixture, NULL,
      test_dleyna_setup, test_store_metadata, test_dleyna_shutdown);
  g_test_add ("/dleyna/remove", TestDleynaFixture, NULL,
      test_dleyna_setup, test_remove, test_dleyna_shutdown);
  g_test_add ("/dleyna/notifications", TestDleynaFixture, NULL,
      test_dleyna_setup, test_notifications, test_dleyna_shutdown);

  return g_test_run ();
}
