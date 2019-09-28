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

static gint num_tests;

static GMainLoop *main_loop = NULL;

static GrlKeyID tvdb_key, imdb_key, zap2it_key, ss_key;

static struct {
  const gchar *show;
  const gchar *title;
  gint   season;
  gint   episode;
  const gchar *imdb;
  const gchar *tvdb_id;
  const gchar *zap2it_id;
  const gchar *publication_date;
  const gchar *url_episode_screen;
} videos[] = {
  {"House", "Now What?", 7, 1,
    "tt1697219", "2495251","EP00688359","2010-09-20",
    "https://thetvdb.com/banners/episodes/73255/2495251.jpg" },
  {"House", "Selfish", 7, 2,
    "tt1685104", "2756371","EP00688359","2010-09-27",
    "https://thetvdb.com/banners/episodes/73255/2756371.jpg" },
  {"House", "Unwritten", 7, 3,
    "tt1726377", "2825851","EP00688359","2010-10-04",
    "https://thetvdb.com/banners/episodes/73255/2825851.jpg" },
  {"House", "Massage Therapy", 7, 4,
    "tt1726378", "2878681","EP00688359","2010-10-11",
    "https://thetvdb.com/banners/episodes/73255/2878681.jpg" },
  {"House", "Unplanned Parenthood", 7, 5,
    "tt1726379", "2878691","EP00688359","2010-10-18",
    "https://thetvdb.com/banners/episodes/73255/2878691.jpg" },
  {"House", "Office Politics", 7, 6,
    "tt1708695", "2878701","EP00688359","2010-11-08",
    "https://thetvdb.com/banners/episodes/73255/2878701.jpg" },
  {"House", "A Pox on Our House", 7, 7,
    "tt1726380", "2878711","EP00688359","2010-11-15",
    "https://thetvdb.com/banners/episodes/73255/2878711.jpg" },
  {"House", "A Pox on Our House", 7, 7,
    "tt1726380", "2878711","EP00688359","2010-11-15",
    "https://thetvdb.com/banners/episodes/73255/2878711.jpg" },
  {"House", "Larger Than Life", 7, 9,
    "tt1726382", "2861961","EP00688359","2011-01-17",
    "https://thetvdb.com/banners/episodes/73255/2861961.jpg" },
  {"House", "Carrot or Stick", 7, 10,
    "tt1726371", "2861951","EP00688359","2011-01-24",
    "https://thetvdb.com/banners/episodes/73255/2861951.jpg" },
  {"House", "Family Practice", 7, 11,
    "tt1726372", "3473831","EP00688359","2011-02-07",
    "https://thetvdb.com/banners/episodes/73255/3473831.jpg" },
  {"House", "You Must Remember This", 7, 12,
    "tt1726373", "3553891","EP00688359","2011-02-14",
    "https://thetvdb.com/banners/episodes/73255/3553891.jpg" },
  {"House", "You Must Remember This", 7, 12,
    "tt1726373", "3553891","EP00688359","2011-02-14",
    "https://thetvdb.com/banners/episodes/73255/3553891.jpg" },
  {"House", "Two Stories", 7, 13,
    "tt1726374", "3565631","EP00688359","2011-02-21",
    "https://thetvdb.com/banners/episodes/73255/3565631.jpg" },
  {"House", "Recession Proof", 7, 14,
    "tt1726375", "3565641","EP00688359","2011-02-28",
    "https://thetvdb.com/banners/episodes/73255/3565641.jpg" },
  {"House", "Bombshells", 7, 15,
    "tt1726376", "3565651","EP00688359","2011-03-07",
    "https://thetvdb.com/banners/episodes/73255/3565651.jpg" },
  {"House", "Out of the Chute", 7, 16,
    "tt1842688", "3565661","EP00688359","2011-03-14",
    "https://thetvdb.com/banners/episodes/73255/3565661.jpg" },
  {"House", "Fall from Grace", 7, 17,
    "tt1842081", "3565671","EP00688359","2011-03-21",
    "https://thetvdb.com/banners/episodes/73255/3565671.jpg" },
  {"House", "Fall from Grace", 7, 17,
    "tt1842081", "3565671","EP00688359","2011-03-21",
    "https://thetvdb.com/banners/episodes/73255/3565671.jpg" },
  {"House", "Last Temptation", 7, 19,
    "tt1880571", "3934671","EP00688359","2011-04-18",
    "https://thetvdb.com/banners/episodes/73255/3934671.jpg" },
  {"House", "Changes", 7, 20,
    "tt1883788", "3934691","EP00688359","2011-05-02",
    "https://thetvdb.com/banners/episodes/73255/3934691.jpg" },
  {"House", "The Fix", 7, 21,
    "tt1902562", "3934761","EP00688359","2011-05-09",
    "https://thetvdb.com/banners/episodes/73255/3934761.jpg" },
  {"House", "After Hours", 7, 22,
    "tt1900094", "3934781","EP00688359","2011-05-16",
    "https://thetvdb.com/banners/episodes/73255/3934781.jpg" },
  {"House", "Twenty Vicodin", 8, 1,
    "tt2006450", "4120406","EP00688359","2011-10-03",
    "https://thetvdb.com/banners/episodes/73255/4120406.jpg" },
  {"House", "Transplant", 8, 2,
    "tt2015677", "4155773","EP00688359","2011-10-10",
    "https://thetvdb.com/banners/episodes/73255/4155773.jpg" },
  {"House", "Charity Case", 8, 3,
    "tt2016511", "4155774","EP00688359","2011-10-17",
    "https://thetvdb.com/banners/episodes/73255/4155774.jpg" },
  {"House", "Risky Business", 8, 4,
    "tt2021267", "4159274","EP00688359","2011-10-31",
    "https://thetvdb.com/banners/episodes/73255/4159274.jpg" },
  {"House", "The Confession", 8, 5,
    "tt2063276", "4176819","EP00688359","2011-11-07",
    "https://thetvdb.com/banners/episodes/73255/4176819.jpg" },
  {"House", "Parents", 8, 6,
    "tt2084392", "4190490","EP00688359","2011-11-14",
    "https://thetvdb.com/banners/episodes/73255/4190490.jpg" },
  {"House", "Dead & Buried", 8, 7,
    "tt2084393", "4190489","EP00688359","2011-11-21",
    "https://thetvdb.com/banners/episodes/73255/4190489.jpg" },
  {"House", "Perils of Paranoia", 8, 8,
    "tt2084394", "4190488","EP00688359","2011-11-28",
    "https://thetvdb.com/banners/episodes/73255/4190488.jpg" },
  {"House", "Better Half", 8, 9,
    "tt2092629", "4195884","EP00688359","2012-01-23",
    "https://thetvdb.com/banners/episodes/73255/4195884.jpg" },
  {"House", "Runaways", 8, 10,
    "tt2121953", "4217133","EP00688359","2012-01-30",
    "https://thetvdb.com/banners/episodes/73255/4217133.jpg" },
  {"House", "Nobody's Fault", 8, 11,
    "tt2121954", "4217134","EP00688359","2012-02-06",
    "https://thetvdb.com/banners/episodes/73255/4217134.jpg" },
  {"House", "Chase", 8, 12,
    "tt2121955", "4217135","EP00688359","2012-02-13",
    "https://thetvdb.com/banners/episodes/73255/4217135.jpg" },
  {"House", "Chase", 8, 12,
    "tt2121955", "4217135","EP00688359","2012-02-13",
    "https://thetvdb.com/banners/episodes/73255/4217135.jpg" },
  {"House", "Man of the House", 8, 13,
    "tt2121956", "4225825","EP00688359","2012-02-20",
    "https://thetvdb.com/banners/episodes/73255/4225825.jpg" },
  {"House", "Love is Blind", 8, 14,
    "tt2121957", "4225826","EP00688359","2012-03-19",
    "https://thetvdb.com/banners/episodes/73255/4225826.jpg" },
  {"House", "Blowing the Whistle", 8, 15,
    "tt2121958", "4225827","EP00688359","2012-04-02",
    "https://thetvdb.com/banners/episodes/73255/4225827.jpg" },
  {"House", "Gut Check", 8, 16,
    "tt2121959", "4265763","EP00688359","2012-04-09",
    "https://thetvdb.com/banners/episodes/73255/4265763.jpg" },
  {"House", "We Need the Eggs", 8, 17,
    "tt2121960", "4265764","EP00688359","2012-04-16",
    "https://thetvdb.com/banners/episodes/73255/4265764.jpg" },
  {"House", "Body and Soul", 8, 18,
    "tt2121961", "4265765","EP00688359","2012-04-23",
    "https://thetvdb.com/banners/episodes/73255/4265765.jpg" },
  {"House", "The C Word", 8, 19,
    "tt2121962", "4265766","EP00688359","2012-04-30",
    "https://thetvdb.com/banners/episodes/73255/4265766.jpg" },
  {"House", "Post Mortem", 8, 20,
    "tt2121963", "4265767","EP00688359","2012-05-07",
    "https://thetvdb.com/banners/episodes/73255/4265767.jpg" },
  {"House", "Holding On", 8, 21,
    "tt2121964", "4265768","EP00688359","2012-05-14",
    "https://thetvdb.com/banners/episodes/73255/4265768.jpg" },
  {"House", "Everybody Dies", 8, 22,
    "tt2121965", "4265769","EP00688359","2012-05-21",
    "https://thetvdb.com/banners/episodes/73255/4265769.jpg" }
};


static void
check_videos_metadata (GrlSource    *source,
                       guint         operation_id,
                       GrlMedia     *media,
                       gpointer      user_data,
                       const GError *error)
{
  GDateTime *date;
  const gchar *imdb;
  const gchar *tvdb_id;
  const gchar *zap2it;
  const gchar *title;
  const gchar *episode_screen;
  gchar *publication_date;
  gint i = GPOINTER_TO_INT (user_data);

  if (error)
    g_error ("Resolve operation failed. Reason: %s", error->message);

  title = grl_media_get_episode_title (media);
  g_assert_cmpstr (videos[i].title, ==, title);

  imdb = grl_data_get_string (GRL_DATA (media), imdb_key);
  g_assert_cmpstr (videos[i].imdb, ==, imdb);

  tvdb_id = grl_data_get_string (GRL_DATA (media), tvdb_key);
  g_assert_cmpstr (videos[i].tvdb_id, ==, tvdb_id);

  zap2it = grl_data_get_string (GRL_DATA (media), zap2it_key);
  g_assert_cmpstr (videos[i].zap2it_id, ==, zap2it);

  date = grl_media_get_publication_date (GRL_MEDIA (media));
  publication_date = g_date_time_format (date, "%Y-%m-%d");
  g_assert_cmpstr (videos[i].publication_date, ==, publication_date);
  g_free (publication_date);

  episode_screen = grl_data_get_string (GRL_DATA (media), ss_key);
  if (g_strcmp0 (videos[i].url_episode_screen, episode_screen) != 0)
    g_message ("[%s] Episode screen changed from %s to %s",
               videos[i].show, videos[i].url_episode_screen, episode_screen);

  num_tests--;
  if (num_tests == 0)
    g_main_loop_quit (main_loop);
}

static void
test_episodes_stress (void)
{
  GrlSource *source;
  guint i;
  GrlRegistry *registry;
  GrlOperationOptions *options;
  GList *keys;
  GrlCaps *caps;

  source = test_get_source();
  g_assert (source);

  if (!(grl_source_supported_operations (source) & GRL_OP_RESOLVE))
    g_error ("Source is not searchable!");

  registry = grl_registry_get_default ();

  tvdb_key = grl_registry_lookup_metadata_key (registry, "thetvdb-id");
  imdb_key = grl_registry_lookup_metadata_key (registry, "thetvdb-imdb-id");
  zap2it_key = grl_registry_lookup_metadata_key (registry, "thetvdb-zap2it-id");
  ss_key = grl_registry_lookup_metadata_key (registry, "thetvdb-episode-screenshot");

  g_assert_cmpint (tvdb_key, !=, GRL_METADATA_KEY_INVALID);
  g_assert_cmpint (imdb_key, !=, GRL_METADATA_KEY_INVALID);
  g_assert_cmpint (zap2it_key, !=, GRL_METADATA_KEY_INVALID);
  g_assert_cmpint (ss_key, !=, GRL_METADATA_KEY_INVALID);

  caps = grl_source_get_caps (source, GRL_OP_RESOLVE);
  options = grl_operation_options_new (caps);
  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_NORMAL);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_EPISODE_TITLE,
                                    GRL_METADATA_KEY_PUBLICATION_DATE,
                                    tvdb_key,
                                    imdb_key,
                                    zap2it_key,
                                    ss_key,
                                    GRL_METADATA_KEY_INVALID);

  for (i = 0; i < G_N_ELEMENTS (videos); i++) {
    GrlMedia *video;

    video = grl_media_video_new ();
    grl_media_set_show (video, videos[i].show);
    grl_media_set_season (video, videos[i].season);
    grl_media_set_episode (video, videos[i].episode);

    grl_source_resolve (source,
                        video,
                        keys,
                        options,
                        check_videos_metadata,
                        GINT_TO_POINTER (i));

    g_object_unref (video);
  }
  g_object_unref (options);
  g_object_unref (caps);
  g_list_free (keys);

  num_tests = i;
  main_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (main_loop);
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

  g_test_add_func ("/thetvdb/resolve/episodes_from_episode", test_episodes_stress);

  result = g_test_run ();

  test_shutdown_thetvdb ();

  grl_deinit ();

  return result;
}
