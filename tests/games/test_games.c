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

#define THEGAMESDB "grl-thegamesdb"

static GrlMedia *
build_game_media (const gchar *title)
{
  GrlMedia *media;

  media = grl_media_new ();
  grl_media_set_title (media, title);

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
#define DESCRIPTION "Set in the future, the game centers around a secret organization of ninja-like operatives known as \"Striders\", who specializes in various kinds of wetworks such as smuggling, kidnapping, demolitions, and disruption. The player takes control Strider Hiryu, the youngest elite-class Strider in the organization. Hiryu is summoned by the organization's second-in-command, Vice Director Matic, to assassinate his friend Kain, who has been captured by hostile forces and has become a liability to the Striders. Instead of killing him, Hiryu decides to rescue Kain from his captors. With the help of his fellow Strider Sheena, Hiryu uncovers a conspiracy between a certain faction of the Strider organization led by Matic himself and an unknown organization known simply as the \"Enterprise\" (headed by a man named Faceas Clay) which involves the development of a mind-control weapon codenamed \"Zain\". In the course of finding and destroying these Zain units, Hiryu learns that the faction of conspirators is headed by Vice Director Matic himself. Hiryu eventually tracks Matic to an orbiting space station where the two Striders face off; after a brief battle Hiryu bests Matic and kills him. Afterwards Hiryu locates and destroys the last of the Zain units.\n\nIn the epilogue, it is revealed that though Hiryu was asked to return to the Strider organization he instead opted to retire. The final credits show him sheathing his weapon and walking away."

static void
test_resolve_good_found (void)
{
  GError *error = NULL;
  GList *keys;
  GrlMedia *media;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;
  GDateTime *date_time;
  guint expected_n_thumbnails, expected_n_external_urls;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, THEGAMESDB);
  g_assert (source);

  media = build_game_media ("Strider");
  grl_media_set_mime (media, "application/x-genesis-rom");

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_THUMBNAIL,
                                    GRL_METADATA_KEY_DESCRIPTION,
                                    GRL_METADATA_KEY_EXTERNAL_URL,
                                    GRL_METADATA_KEY_RATING,
                                    GRL_METADATA_KEY_PUBLICATION_DATE,
                                    GRL_METADATA_KEY_ORIGINAL_TITLE,
                                    NULL);

  options = grl_operation_options_new (NULL);
  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_FULL);

  grl_source_resolve_sync (source, media, keys, options, &error);

  g_assert_no_error (error);

  /* We should get a thumbnail */
  expected_n_thumbnails = grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_THUMBNAIL);
  g_assert_cmpuint (expected_n_thumbnails, ==, 1);
  g_assert_cmpstr (grl_media_get_thumbnail_nth (media, 0),
                   ==,
                   "http://thegamesdb.net/banners/boxart/original/front/702-1.jpg");

  g_assert_cmpstr (grl_media_get_description (media),
                   ==,
                   DESCRIPTION);

  expected_n_external_urls = grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_EXTERNAL_URL);
  g_assert_cmpuint (expected_n_external_urls, ==, 1);
  g_assert_cmpstr (grl_media_get_external_url (media),
                   ==,
                   "http://thegamesdb.net/game/702/");

  /* Comparing floats fails with: (3.71000004 == 3.71) */
  g_assert_cmpint (grl_media_get_rating (media) * 100, ==, 300);

  date_time = grl_media_get_publication_date (media);
  g_assert_cmpint (g_date_time_get_year (date_time), ==, 1989);

  g_list_free (keys);
  g_object_unref (options);
  g_object_unref (media);
}

static void
test_resolve_thumbnail_found (GrlSource *source,
                              GList *keys,
                              GrlOperationOptions *options,
                              const gchar *title,
                              const gchar *mime,
                              const gchar *url,
                              guint expected_thumbnail_index,
                              const gchar *expected_thumbnail_url)
{
  GError *error = NULL;
  GrlMedia *media;
  guint expected_n_thumbnails;

  media = build_game_media (title);
  grl_media_set_mime (media, mime);
  grl_media_set_url (media, url);

  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_FULL);

  grl_source_resolve_sync (source, media, keys, options, &error);

  g_assert_no_error (error);

  /* We should get a thumbnail */
  expected_n_thumbnails = grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_THUMBNAIL);
  g_assert_cmpuint (expected_n_thumbnails, >, 0);
  g_assert_cmpstr (grl_media_get_thumbnail_nth (media, expected_thumbnail_index),
                   ==,
                   expected_thumbnail_url);

  g_object_unref (media);
}

static void
test_resolve_thumbnails_found (void)
{
  GList *keys;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, THEGAMESDB);
  g_assert (source);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_THUMBNAIL,
                                    NULL);

  options = grl_operation_options_new (NULL);
  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_FULL);

  test_resolve_thumbnail_found (source, keys, options,
                                "Kirby & the Amazing Mirror",
                                "application/x-gba-rom",
                                NULL,
                                0,
                                "http://thegamesdb.net/banners/boxart/original/front/2336-1.png");

  test_resolve_thumbnail_found (source, keys, options,
                                "Kirby's Dream Land",
                                "application/x-gameboy-rom",
                                NULL,
                                0,
                                "http://thegamesdb.net/banners/boxart/original/front/8706-1.jpg");

  test_resolve_thumbnail_found (source, keys, options,
                                "Sonic the Hedgehog",
                                "application/x-sms-rom",
                                NULL,
                                0,
                                "http://thegamesdb.net/banners/boxart/original/front/3016-1.jpg");
 
  test_resolve_thumbnail_found (source, keys, options,
                                "Sonic the Hedgehog",
                                "application/x-sms-rom",
                                "sonic.gg",
                                0,
                                "http://thegamesdb.net/banners/boxart/original/front/5754-1.jpg");

  test_resolve_thumbnail_found (source, keys, options,
                                "Astérix",
                                "application/x-gamegear-rom",
                                NULL,
                                0,
                                "http://thegamesdb.net/banners/boxart/original/front/11837-1.jpg");

  test_resolve_thumbnail_found (source, keys, options,
                                "Shatterhand",
                                "application/x-nes-rom",
                                NULL,
                                0,
                                "http://thegamesdb.net/banners/boxart/original/front/22619-1.jpg");

  g_list_free (keys);
  g_object_unref (options);
}

static void
test_resolve_genre_found (GrlSource *source,
                              GList *keys,
                              GrlOperationOptions *options,
                              const gchar *title,
                              const gchar *mime,
                              guint no_of_genres)
{
  GError *error = NULL;
  GrlMedia *media;
  guint expected_n_genres;

  media = build_game_media (title);
  grl_media_set_mime (media, mime);

  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_FULL);

  grl_source_resolve_sync (source, media, keys, options, &error);

  g_assert_no_error (error);

  /* We should get a genre */
  expected_n_genres = grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_GENRE);
  g_assert_cmpuint (expected_n_genres, ==, no_of_genres);

  g_object_unref (media);
}

static void
test_resolve_genres_found (void)
{
  GList *keys;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, THEGAMESDB);
  g_assert (source);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_GENRE,
                                    NULL);

  options = grl_operation_options_new (NULL);
  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_FULL);

  test_resolve_genre_found (source, keys, options,
                            "Astérix",
                            "application/x-gamegear-rom",
                            1);

  test_resolve_genre_found (source, keys, options,
                            "Shatterhand",
                            "application/x-nes-rom",
                            2);

  g_list_free (keys);
  g_object_unref (options);
}

static void
test_resolve_key_found (GrlRegistry *registry,
                        GrlSource *source,
                        GList *keys,
                        GrlOperationOptions *options,
                        const gchar *key_name,
                        const gchar *title,
                        const gchar *mime,
                        guint no_of_values)
{
  GError *error = NULL;
  GrlMedia *media;
  GrlKeyID key;
  guint expected_n_values;

  media = build_game_media (title);
  grl_media_set_mime (media, mime);

  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_FULL);

  key = grl_registry_lookup_metadata_key (registry, key_name);

  g_assert_cmpuint (key, ==, GRL_METADATA_KEY_INVALID);

  grl_source_resolve_sync (source, media, keys, options, &error);

  g_assert_no_error (error);

  key = grl_registry_lookup_metadata_key (registry, key_name);

  g_assert_cmpuint (key, !=, GRL_METADATA_KEY_INVALID);

  /* We should get a value */
  expected_n_values = grl_data_length (GRL_DATA (media), key);
  g_assert_cmpuint (expected_n_values, ==, no_of_values);

  g_object_unref (media);
}

static void
test_resolve_keys_found (void)
{
  GList *keys;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlSource *source;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, THEGAMESDB);
  g_assert (source);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_GENRE,
                                    NULL);

  options = grl_operation_options_new (NULL);
  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_FULL);

  test_resolve_key_found (registry, source, keys, options,
                          "developer",
                          "Kirby & the Amazing Mirror",
                          "application/x-gba-rom",
                           1);

  g_list_free (keys);
  g_object_unref (options);
}


int
main(int argc, char **argv)
{
  gint result;

  setlocale (LC_ALL, "");

  g_setenv ("GRL_PLUGIN_PATH", LUA_FACTORY_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_LUA_SOURCES_PATH", LUA_SOURCES_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", "grl-lua-factory", TRUE);
  g_setenv ("GRL_NET_MOCKED", GAMES_DATA_PATH "network-data.ini", TRUE);

  grl_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

  test_setup ();

  /* FIXME: Move tests to g_test_add() to init/deinit the registry for each test */
  g_test_add_func ("/thegamesdb/resolve/keys-found", test_resolve_keys_found);
  g_test_add_func ("/thegamesdb/resolve/good-found", test_resolve_good_found);
  g_test_add_func ("/thegamesdb/resolve/thumbnails-found", test_resolve_thumbnails_found);
  g_test_add_func ("/thegamesdb/resolve/genre-found", test_resolve_genres_found);

  result = g_test_run ();

  grl_deinit ();

  return result;
}
