/*
 * Copyright (C) 2014 Igalia S.L.
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

#define LASTFM_ALBUMART_ID "grl-lastfm-albumart"

static GrlMedia *
build_media_audio (const gchar *artist,
                   const gchar *album)
{
  GrlMedia *media;

  media = grl_media_audio_new ();
  grl_media_audio_set_artist (GRL_MEDIA_AUDIO (media), artist);
  grl_media_audio_set_album (GRL_MEDIA_AUDIO (media), album);

  return media;
}

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
test_may_resolve_good (void)
{
  GList *missing_keys = NULL;
  GrlMedia *media;
  GrlRegistry *registry;
  GrlSource *source;
  gboolean can_resolve;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, LASTFM_ALBUMART_ID);
  g_assert (source);

  media = build_media_audio ("My Artist", "My Album");

  can_resolve = grl_source_may_resolve (source, media, GRL_METADATA_KEY_THUMBNAIL, &missing_keys);

  g_assert (can_resolve);
  g_assert (!missing_keys);

  g_object_unref (media);
}

static void
test_may_resolve_wrong_key (void)
{
  GList *missing_keys = NULL;
  GrlMedia *media;
  GrlRegistry *registry;
  GrlSource *source;
  gboolean can_resolve;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, LASTFM_ALBUMART_ID);
  g_assert (source);

  media = build_media_audio ("My Artist", "My Album");

  can_resolve = grl_source_may_resolve (source, media, GRL_METADATA_KEY_TITLE, &missing_keys);

  g_assert (!can_resolve);
  g_assert (!missing_keys);

  g_object_unref (media);
}

static void
test_may_resolve_missing_key (void)
{
  GList *missing_keys = NULL;
  GrlMedia *media;
  GrlRegistry *registry;
  GrlSource *source;
  gboolean can_resolve;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, LASTFM_ALBUMART_ID);
  g_assert (source);

  media = build_media_audio ("My Artist", NULL);

  can_resolve = grl_source_may_resolve (source, media, GRL_METADATA_KEY_THUMBNAIL, &missing_keys);

  g_assert (!can_resolve);
  g_assert_cmpint (g_list_length (missing_keys), ==, 1);
  g_assert (GRLPOINTER_TO_KEYID (missing_keys->data) == GRL_METADATA_KEY_ALBUM);

  g_object_unref (media);
}

static void
test_may_resolve_missing_media (void)
{
  GList *missing_keys = NULL;
  GrlRegistry *registry;
  GrlSource *source;
  gboolean can_resolve;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, LASTFM_ALBUMART_ID);
  g_assert (source);

  can_resolve = grl_source_may_resolve (source, NULL, GRL_METADATA_KEY_THUMBNAIL, &missing_keys);

  g_assert (!can_resolve);
  g_assert_cmpint (g_list_length (missing_keys), ==, 2);
  g_assert (GRLPOINTER_TO_KEYID (missing_keys->data) == GRL_METADATA_KEY_ARTIST ||
            GRLPOINTER_TO_KEYID (missing_keys->data) == GRL_METADATA_KEY_ALBUM);
  g_assert (GRLPOINTER_TO_KEYID (missing_keys->next->data) == GRL_METADATA_KEY_ARTIST ||
            GRLPOINTER_TO_KEYID (missing_keys->next->data) == GRL_METADATA_KEY_ALBUM);
  g_assert (GRLPOINTER_TO_KEYID (missing_keys->data) != GRLPOINTER_TO_KEYID (missing_keys->next->data));
}

static void
test_resolve_good_found (void)
{
  GError *error = NULL;
  GList *keys;
  GrlMedia *media;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;
  guint expected_n_thumbnails;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, LASTFM_ALBUMART_ID);
  g_assert (source);

  media = build_media_audio ("madonna", "frozen");

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_THUMBNAIL, NULL);

  options = grl_operation_options_new (NULL);
  grl_operation_options_set_flags (options, GRL_RESOLVE_FULL);

  grl_source_resolve_sync (source, media, keys, options, &error);

  g_assert_no_error (error);

  /* We should get 5 thumbnails */
  expected_n_thumbnails = grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_THUMBNAIL);
  g_assert_cmpuint (expected_n_thumbnails, ==, 5);
  g_assert_cmpstr (grl_media_get_thumbnail_nth (media, 0),
                   ==,
                   "http://userserve-ak.last.fm/serve/500/76737256.png");
  g_assert_cmpstr (grl_media_get_thumbnail_nth (media, 1),
                   ==,
                   "http://userserve-ak.last.fm/serve/252/76737256.png");
  g_assert_cmpstr (grl_media_get_thumbnail_nth (media, 2),
                   ==,
                   "http://userserve-ak.last.fm/serve/126/76737256.png");
  g_assert_cmpstr (grl_media_get_thumbnail_nth (media, 3),
                   ==,
                   "http://userserve-ak.last.fm/serve/64s/76737256.png");
  g_assert_cmpstr (grl_media_get_thumbnail_nth (media, 4),
                   ==,
                   "http://userserve-ak.last.fm/serve/34s/76737256.png");

  g_list_free (keys);
  g_object_unref (options);
  g_object_unref (media);
}

static void
test_resolve_good_found_default (void)
{
  GError *error = NULL;
  GList *keys;
  GrlMedia *media;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;
  guint expected_n_thumbnails;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, LASTFM_ALBUMART_ID);
  g_assert (source);

  media = build_media_audio ("madonna", "frocen");

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_THUMBNAIL, NULL);

  options = grl_operation_options_new (NULL);
  grl_operation_options_set_flags (options, GRL_RESOLVE_FULL);

  grl_source_resolve_sync (source, media, keys, options, &error);

  g_assert_no_error (error);

  /* We should get 0 thumbnails */
  expected_n_thumbnails = grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_THUMBNAIL);
  g_assert_cmpuint (expected_n_thumbnails, ==, 0);
  g_assert (!grl_media_get_thumbnail (media));

  g_list_free (keys);
  g_object_unref (options);
  g_object_unref (media);
}

static void
test_resolve_good_not_found (void)
{
  GError *error = NULL;
  GList *keys;
  GrlMedia *media;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;
  guint expected_n_thumbnails;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, LASTFM_ALBUMART_ID);
  g_assert (source);

  media = build_media_audio ("madonna", "unknown");

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_THUMBNAIL, NULL);

  options = grl_operation_options_new (NULL);
  grl_operation_options_set_flags (options, GRL_RESOLVE_FULL);

  grl_source_resolve_sync (source, media, keys, options, &error);

  g_assert_no_error (error);

  /* We should get 0 thumbnails */
  expected_n_thumbnails = grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_THUMBNAIL);
  g_assert_cmpuint (expected_n_thumbnails, ==, 0);
  g_assert (!grl_media_get_thumbnail (media));

  g_list_free (keys);
  g_object_unref (options);
  g_object_unref (media);
}

static void
test_resolve_missing_key (void)
{
  GError *error = NULL;
  GList *keys;
  GrlMedia *media;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;
  guint expected_n_thumbnails;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, LASTFM_ALBUMART_ID);
  g_assert (source);

  media = build_media_audio ("madonna", NULL);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_THUMBNAIL, NULL);

  options = grl_operation_options_new (NULL);
  grl_operation_options_set_flags (options, GRL_RESOLVE_FULL);

  grl_source_resolve_sync (source, media, keys, options, &error);

  g_assert_no_error (error);

  /* We should get 0 thumbnails */
  expected_n_thumbnails = grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_THUMBNAIL);
  g_assert_cmpuint (expected_n_thumbnails, ==, 0);
  g_assert (!grl_media_get_thumbnail (media));

  g_list_free (keys);
  g_object_unref (options);
  g_object_unref (media);
}

int
main(int argc, char **argv)
{
  g_setenv ("GRL_PLUGIN_PATH", LASTFM_ALBUMART_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", LASTFM_ALBUMART_ID, TRUE);
  g_setenv ("GRL_NET_MOCKED", LASTFM_ALBUMART_DATA_PATH "network-data.ini", TRUE);

  grl_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

#if !GLIB_CHECK_VERSION(2,32,0)
  g_thread_init (NULL);
#endif

  test_setup ();

  g_test_add_func ("/lastfm-albumart/may-resolve/good", test_may_resolve_good);
  g_test_add_func ("/lastfm-albumart/may-resolve/wrong-key", test_may_resolve_wrong_key);
  g_test_add_func ("/lastfm-albumart/may-resolve/missing-key", test_may_resolve_missing_key);
  g_test_add_func ("/lastfm-albumart/may-resolve/missing-media", test_may_resolve_missing_media);
  g_test_add_func ("/lastfm-albumart/resolve/good-found", test_resolve_good_found);
  g_test_add_func ("/lastfm-albumart/resolve/good-found-default", test_resolve_good_found_default);
  g_test_add_func ("/lastfm-albumart/resolve/good-not-found", test_resolve_good_not_found);
  g_test_add_func ("/lastfm-albumart/resolve/missing-key", test_resolve_missing_key);

  gint result = g_test_run ();

  grl_deinit ();

  return result;
}
