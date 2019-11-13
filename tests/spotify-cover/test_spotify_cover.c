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

#include <locale.h>
#include <grilo.h>

#define SPOTIFY_ALBUMART_ID "grl-spotify-cover"

static GrlMedia *
build_media_audio (const gchar *artist,
                   const gchar *album)
{
  GrlMedia *media;

  media = grl_media_audio_new ();
  grl_media_set_artist (media, artist);
  grl_media_set_album (media, album);

  return media;
}

static void
test_setup (void)
{
  GError *error = NULL;
  GrlRegistry *registry;

  registry = grl_registry_get_default ();
  grl_registry_load_all_plugins (registry, TRUE, &error);
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
  source = grl_registry_lookup_source (registry, SPOTIFY_ALBUMART_ID);
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
  source = grl_registry_lookup_source (registry, SPOTIFY_ALBUMART_ID);
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
  source = grl_registry_lookup_source (registry, SPOTIFY_ALBUMART_ID);
  g_assert (source);

  media = build_media_audio ("My Artist", NULL);

  can_resolve = grl_source_may_resolve (source, media, GRL_METADATA_KEY_THUMBNAIL, &missing_keys);

  g_assert (!can_resolve);
  g_assert_cmpint (g_list_length (missing_keys), ==, 1);
  g_assert (GRLPOINTER_TO_KEYID (missing_keys->data) == GRL_METADATA_KEY_ALBUM);

  g_object_unref (media);
  g_list_free (missing_keys);
}

static void
test_may_resolve_missing_media (void)
{
  GList *missing_keys = NULL;
  GrlRegistry *registry;
  GrlSource *source;
  gboolean can_resolve;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, SPOTIFY_ALBUMART_ID);
  g_assert (source);

  can_resolve = grl_source_may_resolve (source, NULL, GRL_METADATA_KEY_THUMBNAIL, &missing_keys);

  g_assert (!can_resolve);
  g_assert_cmpint (g_list_length (missing_keys), ==, 2);
  g_assert (GRLPOINTER_TO_KEYID (missing_keys->data) == GRL_METADATA_KEY_ARTIST ||
            GRLPOINTER_TO_KEYID (missing_keys->data) == GRL_METADATA_KEY_ALBUM);
  g_assert (GRLPOINTER_TO_KEYID (missing_keys->next->data) == GRL_METADATA_KEY_ARTIST ||
            GRLPOINTER_TO_KEYID (missing_keys->next->data) == GRL_METADATA_KEY_ALBUM);
  g_assert (GRLPOINTER_TO_KEYID (missing_keys->data) != GRLPOINTER_TO_KEYID (missing_keys->next->data));
  g_list_free (missing_keys);
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
  source = grl_registry_lookup_source (registry, SPOTIFY_ALBUMART_ID);
  g_assert (source);

  media = build_media_audio ("madonna", "ray of light");

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_THUMBNAIL, NULL);

  options = grl_operation_options_new (NULL);
  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_FULL);

  grl_source_resolve_sync (source, media, keys, options, &error);

  g_assert_no_error (error);

  /* We should get 5 thumbnails */
  expected_n_thumbnails = grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_THUMBNAIL);
  g_assert_cmpuint (expected_n_thumbnails, ==, 3);
  g_assert_cmpstr (grl_media_get_thumbnail_nth (media, 0),
                   ==,
                   "https://i.scdn.co/image/246565c45ea4085d5b3889619fa1112ec6d42eed");
  g_assert_cmpstr (grl_media_get_thumbnail_nth (media, 1),
                   ==,
                   "https://i.scdn.co/image/f89849d36862a9dd2807be1d6d07eb0159c26673");
  g_assert_cmpstr (grl_media_get_thumbnail_nth (media, 2),
                   ==,
                   "https://i.scdn.co/image/cfa2d86696ff7cd8ea862f50ed05d086f1d66521");

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
  source = grl_registry_lookup_source (registry, SPOTIFY_ALBUMART_ID);
  g_assert (source);

  media = build_media_audio ("madonna", "ray of darkness");

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_THUMBNAIL, NULL);

  options = grl_operation_options_new (NULL);
  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_NORMAL);

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
  source = grl_registry_lookup_source (registry, SPOTIFY_ALBUMART_ID);
  g_assert (source);

  media = build_media_audio ("madonna", NULL);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_THUMBNAIL, NULL);

  options = grl_operation_options_new (NULL);
  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_NORMAL);

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
  setlocale (LC_ALL, "");

  g_setenv ("GRL_PLUGIN_PATH", LUA_FACTORY_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_LUA_SOURCES_PATH", LUA_SOURCES_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", "grl-lua-factory", TRUE);
  g_setenv ("GRL_NET_MOCKED", SPOTIFY_COVER_DATA_PATH "network-data.ini", TRUE);

  grl_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

  test_setup ();

  g_test_add_func ("/spotify-cover/may-resolve/good", test_may_resolve_good);
  g_test_add_func ("/spotify-cover/may-resolve/wrong-key", test_may_resolve_wrong_key);
  g_test_add_func ("/spotify-cover/may-resolve/missing-key", test_may_resolve_missing_key);
  g_test_add_func ("/spotify-cover/may-resolve/missing-media", test_may_resolve_missing_media);
  g_test_add_func ("/spotify-cover/resolve/good-found", test_resolve_good_found);
  g_test_add_func ("/spotify-cover/resolve/good-not-found", test_resolve_good_not_found);
  g_test_add_func ("/spotify-cover/resolve/missing-key", test_resolve_missing_key);

  gint result = g_test_run ();

  grl_deinit ();

  return result;
}
