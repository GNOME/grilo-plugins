/*
 * Copyright (C) 2020 Red Hat Inc.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
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

#define TRACKER3_ID "grl-tracker3"
#define TRACKER3_SOURCE_ID "grl-tracker3-source"

const gchar *test_files[] = {
  "file://" TRACKER3_DATA_PATH "/sample.flac",
  "file://" TRACKER3_DATA_PATH "/sample.mp3",
  "file://" TRACKER3_DATA_PATH "/sample.ogv",
  "file://" TRACKER3_DATA_PATH "/sample.ogg",
  "file://" TRACKER3_DATA_PATH "/sample.png",
};

static void
content_changed_cb (GrlSource           *source,
		    GPtrArray           *medias,
		    GrlSourceChangeType  type,
		    gboolean             location_unknown,
                    GMainLoop           *main_loop)
{
  static gint n_changes = 0;
  guint i;

  for (i = 0; i < medias->len; i++) {
    GrlMedia *media = g_ptr_array_index (medias, i);

    g_assert_true (g_strv_contains ((const gchar * const *) test_files,
                                    grl_media_get_url (media)));
    n_changes++;
  }

  if (n_changes == G_N_ELEMENTS (test_files)) {
    g_signal_handlers_disconnect_by_func (source, content_changed_cb, main_loop);
    g_main_loop_quit (main_loop);
  }
}

static void
on_source_added (GrlRegistry *registry,
                 GrlSource   *source,
                 GMainLoop   *main_loop)
{
  GError *error = NULL;

  g_assert_cmpstr (grl_source_get_id (source), ==, TRACKER3_SOURCE_ID);

  grl_source_notify_change_start (source, &error);
  g_signal_connect (source, "content-changed", G_CALLBACK (content_changed_cb), main_loop);
  g_assert_no_error (error);

  /* Some silly query, we want to start the miner service */
  grl_source_test_media_from_uri (source, "file:///");
}

static void
test_setup (void)
{
  GError *error = NULL;
  GrlRegistry *registry;
  GMainLoop *main_loop;

  registry = grl_registry_get_default ();
  grl_registry_load_all_plugins (registry, TRUE, &error);
  g_assert_no_error (error);

  main_loop = g_main_loop_new (NULL, FALSE);

  g_signal_connect (registry, "source-added",
                    G_CALLBACK (on_source_added), main_loop);

  g_main_loop_run (main_loop);

  g_main_loop_unref (main_loop);
}

static gint
compare_by_url (gconstpointer a,
                gconstpointer b)
{
  return g_strcmp0 (grl_media_get_url (GRL_MEDIA (a)),
                    grl_media_get_url (GRL_MEDIA (b)));
}

static void
test_browse (void)
{
  GrlRegistry *registry;
  GrlSource *source;
  GList *subelements, *elements, *l;
  GError *error = NULL;
  GrlOperationOptions *options;
  GList *keys;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, TRACKER3_SOURCE_ID);
  g_assert (source);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_URL,
                                    GRL_METADATA_KEY_TITLE,
                                    GRL_METADATA_KEY_INVALID);

  options = grl_operation_options_new (NULL);
  elements = grl_source_browse_sync (source, NULL, NULL, options, &error);
  g_assert_no_error (error);

  g_assert_cmpint (g_list_length (elements), ==, 3);

  for (l = elements; l; l = l->next) {
    GrlMedia *media = l->data;

    if (g_strcmp0 (grl_media_get_title (media), "Music") == 0) {
      subelements = grl_source_browse_sync (source, media, keys, options, &error);
      g_assert_cmpint (g_list_length (subelements), ==, 3);
      subelements = g_list_sort (subelements, compare_by_url);

      g_assert_cmpstr (grl_media_get_url (g_list_nth_data (subelements, 0)),
                       ==,
                       "file://" TRACKER3_DATA_PATH "/sample.flac");
      g_assert_cmpstr (grl_media_get_url (g_list_nth_data (subelements, 1)),
                       ==,
                       "file://" TRACKER3_DATA_PATH "/sample.mp3");
      g_assert_cmpstr (grl_media_get_url (g_list_nth_data (subelements, 2)),
                       ==,
                       "file://" TRACKER3_DATA_PATH "/sample.ogg");
      g_list_free_full (subelements, g_object_unref);
    } else if (g_strcmp0 (grl_media_get_title (media), "Videos") == 0) {
      subelements = grl_source_browse_sync (source, media, keys, options, &error);
      g_assert_cmpint (g_list_length (subelements), ==, 1);
      g_assert_cmpstr (grl_media_get_url (g_list_nth_data (subelements, 0)),
                       ==,
                       "file://" TRACKER3_DATA_PATH "/sample.ogv");
      g_list_free_full (subelements, g_object_unref);
    } else if (g_strcmp0 (grl_media_get_title (media), "Photos") == 0) {
      subelements = grl_source_browse_sync (source, media, keys, options, &error);
      g_assert_cmpint (g_list_length (subelements), ==, 1);
      g_assert_cmpstr (grl_media_get_url (g_list_nth_data (subelements, 0)),
                       ==,
                       "file://" TRACKER3_DATA_PATH "/sample.png");
      g_list_free_full (subelements, g_object_unref);
    } else {
      g_assert_cmpstr (grl_media_get_title (media), ==, "b0rk");
    }
  }

  g_list_free_full (elements, g_object_unref);
  g_object_unref (options);
  g_list_free (keys);
}

static void
test_browse_type_filter (void)
{
  GrlRegistry *registry;
  GrlSource *source;
  GList *elements;
  GError *error = NULL;
  GrlOperationOptions *options;
  GList *keys;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, TRACKER3_SOURCE_ID);
  g_assert (source);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_URL,
                                    GRL_METADATA_KEY_TITLE,
                                    GRL_METADATA_KEY_INVALID);

  options = grl_operation_options_new (NULL);
  grl_operation_options_set_type_filter (options, GRL_TYPE_FILTER_VIDEO);
  elements = grl_source_browse_sync (source, NULL, keys, options, &error);
  g_assert_no_error (error);

  g_assert_cmpint (g_list_length (elements), ==, 1);

  elements = g_list_sort (elements, compare_by_url);

  g_assert_cmpstr (grl_media_get_url (g_list_nth_data (elements, 0)),
                   ==,
                   "file://" TRACKER3_DATA_PATH "/sample.ogv");

  g_list_free_full (elements, g_object_unref);
  g_object_unref (options);
  g_list_free (keys);
}

static void
test_media_from_uri (void)
{
  GrlRegistry *registry;
  GrlSource *source;
  GrlMedia *media;
  GError *error = NULL;
  GrlOperationOptions *options;
  GList *keys;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, TRACKER3_SOURCE_ID);
  g_assert (source);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_URL,
                                    GRL_METADATA_KEY_TITLE,
                                    GRL_METADATA_KEY_INVALID);

  options = grl_operation_options_new (NULL);
  media = grl_source_get_media_from_uri_sync (source,
                                              "file://" TRACKER3_DATA_PATH "/sample.ogv",
                                              keys,
                                              options, &error);
  g_assert_no_error (error);

  g_assert_cmpstr (grl_media_get_url (media),
                   ==,
                   "file://" TRACKER3_DATA_PATH "/sample.ogv");
  g_assert_cmpstr (grl_media_get_title (media),
                   ==,
                   "sample");

  g_object_unref (options);
  g_object_unref (media);
  g_list_free (keys);
}

static void
test_media_from_uri_nonexistent (void)
{
  GrlRegistry *registry;
  GrlSource *source;
  GrlMedia *media;
  GError *error = NULL;
  GrlOperationOptions *options;
  GList *keys;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, TRACKER3_SOURCE_ID);
  g_assert (source);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_URL,
                                    GRL_METADATA_KEY_TITLE,
                                    GRL_METADATA_KEY_INVALID);

  options = grl_operation_options_new (NULL);
  media = grl_source_get_media_from_uri_sync (source,
                                              "file://" TRACKER3_DATA_PATH "/IDoNotExist",
                                              keys,
                                              options, &error);
  g_assert_no_error (error);
  g_assert_null (media);

  g_object_unref (options);
  g_list_free (keys);
}

static void
test_resolve (void)
{
  GrlRegistry *registry;
  GrlSource *source;
  GrlMedia *media, *resolved;
  GError *error = NULL;
  GrlOperationOptions *options;
  GDateTime *datetime;
  GList *keys;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, TRACKER3_SOURCE_ID);
  g_assert (source);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_URL,
                                    GRL_METADATA_KEY_TITLE,
                                    GRL_METADATA_KEY_MODIFICATION_DATE,
                                    GRL_METADATA_KEY_INVALID);

  options = grl_operation_options_new (NULL);
  media = grl_media_audio_new ();
  grl_media_set_url (media, "file://" TRACKER3_DATA_PATH "/sample.mp3");

  resolved = grl_source_resolve_sync (source,
                                      media,
                                      keys,
                                      options, &error);
  g_assert_no_error (error);
  g_assert_nonnull (resolved);
  g_assert_true (resolved == media);

  g_assert_cmpstr (grl_media_get_url (media),
                   ==,
                   "file://" TRACKER3_DATA_PATH "/sample.mp3");
  g_assert_cmpstr (grl_media_get_title (media),
                   ==,
                   "Simply Juvenile");

  datetime = grl_media_get_modification_date (media);
  g_assert_nonnull (datetime);

  g_object_unref (options);
  g_object_unref (media);
  g_list_free (keys);
}

static void
test_resolve_non_existent (void)
{
  GrlRegistry *registry;
  GrlSource *source;
  GrlMedia *media, *resolved;
  GError *error = NULL;
  GrlOperationOptions *options;
  GList *keys;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, TRACKER3_SOURCE_ID);
  g_assert (source);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_URL,
                                    GRL_METADATA_KEY_TITLE,
                                    GRL_METADATA_KEY_INVALID);

  options = grl_operation_options_new (NULL);
  media = grl_media_audio_new ();
  grl_media_set_url (media, "file://" TRACKER3_DATA_PATH "/IDoNotExist");

  resolved = grl_source_resolve_sync (source,
                                      media,
                                      keys,
                                      options, &error);
  g_assert_no_error (error);
  g_assert_nonnull (resolved);

  g_assert_cmpstr (grl_media_get_title (media),
                   ==,
                   NULL);

  g_object_unref (options);
  g_object_unref (media);
  g_list_free (keys);
}

static void
test_search (void)
{
  GrlRegistry *registry;
  GrlSource *source;
  GError *error = NULL;
  GList *elements;
  GrlOperationOptions *options;
  GList *keys;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, TRACKER3_SOURCE_ID);
  g_assert (source);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_URL,
                                    GRL_METADATA_KEY_TITLE,
                                    GRL_METADATA_KEY_INVALID);

  options = grl_operation_options_new (NULL);

  elements = grl_source_search_sync (source,
                                     "Simply",
                                     keys,
                                     options, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_list_length (elements), ==, 1);

  g_assert_cmpstr (grl_media_get_url (g_list_nth_data (elements, 0)),
                   ==,
                   "file://" TRACKER3_DATA_PATH "/sample.mp3");
  g_assert_cmpstr (grl_media_get_title (g_list_nth_data (elements, 0)),
                   ==,
                   "Simply Juvenile");

  g_object_unref (options);
  g_list_free (keys);
}

static void
test_search_non_existent (void)
{
  GrlRegistry *registry;
  GrlSource *source;
  GError *error = NULL;
  GList *elements;
  GrlOperationOptions *options;
  GList *keys;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, TRACKER3_SOURCE_ID);
  g_assert (source);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_URL,
                                    GRL_METADATA_KEY_TITLE,
                                    GRL_METADATA_KEY_INVALID);

  options = grl_operation_options_new (NULL);

  elements = grl_source_search_sync (source,
                                     "I do not exist",
                                     keys,
                                     options, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_list_length (elements), ==, 0);

  g_object_unref (options);
  g_list_free (keys);
}

static void
test_search_range_filter (void)
{
  GrlRegistry *registry;
  GrlSource *source;
  GError *error = NULL;
  GList *elements;
  GrlOperationOptions *options;
  GList *keys;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, TRACKER3_SOURCE_ID);
  g_assert (source);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_URL,
                                    GRL_METADATA_KEY_TITLE,
                                    GRL_METADATA_KEY_INVALID);

  options = grl_operation_options_new (NULL);
  grl_operation_options_set_key_range_filter (options,
                                              GRL_METADATA_KEY_DURATION,
                                              5, 13, NULL);

  elements = grl_source_search_sync (source,
                                     NULL,
                                     keys,
                                     options, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_list_length (elements), ==, 2);

  elements = g_list_sort (elements, compare_by_url);

  g_assert_cmpstr (grl_media_get_url (g_list_nth_data (elements, 0)),
                   ==,
                   "file://" TRACKER3_DATA_PATH "/sample.flac");

  g_assert_cmpstr (grl_media_get_url (g_list_nth_data (elements, 1)),
                   ==,
                   "file://" TRACKER3_DATA_PATH "/sample.ogv");

  g_object_unref (options);
  g_list_free (keys);
}

static void
test_query (void)
{
  GrlRegistry *registry;
  GrlSource *source;
  GError *error = NULL;
  GList *elements;
  GrlOperationOptions *options;
  GList *keys;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, TRACKER3_SOURCE_ID);
  g_assert (source);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_URL,
                                    GRL_METADATA_KEY_TITLE,
                                    GRL_METADATA_KEY_INVALID);

  options = grl_operation_options_new (NULL);

  elements = grl_source_query_sync (source,
                                    "SELECT ('a' AS ?urn) ('file:///a' AS ?url) ('title' AS ?title) {}",
                                    keys,
                                    options, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_list_length (elements), ==, 1);

  g_assert_cmpstr (grl_media_get_url (g_list_nth_data (elements, 0)),
                   ==,
                   "file:///a");
  g_assert_cmpstr (grl_media_get_title (g_list_nth_data (elements, 0)),
                   ==,
                   "title");

  g_object_unref (options);
  g_list_free (keys);
}

int
main(int argc, char **argv)
{
  gint result;

  setlocale (LC_ALL, "");

  g_setenv ("GRL_PLUGIN_PATH", TRACKER3_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", TRACKER3_ID, TRUE);

  grl_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

  test_setup ();

  g_test_add_func ("/tracker3/browse", test_browse);
  g_test_add_func ("/tracker3/browse/type-filter", test_browse_type_filter);
  g_test_add_func ("/tracker3/media-from-uri", test_media_from_uri);
  g_test_add_func ("/tracker3/media-from-uri/non-existent", test_media_from_uri_nonexistent);
  g_test_add_func ("/tracker3/resolve", test_resolve);
  g_test_add_func ("/tracker3/resolve/non-existent", test_resolve_non_existent);
  g_test_add_func ("/tracker3/search", test_search);
  g_test_add_func ("/tracker3/search/non-existent", test_search_non_existent);
  g_test_add_func ("/tracker3/search/range-filter", test_search_range_filter);
  g_test_add_func ("/tracker3/query", test_query);

  result = g_test_run ();

  grl_deinit ();

  return result;
}
