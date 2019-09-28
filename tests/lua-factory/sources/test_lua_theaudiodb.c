/*
 * Copyright (C) 2019 Grilo Project
 *
 * Author: Jean Felder <jfelder@gnome.org>
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
#include "test_lua_factory_utils.h"

#define THEAUDIODB_ID  "grl-theaudiodb-cover"

#define TEST_PLUGINS_PATH  LUA_FACTORY_PLUGIN_PATH
#define TEST_PLUGINS_LOAD  LUA_FACTORY_ID

#define THEAUDIODB_OPS GRL_OP_RESOLVE

static void
test_resolve_album_cover (void)
{
  guint i, expected_nr_thumbnails;
  GList *keys;
  GrlMedia *media;
  GrlOperationOptions *options;
  GrlSource *source;
  GError *error = NULL;

  struct {
    gchar *artist;
    gchar *album;
    guint nr_thumbnails;
    gchar *first_thumbnail;
    gchar *second_thumbnail;
  } audios[] = {
   { "pixies", "doolittle",
     2,
     "https://www.theaudiodb.com/images/media/album/thumb/doolittle-4e3a8a18cb017.jpg",
     "https://www.theaudiodb.com/images/media/album/cdart/doolittle-4e73926514e07.png"
   },
   {
     "nirvana", "nevermind",
     2,
     "https://www.theaudiodb.com/images/media/album/thumb/nevermind-4dcdd240da9e2.jpg",
     "https://www.theaudiodb.com/images/media/album/cdart/nevermind-5232416c46820.png"
   },
  };

  source = test_lua_factory_get_source (THEAUDIODB_ID, THEAUDIODB_OPS);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_THUMBNAIL);
  options = grl_operation_options_new (NULL);
  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_NORMAL);

  for (i = 0; i < G_N_ELEMENTS (audios); i++) {
    media = grl_media_audio_new ();
    grl_media_set_album (media, audios[i].album);
    grl_media_set_artist (media, audios[i].artist);

    grl_source_resolve_sync (source,
                             GRL_MEDIA (media),
                             keys,
                             options,
                             &error);
    g_assert_no_error (error);

    expected_nr_thumbnails = grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_THUMBNAIL);
    g_assert_cmpuint (expected_nr_thumbnails, ==, audios[i].nr_thumbnails);

    g_assert_cmpstr (grl_media_get_thumbnail_nth (GRL_MEDIA (media), 0), ==, audios[i].first_thumbnail);
    g_assert_cmpstr (grl_media_get_thumbnail_nth (GRL_MEDIA (media), 1), ==, audios[i].second_thumbnail);

    g_object_unref (media);
  }

  g_list_free (keys);
  g_object_unref (options);
}

static void
test_resolve_artist_art (void)
{
  guint i, j, expected_nr_thumbnails;
  GList *keys;
  GrlMedia *media;
  GrlOperationOptions *options;
  GrlSource *source;
  GError *error = NULL;

  struct {
    gchar *artist;
    int nr_thumbnails;
    gchar *thumbnails[5];
  } audios[] = {
   { "coldplay",
     5,
     { "https://www.theaudiodb.com/images/media/artist/thumb/uxrqxy1347913147.jpg",
       "https://www.theaudiodb.com/images/media/artist/clearart/ruyuwv1510827568.png",
       "https://www.theaudiodb.com/images/media/artist/fanart/spvryu1347980801.jpg",
       "https://www.theaudiodb.com/images/media/artist/fanart/uupyxx1342640221.jpg",
       "https://www.theaudiodb.com/images/media/artist/fanart/qstpsp1342640238.jpg"
     }
   },
   {
     "system of a down",
     4,
     { "https://www.theaudiodb.com/images/media/artist/thumb/tutqyy1340536730.jpg",
       "https://www.theaudiodb.com/images/media/artist/fanart/system-of-a-down-4def4cf79b8f7.jpg",
       "https://www.theaudiodb.com/images/media/artist/fanart/system-of-a-down-4ddaf61d98ce2.jpg",
       "https://www.theaudiodb.com/images/media/artist/fanart/system-of-a-down-4def4d1583fcc.jpg"
     }
   },
  };

  source = test_lua_factory_get_source (THEAUDIODB_ID, THEAUDIODB_OPS);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_THUMBNAIL);
  options = grl_operation_options_new (NULL);
  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_NORMAL);

  for (i = 0; i < G_N_ELEMENTS (audios); i++) {
    media = grl_media_audio_new ();
    grl_media_set_artist (media, audios[i].artist);

    grl_source_resolve_sync (source,
                             GRL_MEDIA (media),
                             keys,
                             options,
                             &error);
    g_assert_no_error (error);

    expected_nr_thumbnails = grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_THUMBNAIL);
    g_assert_cmpuint (expected_nr_thumbnails, ==, audios[i].nr_thumbnails);

    for (j = 0; j < expected_nr_thumbnails; j++) {
      g_assert_cmpstr (grl_media_get_thumbnail_nth (GRL_MEDIA (media), j), ==, audios[i].thumbnails[j]);
    }

    g_object_unref (media);
  }

  g_list_free (keys);
  g_object_unref (options);
}

static void
test_theaudiodb_setup (gint *p_argc,
                       gchar ***p_argv)
{
  GrlConfig *config;

  g_setenv ("GRL_PLUGIN_PATH", TEST_PLUGINS_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", TEST_PLUGINS_LOAD, TRUE);
  g_setenv ("GRL_LUA_SOURCES_PATH", LUA_FACTORY_SOURCES_PATH, TRUE);
  g_setenv ("GRL_NET_MOCKED", LUA_FACTORY_SOURCES_DATA_PATH "config.ini", TRUE);

  grl_init (p_argc, p_argv);
  g_test_init (p_argc, p_argv, NULL);

  config = grl_config_new (LUA_FACTORY_ID, THEAUDIODB_ID);
  grl_config_set_api_key (config, "THEAUDIODB_TEST_MOCK_API_KEY");
  test_lua_factory_setup (config);
}

gint
main (gint argc, gchar **argv)
{
  setlocale (LC_ALL, "");

  test_theaudiodb_setup (&argc, &argv);

  g_test_add_func ("/lua_factory/sources/theaudiodb/resolve/albumcover",
                   test_resolve_album_cover);

  g_test_add_func ("/lua_factory/sources/theaudiodb/resolve/artistart",
                   test_resolve_artist_art);

  gint result = g_test_run ();

  test_lua_factory_shutdown ();
  test_lua_factory_deinit ();

  return result;
}
