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
#include <grilo.h>

static void
get_show_metadata (GrlSource *source,
                   const gchar *show,
                   gchar **imdb,
                   gchar **tvdb_id,
                   gchar **zap2it,
                   gchar **publication_date,
                   gchar **banner,
                   gchar **fanart,
                   gchar **poster)
{
  GrlMediaVideo *video;
  GrlOperationOptions *options;
  GList *keys;
  GDateTime *date;
  GrlRegistry *registry;
  GrlKeyID tvdb_key, imdb_key, zap2it_key, fanart_key, banner_key, poster_key;

  registry = grl_registry_get_default ();

  tvdb_key = grl_registry_lookup_metadata_key (registry, "thetvdb-id");
  imdb_key = grl_registry_lookup_metadata_key (registry, "thetvdb-imdb-id");
  zap2it_key = grl_registry_lookup_metadata_key (registry, "thetvdb-zap2it-id");
  fanart_key = grl_registry_lookup_metadata_key (registry, "thetvdb-fanart");
  banner_key = grl_registry_lookup_metadata_key (registry, "thetvdb-banner");
  poster_key = grl_registry_lookup_metadata_key (registry, "thetvdb-poster");

  g_assert_cmpint (tvdb_key, !=, GRL_METADATA_KEY_INVALID);
  g_assert_cmpint (imdb_key, !=, GRL_METADATA_KEY_INVALID);
  g_assert_cmpint (zap2it_key, !=, GRL_METADATA_KEY_INVALID);
  g_assert_cmpint (fanart_key, !=, GRL_METADATA_KEY_INVALID);
  g_assert_cmpint (banner_key, !=, GRL_METADATA_KEY_INVALID);
  g_assert_cmpint (poster_key, !=, GRL_METADATA_KEY_INVALID);

  video = GRL_MEDIA_VIDEO (grl_media_video_new ());
  grl_media_video_set_show (video, show);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_PUBLICATION_DATE,
                                    tvdb_key,
                                    imdb_key,
                                    zap2it_key,
                                    fanart_key,
                                    banner_key,
                                    poster_key,
                                    NULL);

  options = grl_operation_options_new (NULL);
  grl_operation_options_set_flags (options, GRL_RESOLVE_NORMAL);

  grl_source_resolve_sync (source,
                           GRL_MEDIA (video),
                           keys,
                           options,
                           NULL);
  *imdb = g_strdup (grl_data_get_string (GRL_DATA (video), imdb_key));
  *tvdb_id = g_strdup (grl_data_get_string (GRL_DATA (video), tvdb_key));
  *zap2it = g_strdup (grl_data_get_string (GRL_DATA (video), zap2it_key));
  *banner = g_strdup (grl_data_get_string (GRL_DATA (video), banner_key));
  *fanart = g_strdup (grl_data_get_string (GRL_DATA (video), fanart_key));
  *poster = g_strdup (grl_data_get_string (GRL_DATA (video), poster_key));

  date = grl_media_get_publication_date (GRL_MEDIA (video));
  *publication_date = g_date_time_format (date, "%Y-%m-%d");

  g_list_free (keys);
  g_object_unref (options);
  g_object_unref (video);
}

static void
test_shows (void)
{
  GrlSource *source;
  guint i;

  struct {
    gchar *show;
    gchar *imdb;
    gchar *tvdb_id;
    gchar *zap2it_id;
    gchar *publication_date;
    gchar *url_banner;
    gchar *url_fanart;
    gchar *url_poster;
  } videos[] = {
    { "Boardwalk Empire", "tt0979432", "84947", "SH01308836", "2010-09-19",
      "http://thetvdb.com/banners/graphical/84947-g6.jpg",
      "http://thetvdb.com/banners/fanart/original/84947-12.jpg",
      "http://thetvdb.com/banners/posters/84947-6.jpg" },
    { "Adventure Time", "tt1305826", "152831", "EP01246265", "2010-04-05",
      "http://thetvdb.com/banners/graphical/152831-g2.jpg",
      "http://thetvdb.com/banners/fanart/original/152831-13.jpg",
      "http://thetvdb.com/banners/posters/152831-2.jpg" },
    { "Felicity", "tt0134247", "73980", "SH184561", "1998-09-29",
      "http://thetvdb.com/banners/graphical/253-g.jpg",
      "http://thetvdb.com/banners/fanart/original/73980-3.jpg",
      "http://thetvdb.com/banners/posters/73980-4.jpg" },
    { "House", "tt0412142", "73255", "EP00688359", "2004-11-16",
      "http://thetvdb.com/banners/graphical/73255-g22.jpg",
      "http://thetvdb.com/banners/fanart/original/73255-47.jpg",
      "http://thetvdb.com/banners/posters/73255-13.jpg" },
    { "Naruto", "tt0409591", "78857", "SH774951", "2002-10-03",
      "http://thetvdb.com/banners/graphical/78857-g3.jpg",
      "http://thetvdb.com/banners/fanart/original/78857-37.jpg",
      "http://thetvdb.com/banners/posters/78857-10.jpg" }
  };

  source = test_get_source ();
  g_assert (source);

  for (i = 0; i < G_N_ELEMENTS (videos); i++) {
    gchar *imdb, *tvdb_id, *zap2it, *pdate, *banner, *fanart, *poster;

    get_show_metadata (source, videos[i].show,
                       &imdb, &tvdb_id, &zap2it, &pdate,
                       &banner, &fanart, &poster);
    g_assert_cmpstr (videos[i].tvdb_id, ==, tvdb_id);
    g_free (tvdb_id);
    g_assert_cmpstr (videos[i].imdb, ==, imdb);
    g_free (imdb);
    g_assert_cmpstr (videos[i].zap2it_id, ==, zap2it);
    g_free (zap2it);
    g_assert_cmpstr (videos[i].publication_date, ==, pdate);
    g_free (pdate);
    /* Those urls may change due up/down vote at tvdb system,
     * just shot a message if they change, do not fail the test. */
    if (g_strcmp0 (videos[i].url_banner, banner) != 0)
      g_message ("[%s] banner changed from %s to %s",
                 videos[i].show, videos[i].url_banner, banner);
    g_free (banner);

    if (g_strcmp0 (videos[i].url_fanart, fanart) != 0)
      g_message ("[%s] fanart changed from %s to %s",
                 videos[i].show, videos[i].url_fanart, fanart);
    g_free (fanart);

    if (g_strcmp0 (videos[i].url_poster, poster) != 0)
      g_message ("[%s] poster changed from %s to %s",
                 videos[i].show, videos[i].url_fanart, fanart);
    g_free (poster);
  }
}

gint
main (gint argc, gchar **argv)
{
  g_setenv ("GRL_PLUGIN_PATH", THETVDB_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", THETVDB_ID, TRUE);
  g_setenv ("GRL_NET_MOCKED", THETVDB_PLUGIN_TEST_DATA_PATH "config.ini", TRUE);

  grl_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

#if !GLIB_CHECK_VERSION(2,32,0)
  g_thread_init (NULL);
#endif

  test_setup_thetvdb ();

  g_test_add_func ("/thetvdb/resolve/shows", test_shows);

  gint result = g_test_run ();

  test_shutdown_thetvdb ();

  grl_deinit ();

  return result;
}
