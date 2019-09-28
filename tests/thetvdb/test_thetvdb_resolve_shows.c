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

/* Returns whether it could find tvdb-id */
static gboolean
get_show_metadata (GrlSource *source,
                   GrlResolutionFlags resolution,
                   gchar **show,
                   gchar **imdb,
                   gchar **tvdb_id,
                   gchar **zap2it,
                   gchar **publication_date,
                   gchar **banner,
                   gchar **fanart,
                   gchar **poster)
{
  GrlMedia *video;
  GrlOperationOptions *options;
  GList *keys;
  GDateTime *date;
  GrlRegistry *registry;
  GrlKeyID tvdb_key, imdb_key, zap2it_key, fanart_key, banner_key, poster_key;
  gboolean success = TRUE;

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

  video = grl_media_video_new ();
  grl_media_set_show (video, *show);
  g_free (*show);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_SHOW,
                                    GRL_METADATA_KEY_PUBLICATION_DATE,
                                    tvdb_key,
                                    imdb_key,
                                    zap2it_key,
                                    fanart_key,
                                    banner_key,
                                    poster_key,
                                    NULL);

  options = grl_operation_options_new (NULL);
  grl_operation_options_set_resolution_flags (options, resolution);

  grl_source_resolve_sync (source,
                           GRL_MEDIA (video),
                           keys,
                           options,
                           NULL);

  *show = g_strdup (grl_media_get_show (video));

  if (tvdb_id) {
      *tvdb_id = g_strdup (grl_data_get_string (GRL_DATA (video), tvdb_key));
      if (*tvdb_id == NULL) {
        success = FALSE;
        goto end;
      }
  }

  if (imdb)
      *imdb = g_strdup (grl_data_get_string (GRL_DATA (video), imdb_key));
  if (zap2it)
      *zap2it = g_strdup (grl_data_get_string (GRL_DATA (video), zap2it_key));
  if (banner)
      *banner = g_strdup (grl_data_get_string (GRL_DATA (video), banner_key));
  if (fanart)
      *fanart = g_strdup (grl_data_get_string (GRL_DATA (video), fanart_key));
  if (poster)
      *poster = g_strdup (grl_data_get_string (GRL_DATA (video), poster_key));

  if (publication_date) {
      date = grl_media_get_publication_date (GRL_MEDIA (video));
      *publication_date = g_date_time_format (date, "%Y-%m-%d");
  }

end:
  g_list_free (keys);
  g_object_unref (options);
  g_object_unref (video);
  return success;
}

static void
test_shows (GrlResolutionFlags resolution,
            gboolean should_fail)
{
  GrlSource *source;
  guint i;

  struct {
    const gchar *show;
    const gchar *imdb;
    const gchar *tvdb_id;
    const gchar *zap2it_id;
    const gchar *publication_date;
    const gchar *url_banner;
    const gchar *url_fanart;
    const gchar *url_poster;
  } videos[] = {
    { "Boardwalk Empire", "tt0979432", "84947", "SH01308836", "2010-09-19",
      "https://thetvdb.com/banners/graphical/84947-g6.jpg",
      "https://thetvdb.com/banners/fanart/original/84947-12.jpg",
      "https://thetvdb.com/banners/posters/84947-6.jpg" },
    { "Adventure Time", "tt1305826", "152831", "EP01246265", "2010-04-05",
      "https://thetvdb.com/banners/graphical/152831-g2.jpg",
      "https://thetvdb.com/banners/fanart/original/152831-13.jpg",
      "https://thetvdb.com/banners/posters/152831-2.jpg" },
    { "Felicity", "tt0134247", "73980", "SH184561", "1998-09-29",
      "https://thetvdb.com/banners/graphical/253-g.jpg",
      "https://thetvdb.com/banners/fanart/original/73980-3.jpg",
      "https://thetvdb.com/banners/posters/73980-4.jpg" },
    { "House", "tt0412142", "73255", "EP00688359", "2004-11-16",
      "https://thetvdb.com/banners/graphical/73255-g22.jpg",
      "https://thetvdb.com/banners/fanart/original/73255-47.jpg",
      "https://thetvdb.com/banners/posters/73255-13.jpg" },
    { "Naruto", "tt0409591", "78857", "SH774951", "2002-10-03",
      "https://thetvdb.com/banners/graphical/78857-g3.jpg",
      "https://thetvdb.com/banners/fanart/original/78857-37.jpg",
      "https://thetvdb.com/banners/posters/78857-10.jpg" }
  };

  source = test_get_source ();
  g_assert (source);

  for (i = 0; i < G_N_ELEMENTS (videos); i++) {
    gchar *show, *imdb, *tvdb_id, *zap2it, *pdate, *banner, *fanart, *poster;
    gboolean success;

    show = g_utf8_casefold (videos[i].show, -1);
    success = get_show_metadata (source, resolution, &show,
                                 &imdb, &tvdb_id, &zap2it, &pdate,
                                 &banner, &fanart, &poster);

    if (should_fail) {
      g_assert_false (success);
      continue;
    }

    g_assert_cmpstr (videos[i].show, ==, show);
    g_free (show);
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

/* As the net is mocked we ensure that request with fuzzy name returns the same
 * data that the correct name in tvdb's database. Note that tvdb has its own
 * fuzzy name handling and the response could change in the future;
 * Current wiki for GetSeries API says:
 * "This is the string you want to search for. If there is an exact match for
 *  the parameter, it will be the first result returned."
 */
static void
test_shows_fuzzy_name (void)
{
  GrlSource *source;
  guint i;

  struct {
    const gchar *name_in_tvdb;
    const gchar *fuzzy_name;
    const gchar *tvdb_id;
    const gchar *imdb;
  } videos[] = {
    { "CSI: Miami", "CSI - Miami", "78310", "tt0313043" },
    { "CSI: Miami", "les experts miami", "78310", "tt0313043" }
  };

  test_reset_thetvdb ();
  source = test_get_source ();
  g_assert (source);

  /* First we search and populate the db using the fuzzy name and then we do
   * a cache-only request with both, correct and fuzzy name */
  for (i = 0; i < G_N_ELEMENTS (videos); i++) {
    gchar *imdb, *tvdb_id, *show;

    show = g_strdup (videos[i].fuzzy_name);
    get_show_metadata (source, GRL_RESOLVE_NORMAL, &show, &imdb, &tvdb_id,
                       NULL, NULL, NULL, NULL, NULL);
    g_assert_cmpstr (videos[i].tvdb_id, ==, tvdb_id);
    g_free (tvdb_id);
    g_assert_cmpstr (videos[i].imdb, ==, imdb);
    g_free (imdb);
    g_free (show);

    show = g_strdup (videos[i].name_in_tvdb);
    get_show_metadata (source, GRL_RESOLVE_FAST_ONLY, &show, &imdb, &tvdb_id,
                       NULL, NULL, NULL, NULL, NULL);
    g_assert_cmpstr (videos[i].tvdb_id, ==, tvdb_id);
    g_free (tvdb_id);
    g_assert_cmpstr (videos[i].imdb, ==, imdb);
    g_free (imdb);
    g_free (show);

    show = g_strdup (videos[i].fuzzy_name);
    get_show_metadata (source, GRL_RESOLVE_FAST_ONLY, &show, &imdb, &tvdb_id,
                       NULL, NULL, NULL, NULL, NULL);
    g_assert_cmpstr (videos[i].tvdb_id, ==, tvdb_id);
    g_free (tvdb_id);
    g_assert_cmpstr (videos[i].imdb, ==, imdb);
    g_free (imdb);
    g_free (show);
  }
}

static void
test_shows_normal(void)
{
  test_reset_thetvdb ();
  test_shows (GRL_RESOLVE_NORMAL, FALSE);
}

static void
test_shows_fast_only_empty_db(void)
{
  test_reset_thetvdb ();
  test_shows (GRL_RESOLVE_FAST_ONLY, TRUE);
}

static void
test_shows_fast_only_full_db(void)
{
  gchar *mock = g_strdup (g_getenv ("GRL_NET_MOCKED"));
  test_reset_thetvdb ();
  /* Fill database */
  test_shows (GRL_RESOLVE_NORMAL, FALSE);
  /* Fail all web requests for cache-only test */
  g_setenv ("GRL_NET_MOCKED", "/does/not/exist/config.ini", TRUE);
  test_shows (GRL_RESOLVE_FAST_ONLY, FALSE);
  g_setenv ("GRL_NET_MOCKED", mock, TRUE);
  g_free (mock);
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

  g_test_add_func ("/thetvdb/resolve/normal/shows", test_shows_normal);
  g_test_add_func ("/thetvdb/resolve/fast-only/empty-db/shows", test_shows_fast_only_empty_db);
  g_test_add_func ("/thetvdb/resolve/fast-only/full-db/shows", test_shows_fast_only_full_db);
  g_test_add_func ("/thetvdb/resolve/fuzzy-name-shows", test_shows_fuzzy_name);

  result = g_test_run ();

  test_shutdown_thetvdb ();

  grl_deinit ();

  return result;
}
