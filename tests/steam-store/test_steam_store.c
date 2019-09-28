/*
 * Copyright (C) 2018 Igalia S.L.
 *
 * Author: Tony Crisci <tony@dubstepdish.com>
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
#include <stdio.h>

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
test_resolve_game_found (void)
{
  GError *error = NULL;
  GrlRegistry *registry;
  GrlSource *source;
  GrlOperationOptions *options;
  GList *keys;
  guint expected_n_thumbnails, expected_n_publishers, expected_n_developers;
  GrlKeyID publisher_key, developer_key;
  const GValue *developers, *publishers;
  GList *publishers_list, *developers_list;

  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, "grl-steam-store");
  g_assert (source);

  GrlMedia *media = grl_media_new ();
  grl_media_set_id(media, "641990");
  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_TITLE,
                                    GRL_METADATA_KEY_THUMBNAIL,
                                    GRL_METADATA_KEY_DESCRIPTION,
                                    GRL_METADATA_KEY_EXTERNAL_URL,
                                    GRL_METADATA_KEY_RATING,
                                    GRL_METADATA_KEY_PUBLICATION_DATE,
                                    NULL);
  options = grl_operation_options_new (NULL);
  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_FULL);
  grl_source_resolve_sync (source, media, keys, options, &error);
  g_assert_no_error (error);

  /* Thumbnail */
  expected_n_thumbnails = grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_THUMBNAIL);
  g_assert_cmpuint (expected_n_thumbnails, ==, 1);
  g_assert_cmpstr (grl_media_get_thumbnail_nth (media, 0),
                                                ==,
                                                "https://steamcdn-a.akamaihd.net/steam/apps/641990/header.jpg?t=1525881861");

  /* Description */
  g_assert_cmpstr (grl_media_get_description (media),
                   ==,
                   "<ABOUT_THE_GAME>");
  /* Title */
  g_assert_cmpstr (grl_media_get_title (media),
                   ==,
                   "The Escapists 2");

  /* Publication Date */
  GDateTime *date_time = grl_media_get_publication_date (media);
  g_assert_cmpint (g_date_time_get_year (date_time), ==, 2017);

  /* External URL */
  g_assert_cmpstr (grl_media_get_external_url (media),
                   ==,
                   "https://www.team17.com/games/the-escapists-2/");

  /* Rating */
  g_assert_cmpfloat (grl_media_get_rating (media),
                     ==,
                     75.0);

  /* Genres */
  gint expected_n_genres = grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_GENRE);
  g_assert_cmpuint (expected_n_genres, ==, 3);
  g_assert_cmpstr (grl_media_get_genre_nth (media, 0),
                   ==,
                   "Indie");
  g_assert_cmpstr (grl_media_get_genre_nth (media, 1),
                   ==,
                   "Simulation");
  g_assert_cmpstr (grl_media_get_genre_nth (media, 2),
                   ==,
                   "Strategy");

  /* Publishers */
  publisher_key = grl_registry_lookup_metadata_key (registry, "publisher");
  publishers = grl_data_get (GRL_DATA (media), publisher_key);
  g_assert_nonnull (publishers);
  expected_n_publishers = grl_data_length (GRL_DATA (media), publisher_key);
  g_assert_cmpuint (expected_n_publishers, ==, 1);
  publishers_list = grl_data_get_single_values_for_key_string (GRL_DATA (media), publisher_key);
  g_assert_nonnull (publishers_list);
  g_assert_cmpstr ((gchar *)g_list_nth_data (publishers_list, 0),
                   ==,
                   "Team17 Digital Ltd");

  /* Developers */
  developer_key = grl_registry_lookup_metadata_key(registry, "developer");
  developers = grl_data_get (GRL_DATA (media), developer_key);
  g_assert_nonnull (developers);
  expected_n_developers = grl_data_length (GRL_DATA (media), developer_key);
  g_assert_cmpuint (expected_n_developers, ==, 2);
  developers_list = grl_data_get_single_values_for_key_string (GRL_DATA (media), developer_key);
  g_assert_nonnull (developers_list);
  g_assert_cmpstr ((gchar *)g_list_nth_data (developers_list, 0),
                   ==,
                   "Team17 Digital Ltd");
  g_assert_cmpstr ((gchar *)g_list_nth_data (developers_list, 1),
                   ==,
                   "Mouldy Toof Studios");
}

int
main (int argc, char **argv)
{
  setlocale (LC_ALL, "");

  g_setenv ("GRL_PLUGIN_PATH", LUA_FACTORY_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_LUA_SOURCES_PATH", LUA_SOURCES_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", "grl-lua-factory", TRUE);
  g_setenv ("GRL_NET_MOCKED", STEAM_STORE_DATA_PATH "network-data.ini", TRUE);

  grl_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

  test_setup ();

  g_test_add_func ("/steam-store/resolve/game-found", test_resolve_game_found);

  gint result = g_test_run ();

  grl_deinit ();

  return result;
}
