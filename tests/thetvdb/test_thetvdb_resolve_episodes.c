/*
 * Copyright (C) 2014. All rights reserved.
 *
 * Author: Victor Toso <me@victortoso.com>
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

#include "test_thetvdb_utils.h"
#include <locale.h>
#include <grilo.h>

static void
get_episode_metadata_from_episode (GrlSource *source,
                                   const gchar *show,
                                   gint season,
                                   gint episode,
                                   gchar **imdb,
                                   gchar **tvdb_id,
                                   gchar **zap2it,
                                   gchar **publication_date,
                                   gchar **title,
                                   gchar **episode_screen)
{
  GrlMedia *video;
  GrlOperationOptions *options;
  GList *keys;
  GDateTime *date;
  GrlRegistry *registry;
  GrlKeyID tvdb_key, imdb_key, zap2it_key, ss_key;

  registry = grl_registry_get_default ();

  tvdb_key = grl_registry_lookup_metadata_key (registry, "thetvdb-id");
  imdb_key = grl_registry_lookup_metadata_key (registry, "thetvdb-imdb-id");
  zap2it_key = grl_registry_lookup_metadata_key (registry, "thetvdb-zap2it-id");
  ss_key = grl_registry_lookup_metadata_key (registry, "thetvdb-episode-screenshot");

  g_assert_cmpint (tvdb_key, !=, GRL_METADATA_KEY_INVALID);
  g_assert_cmpint (imdb_key, !=, GRL_METADATA_KEY_INVALID);
  g_assert_cmpint (zap2it_key, !=, GRL_METADATA_KEY_INVALID);
  g_assert_cmpint (ss_key, !=, GRL_METADATA_KEY_INVALID);

  video = grl_media_video_new ();
  grl_media_set_show (video, show);
  grl_media_set_season (video, season);
  grl_media_set_episode (video, episode);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_EPISODE_TITLE,
                                    GRL_METADATA_KEY_PUBLICATION_DATE,
                                    tvdb_key,
                                    imdb_key,
                                    zap2it_key,
                                    ss_key,
                                    NULL);

  options = grl_operation_options_new (NULL);
  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_NORMAL);

  grl_source_resolve_sync (source,
                           GRL_MEDIA (video),
                           keys,
                           options,
                           NULL);
  *title = g_strdup (grl_media_get_episode_title (video));
  *imdb = g_strdup (grl_data_get_string (GRL_DATA (video), imdb_key));
  *tvdb_id = g_strdup (grl_data_get_string (GRL_DATA (video), tvdb_key));
  *zap2it = g_strdup (grl_data_get_string (GRL_DATA (video), zap2it_key));
  *episode_screen = g_strdup (grl_data_get_string (GRL_DATA (video), ss_key));

  date = grl_media_get_publication_date (GRL_MEDIA (video));
  *publication_date = g_date_time_format (date, "%Y-%m-%d");

  g_list_free (keys);
  g_object_unref (options);
  g_object_unref (video);
}

static void
test_episodes_from_episode (void)
{
  GrlSource *source;
  guint i;

  struct {
    const gchar *show;
    const gchar *title;
    gint season;
    gint episode;
    const gchar *imdb;
    const gchar *tvdb_id;
    const gchar *zap2it_id;
    const gchar *publication_date;
    const gchar *url_episode_screen;
  } videos[] = {
    { "Boardwalk Empire", "New York Sour", 4, 1,
      "tt2483070", "4596908", "SH01308836", "2013-09-08",
      "https://thetvdb.com/banners/episodes/84947/4596908.jpg" },
    { "Adventure Time", "It Came from the Nightosphere", 2, 1,
      "tt1305826", "2923061", "EP01246265", "2010-10-11",
      "https://thetvdb.com/banners/episodes/152831/2923061.jpg" },
    { "Felicity", "The Last Stand", 1, 2,
      "tt0134247", "133911", "SH184561", "1998-10-06",
      "https://thetvdb.com/banners/episodes/73980/133911.jpg" },
    { "House", "Everybody Dies", 8, 22,
      "tt2121965", "4265769", "EP00688359", "2012-05-21",
      "https://thetvdb.com/banners/episodes/73255/4265769.jpg" },
    { "Naruto", "Yakumo's Sealed Power", 5, 25,
      "tt0409591", "446714", "SH774951", "2006-10-05",
      "https://thetvdb.com/banners/episodes/78857/446714.jpg" }
  };

  source = test_get_source();
  g_assert (source);

  for (i = 0; i < G_N_ELEMENTS (videos); i++) {
    gchar *imdb, *tvdb_id, *zap2it_id, *pdate, *title, *url_episode_screen;

    get_episode_metadata_from_episode (source, videos[i].show, videos[i].season,
                                       videos[i].episode, &imdb, &tvdb_id,
                                       &zap2it_id, &pdate, &title,
                                       &url_episode_screen);
    g_assert_cmpstr (videos[i].imdb, ==, imdb);
    g_free (imdb);
    g_assert_cmpstr (videos[i].tvdb_id, ==, tvdb_id);
    g_free (tvdb_id);
    g_assert_cmpstr (videos[i].zap2it_id, ==, zap2it_id);
    g_free (zap2it_id);
    g_assert_cmpstr (videos[i].publication_date, ==, pdate);
    g_free (pdate);
    g_assert_cmpstr (videos[i].title, ==, title);
    g_free (title);
    /* Those urls may change due up/down vote at tvdb system,
     * just shot a message if they change, do not fail the test. */
    if (g_strcmp0 (videos[i].url_episode_screen, url_episode_screen) != 0)
      g_message ("[%s] Episode screen changed from %s to %s",
                 videos[i].show, videos[i].url_episode_screen, url_episode_screen);
    g_free (url_episode_screen);
  }
}

gint
main (gint argc, gchar **argv)
{
  gint result;

  setlocale (LC_ALL, "");

  g_setenv ("GRL_PLUGIN_PATH", THETVDB_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", THETVDB_ID, TRUE);
  g_setenv ("GRL_NET_MOCKED", THETVDB_PLUGIN_TEST_DATA_PATH "config.ini", TRUE);

  grl_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

  test_setup_thetvdb ();

  g_test_add_func ("/thetvdb/resolve/episodes_from_episode", test_episodes_from_episode);

  result = g_test_run ();

  test_shutdown_thetvdb ();

  grl_deinit ();

  return result;
}
