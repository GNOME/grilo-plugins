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

#define VIMEO_ID "grl-vimeo"

#define VIMEO_KEY      "TEST_VIMEO_KEY"
#define VIMEO_SECRET   "TEST_VIMEO_SECRET"

static void
test_setup (void)
{
  GError *error = NULL;
  GrlConfig *config;
  GrlRegistry *registry;

  registry = grl_registry_get_default ();

  config = grl_config_new (VIMEO_ID, NULL);
  grl_config_set_api_key (config, VIMEO_KEY);
  grl_config_set_api_secret (config, VIMEO_SECRET);
  grl_registry_add_config (registry, config, NULL);

  grl_registry_load_all_plugins (registry, &error);
  g_assert_no_error (error);
}

static void
test_search_normal (void)
{
  GError *error = NULL;
  GList *medias;
  GrlMedia *media;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, VIMEO_ID);
  g_assert (source);
  options = grl_operation_options_new (NULL);
  grl_operation_options_set_count (options, 2);
  grl_operation_options_set_flags (options, GRL_RESOLVE_FAST_ONLY);
  g_assert (options);

  medias = grl_source_search_sync (source,
                                   "gnome",
                                   grl_source_supported_keys (source),
                                   options,
                                   &error);

  g_assert_cmpint (g_list_length(medias), ==, 2);
  g_assert_no_error (error);

  media = g_list_nth_data (medias, 0);

  g_assert (GRL_IS_MEDIA_VIDEO (media));
  g_assert_cmpstr (grl_media_get_id (media),
                   ==,
                   "31110838");
  g_assert_cmpstr (grl_media_get_title (media),
                   ==,
                   "Mound by Allison Schulnik");
  g_assert_cmpstr (grl_media_get_author (media),
                   ==,
                   "garaco taco");
  g_assert_cmpstr (grl_media_get_thumbnail (media),
                   ==,
                   "http://b.vimeocdn.com/ts/209/404/209404005_100.jpg");

  media = g_list_nth_data (medias, 1);

  g_assert (GRL_IS_MEDIA_VIDEO (media));
  g_assert_cmpstr (grl_media_get_id (media),
                   ==,
                   "13797705");
  g_assert_cmpstr (grl_media_get_title (media),
                   ==,
                   "Shell Yes! motion design mockup");
  g_assert_cmpstr (grl_media_get_author (media),
                   ==,
                   "GNOME Shell");
  g_assert_cmpstr (grl_media_get_thumbnail (media),
                   ==,
                   "http://b.vimeocdn.com/ts/798/198/79819832_100.jpg");

  g_list_free_full (medias, g_object_unref);
  g_object_unref (options);
}

static void
test_search_null (void)
{
  GError *error = NULL;
  GList *medias;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, VIMEO_ID);
  g_assert (source);
  options = grl_operation_options_new (NULL);
  grl_operation_options_set_count (options, 2);
  grl_operation_options_set_flags (options, GRL_RESOLVE_FAST_ONLY);
  g_assert (options);

  medias = grl_source_search_sync (source,
                                   NULL,
                                   grl_source_supported_keys (source),
                                   options,
                                   &error);

  g_assert_cmpint (g_list_length(medias), ==, 0);
  g_assert_error (error,
                  GRL_CORE_ERROR,
                  GRL_CORE_ERROR_SEARCH_NULL_UNSUPPORTED);

  g_object_unref (options);
}

static void
test_search_empty (void)
{
  GError *error = NULL;
  GList *medias;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, VIMEO_ID);
  g_assert (source);
  options = grl_operation_options_new (NULL);
  grl_operation_options_set_count (options, 2);
  grl_operation_options_set_flags (options, GRL_RESOLVE_FAST_ONLY);
  g_assert (options);

  medias = grl_source_search_sync (source,
                                   "invalidfoo",
                                   grl_source_supported_keys (source),
                                   options,
                                   &error);

  g_assert_cmpint (g_list_length(medias), ==, 0);
  g_assert_no_error (error);

  g_object_unref (options);
}

int
main (int argc, char **argv)
{
  g_setenv ("GRL_PLUGIN_PATH", VIMEO_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", VIMEO_ID, TRUE);
  g_setenv ("GRL_NET_MOCKED", VIMEO_DATA_PATH "network-data.ini", TRUE);

  grl_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

#if !GLIB_CHECK_VERSION(2,32,0)
  g_thread_init (NULL);
#endif

  test_setup ();

  g_test_add_func ("/vimeo/search/normal", test_search_normal);
  g_test_add_func ("/vimeo/search/null", test_search_null);
  g_test_add_func ("/vimeo/search/empty", test_search_empty);

  return g_test_run ();
}
