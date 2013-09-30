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

#define BLIPTV_ID "grl-bliptv"

#define AUTOSPLIT 100

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
test_unload (const gchar *plugin_id)
{
  GError *error = NULL;
  GrlRegistry *registry;

  registry = grl_registry_get_default ();
  grl_registry_unload_plugin (registry, plugin_id, &error);
  g_assert_no_error (error);
}

static void
test_browse (void)
{
  GError *error = NULL;
  GList *medias;
  GrlMedia *media;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, "grl-bliptv");
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

  media = g_list_nth_data (medias, 0);

  g_assert (GRL_IS_MEDIA_VIDEO (media));
  g_assert_cmpstr (grl_media_get_id (media),
                   ==,
                   "6597003");
  g_assert_cmpstr (grl_media_get_mime (media),
                   ==,
                   "video/vnd.objectvideo");
  g_assert_cmpstr (grl_media_get_thumbnail (media),
                   ==,
                   "http://a.images.blip.tv/SPI1-MichaelKluehOnTheMorningSwimShow235-244.jpg");
  g_assert_cmpstr (grl_media_get_title (media),
                   ==,
                   "Michael Klueh on The Morning Swim Show");

  media = g_list_nth_data (medias, 1);

  g_assert (GRL_IS_MEDIA_VIDEO (media));
  g_assert_cmpstr (grl_media_get_id (media),
                   ==,
                   "6597002");
  g_assert_cmpstr (grl_media_get_mime (media),
                   ==,
                   "video/vnd.objectvideo");
  g_assert_cmpstr (grl_media_get_thumbnail (media),
                   ==,
                   "http://a.images.blip.tv/SPI1-DavidPlummerOnTheMorningSwimShow861-682.jpg");
  g_assert_cmpstr (grl_media_get_title (media),
                   ==,
                   "David Plummer on The Morning Swim Show");

  g_list_free_full (medias, g_object_unref);
  g_object_unref (options);
}

static void
test_search_results (void)
{
  GError *error = NULL;
  GList *medias;
  GrlMedia *media;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, "grl-bliptv");
  g_assert (source);
  options = grl_operation_options_new (NULL);
  grl_operation_options_set_count (options, 2);
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
                   "6597160");
  g_assert_cmpstr (grl_media_get_mime (media),
                   ==,
                   "video/vnd.objectvideo");
  g_assert_cmpstr (grl_media_get_thumbnail (media),
                   ==,
                   "http://a.images.blip.tv/SunsetVineAPP-SeaMasterSailingJune2013374.jpg");
  g_assert_cmpstr (grl_media_get_title (media),
                   ==,
                   "Sea Master Sailing June 2013");

  media = g_list_nth_data (medias, 1);

  g_assert (GRL_IS_MEDIA_VIDEO (media));
  g_assert_cmpstr (grl_media_get_id (media),
                   ==,
                   "6597159");
  g_assert_cmpstr (grl_media_get_mime (media),
                   ==,
                   "video/vnd.objectvideo");
  g_assert_cmpstr (grl_media_get_thumbnail (media),
                   ==,
                   "http://a.images.blip.tv/RenaissanceWW-RWW164ShavePony286-905.jpg");
  g_assert_cmpstr (grl_media_get_title (media),
                   ==,
                   "RWW 164 Shave Pony");

  g_list_free_full (medias, g_object_unref);
  g_object_unref (options);
}

static void
test_search_no_results (void)
{
  GError *error = NULL;
  GList *medias;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, "grl-bliptv");
  g_assert (source);
  options = grl_operation_options_new (NULL);
  grl_operation_options_set_count (options, 2);
  g_assert (options);

  medias = grl_source_search_sync (source,
                                   "grilo",
                                   grl_source_supported_keys (source),
                                   options,
                                   &error);

  g_assert_cmpint (g_list_length(medias), ==, 0);
  g_assert_no_error (error);

  g_object_unref (options);
}

static void
test_autosplit (void)
{
  GError *error = NULL;
  GList *medias;
  GrlMedia *media;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, "grl-bliptv");
  g_assert (source);
  options = grl_operation_options_new (NULL);
  grl_operation_options_set_count (options, 2);
  grl_operation_options_set_skip (options, AUTOSPLIT - 1);
  g_assert (options);

  medias = grl_source_browse_sync (source,
                                   NULL,
                                   grl_source_supported_keys (source),
                                   options,
                                   &error);

  g_assert_cmpint (g_list_length(medias), ==, 2);
  g_assert_no_error (error);

  media = g_list_nth_data (medias, 0);

  g_assert (GRL_IS_MEDIA_VIDEO (media));
  g_assert_cmpstr (grl_media_get_id (media),
                   ==,
                   "6596718");
  g_assert_cmpstr (grl_media_get_mime (media),
                   ==,
                   "video/vnd.objectvideo");
  g_assert_cmpstr (grl_media_get_thumbnail (media),
                   ==,
                   "http://a.images.blip.tv/Marcelobronxnet-FuneralPreparationPeopleWithSpecialNeedsSPSI516.jpg");
  g_assert_cmpstr (grl_media_get_title (media),
                   ==,
                   "Funeral Preparation & People with Special Needs | SPSI");

  media = g_list_nth_data (medias, 1);

  g_assert (GRL_IS_MEDIA_VIDEO (media));
  g_assert_cmpstr (grl_media_get_id (media),
                   ==,
                   "6596714");
  g_assert_cmpstr (grl_media_get_mime (media),
                   ==,
                   "video/vnd.objectvideo");
  g_assert_cmpstr (grl_media_get_thumbnail (media),
                   ==,
                   "http://a.images.blip.tv/LivingYourYoga-TheNewMicPacksHaveArrived784.jpg");
  g_assert_cmpstr (grl_media_get_title (media),
                   ==,
                   "The New Mic Packs have arrived");

  g_list_free_full (medias, g_object_unref);
  g_object_unref (options);
}

int
main(int argc, char **argv)
{
  g_setenv ("GRL_PLUGIN_PATH", BLIPTV_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", BLIPTV_ID, TRUE);
  g_setenv ("GRL_NET_MOCKED", BLIPTV_DATA_PATH "network-data.ini", TRUE);

  grl_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

#if !GLIB_CHECK_VERSION(2,32,0)
  g_thread_init (NULL);
#endif

  test_setup ();

  g_test_add_func ("/bliptv/browse", test_browse);
  g_test_add_func ("/bliptv/search/results", test_search_results);
  g_test_add_func ("/bliptv/search/no-results", test_search_no_results);
  g_test_add_func ("/bliptv/autosplit", test_autosplit);

  gint result = g_test_run ();

  test_unload (BLIPTV_ID);

  return result;
}
