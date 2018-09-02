/*
 * Copyright (C) 2016 Grilo Project
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

#include "test_lua_factory_utils.h"

#define ACOUSTID_ID  "grl-acoustid"
#define CHROMAPRINT_ID "grl-chromaprint"

#define TEST_PLUGINS_PATH  LUA_FACTORY_PLUGIN_PATH ";" CHROMAPRINT_PLUGIN_PATH
#define TEST_PLUGINS_LOAD  LUA_FACTORY_ID ":" CHROMAPRINT_ID

#define ACOUSTID_OPS GRL_OP_RESOLVE

#define GRESOURCE_PREFIX "resource:///org/gnome/grilo/plugins/test/acoustid/data/"

#define FINGERPRINT_LUDOVICO_EI  GRESOURCE_PREFIX "chromaprint_ludovico_einaudi_primavera.txt"
#define FINGERPRINT_NORAH_JONES  GRESOURCE_PREFIX "chromaprint_norah_jones_chasing_pirates.txt"
#define FINGERPRINT_PHILIP_GLAS  GRESOURCE_PREFIX "chromaprint_philip_glass_the_passion_of.txt"
#define FINGERPRINT_TROMBONE_SH  GRESOURCE_PREFIX "chromaprint_trombone_shorty_buckjump.txt"
#define FINGERPRINT_RADIOHEAD_PA GRESOURCE_PREFIX "chromaprint_radiohead_paranoid_android.txt"

static gchar *
resolve (GrlSource   *source,
         const gchar *fingerprint,
         gint         duration,
         gchar      **out_mb_artist_id,
         gchar      **out_artist,
         gchar      **out_mb_album_id,
         gchar      **out_album,
         gchar      **out_mb_recording_id,
         gchar      **out_title,
         gchar      **out_mb_release_id,
         gchar      **out_mb_release_group_id,
	 gint        *out_album_disc_number,
	 gchar      **out_publication_date,
	 gint        *out_track_number)
{
  GList *keys;
  GrlMedia *audio;
  GrlOperationOptions *options;
  GrlRegistry *registry;
  GrlKeyID chromaprint_key, mb_release_id_key, mb_release_group_id_key;
  GDateTime *date;
  GError *error = NULL;

  registry = grl_registry_get_default ();
  chromaprint_key = grl_registry_lookup_metadata_key (registry, "chromaprint");
  g_assert_cmpint (chromaprint_key, !=, GRL_METADATA_KEY_INVALID);

  audio = grl_media_audio_new ();
  grl_data_set_string (GRL_DATA (audio), chromaprint_key, fingerprint);
  grl_media_set_duration (audio, duration);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_MB_ARTIST_ID,
                                    GRL_METADATA_KEY_ARTIST,
                                    GRL_METADATA_KEY_MB_ALBUM_ID,
                                    GRL_METADATA_KEY_ALBUM,
                                    GRL_METADATA_KEY_MB_RECORDING_ID,
                                    GRL_METADATA_KEY_TITLE,
				    GRL_METADATA_KEY_ALBUM_DISC_NUMBER,
				    GRL_METADATA_KEY_PUBLICATION_DATE,
				    GRL_METADATA_KEY_TRACK_NUMBER,
                                    NULL);
  options = grl_operation_options_new (NULL);
  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_NORMAL);

  grl_source_resolve_sync (source,
                           GRL_MEDIA (audio),
                           keys,
                           options,
                           &error);
  g_assert_no_error (error);

  mb_release_id_key = grl_registry_lookup_metadata_key (registry, "mb-release-id");
  g_assert_cmpint (mb_release_id_key, !=, GRL_METADATA_KEY_INVALID);
  mb_release_group_id_key = grl_registry_lookup_metadata_key (registry, "mb-release-group-id");
  g_assert_cmpint (mb_release_group_id_key, !=, GRL_METADATA_KEY_INVALID);

  *out_mb_artist_id = g_strdup (grl_media_get_mb_artist_id (audio));
  *out_artist = g_strdup (grl_media_get_artist (audio));
  *out_mb_album_id = g_strdup (grl_media_get_mb_album_id (audio));
  *out_album = g_strdup (grl_media_get_album (audio));
  *out_mb_recording_id = g_strdup (grl_media_get_mb_recording_id (audio));
  *out_title = g_strdup (grl_media_get_title (audio));
  *out_mb_release_id = g_strdup (grl_data_get_string (GRL_DATA (audio), mb_release_id_key));
  *out_mb_release_group_id = g_strdup (grl_data_get_string (GRL_DATA (audio),
                                                            mb_release_group_id_key));
  *out_album_disc_number = grl_media_get_album_disc_number (audio);
  date = grl_media_get_publication_date (audio);
  *out_publication_date = g_date_time_format (date, "%Y-%m-%d");
  *out_track_number = grl_media_get_track_number (audio);

  g_list_free (keys);
  g_object_unref (options);
  g_object_unref (audio);
  return NULL;
}

static void
test_resolve_fingerprint (void)
{
  GrlSource *source;
  guint i;

  struct {
    gchar *fingerprint_file;
    gint   duration;
    gchar *mb_artist_id;
    gchar *artist;
    gchar *mb_album_id;
    gchar *album;
    gchar *mb_recording_id;
    gchar *title;
    gchar *mb_release_id;
    gint album_disc_number;
    gchar *publication_date;
    gint track_number;
  } audios[] = {
   { FINGERPRINT_LUDOVICO_EI, 445,
     "e7d8aea3-9c1d-4fe0-b93a-481d545296fc", "Craig Ogden",
     "2cde60bc-829c-49af-a62c-e20283167c30", "Classic FM Summer Guitar",
     "5d72b7d4-d0c4-4d0d-ab7f-3a737075e1c9", "Primavera",
     "5fd10cc8-30e0-48aa-9ba1-19d05b871a75",
     1, "2009-01-01", 8 },
   { FINGERPRINT_NORAH_JONES, 160,
     "985c709c-7771-4de3-9024-7bda29ebe3f9", "Norah Jones",
     "f5cffa96-262c-49af-9747-3f04a1d42c78", "\u00d63 Greatest Hits 49",
     "6d8ba615-d8fe-4f99-b38f-0a17d657b1bb", "Chasing Pirates",
     "1ee64f6c-560f-421f-83dc-7fd34e5b0674",
     1, "2010-03-12", 18 },
   { FINGERPRINT_TROMBONE_SH, 243,
     "cae4fd51-4d58-4d48-92c1-6198cc2e45ed", "Trombone Shorty",
     "c3418122-387b-4477-90cf-e5e6d110e054", "For True",
     "96483bdd-f219-4ae3-a94e-04feeeef22a4", "Buckjump",
     "567621e3-b80f-4c30-af5f-2ecf0882e94a",
     1, "2011-01-01", 1 },
   { FINGERPRINT_PHILIP_GLAS, 601,
     "5ae54dee-4dba-49c0-802a-a3b3b3adfe9b", "Philip Glass",
     "52f1f9d5-5166-4ceb-9289-6fb1a87f367c", "The Passion of Ramakrishna",
     "298e15a1-b29b-4947-9dca-ec3634f9ebde", "Part 2",
     "2807def3-7873-4277-b079-c9a963d99993",
     1, "2012-01-01", 3 },
   { FINGERPRINT_RADIOHEAD_PA, 385,
     "a74b1b7f-71a5-4011-9441-d0b5e4122711", "Radiohead",
     "5156ef3b-6d06-46d9-874d-4e7e41ef3be0", "ROCK DA PLACE",
     "5188ac5f-6000-483a-b689-b4773d0b1afa", "Paranoid Android",
     "f9f6ab9e-65da-4e90-853f-23960b04736a",
     1, "2001-01-01", 16,
   },
  };

  source = test_lua_factory_get_source (ACOUSTID_ID, ACOUSTID_OPS);
  for (i = 0; i < G_N_ELEMENTS (audios); i++) {
    gchar *data;
    GFile *file;
    gsize size;
    GError *error = NULL;
    gchar *mb_artist_id, *artist, *mb_album_id, *album, *mb_recording_id, *title,
          *mb_release_id, *mb_release_group_id, *publication_date;
    gint album_disc_number, track_number;

    file = g_file_new_for_uri (audios[i].fingerprint_file);
    g_file_load_contents (file, NULL, &data, &size, NULL, &error);
    g_assert_no_error (error);
    g_clear_pointer (&file, g_object_unref);

    resolve (source, data, audios[i].duration,
             &mb_artist_id, &artist, &mb_album_id, &album, &mb_recording_id, &title,
             &mb_release_id, &mb_release_group_id, &album_disc_number, &publication_date,
	     &track_number);
    g_free (data);

    g_assert_cmpstr (audios[i].title, ==, title);
    g_free (title);
    g_assert_cmpstr (audios[i].mb_artist_id , ==, mb_artist_id);
    g_free (mb_artist_id);
    g_assert_cmpstr (audios[i].artist, ==, artist);
    g_free (artist);
    g_assert_cmpstr (audios[i].mb_album_id, ==, mb_album_id);
    g_free (mb_album_id);
    g_assert_cmpstr (audios[i].album, ==, album);
    g_free (album);
    g_assert_cmpstr (audios[i].mb_recording_id, ==, mb_recording_id);
    g_free (mb_recording_id);
    g_assert_cmpstr (audios[i].mb_release_id, ==, mb_release_id);
    g_free (mb_release_id);
    g_assert_cmpstr (audios[i].mb_album_id, ==, mb_release_group_id);
    g_free (mb_release_group_id);
    g_assert_cmpint (audios[i].album_disc_number, ==, album_disc_number);
    g_assert_cmpstr (audios[i].publication_date, ==, publication_date);
    g_assert_cmpint (audios[i].track_number, ==, track_number);
  }
}

static void
test_acoustid_setup (gint *p_argc,
                     gchar ***p_argv)
{
  GrlRegistry *registry;
  GrlConfig *config;
  GError *error = NULL;

  g_setenv ("GRL_PLUGIN_PATH", TEST_PLUGINS_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", TEST_PLUGINS_LOAD, TRUE);
  g_setenv ("GRL_LUA_SOURCES_PATH", LUA_FACTORY_SOURCES_PATH, TRUE);
  g_setenv ("GRL_NET_MOCKED", LUA_FACTORY_SOURCES_DATA_PATH "config.ini", TRUE);

  grl_init (p_argc, p_argv);
  g_test_init (p_argc, p_argv, NULL);

  /* This test uses 'chromaprint' metadata-key which is created by
   * Chromaprint plugin. For that reason, we need to load and activate it. */
  registry = grl_registry_get_default ();
  grl_registry_load_plugin (registry, CHROMAPRINT_PLUGIN_PATH "/libgrlchromaprint.so", &error);
  g_assert_no_error (error);
  grl_registry_activate_plugin_by_id (registry, CHROMAPRINT_ID, &error);
  g_assert_no_error (error);

  config = grl_config_new (LUA_FACTORY_ID, ACOUSTID_ID);
  grl_config_set_api_key (config, "ACOUSTID_TEST_MOCK_API_KEY");
  test_lua_factory_setup (config);
}

gint
main (gint argc, gchar **argv)
{
  test_acoustid_setup (&argc, &argv);

  g_test_add_func ("/lua_factory/sources/acoustid", test_resolve_fingerprint);

  gint result = g_test_run ();

  test_lua_factory_shutdown ();
  test_lua_factory_deinit ();

  return result;
}
