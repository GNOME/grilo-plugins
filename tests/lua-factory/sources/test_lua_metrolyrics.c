/*
 * Copyright (C) 2015. All rights reserved.
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

#define METROLYRICS_ID  "grl-metrolyrics"
#define METROLYRICS_OPS GRL_OP_RESOLVE

#define GRESOURCE_PREFIX "resource:///org/gnome/grilo/plugins/test/metrolyrics/data/"

#define LYRICS_RING_OF_FIRE      GRESOURCE_PREFIX "lyrics_ring_of_fire.txt"
#define LYRICS_BACK_IT_UP        GRESOURCE_PREFIX "lyrics_back_it_up.txt"
#define LYRICS_BOHEMIAN_RHAPSODY GRESOURCE_PREFIX "lyrics_bohemian_rhapsody.txt"
#define LYRICS_NOBODYS_PERFECT   GRESOURCE_PREFIX "lyrics_nobodys_perfect.txt"

static gchar *
get_lyrics (GrlSource *source,
            const gchar *artist,
            const gchar *title)
{
  GList *keys;
  GrlMedia *audio;
  GrlOperationOptions *options;
  GError *error = NULL;
  gchar *lyrics;

  audio = grl_media_audio_new ();
  grl_media_set_artist (audio, artist);
  grl_media_set_title (audio, title);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_LYRICS, NULL);
  options = grl_operation_options_new (NULL);
  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_NORMAL);

  grl_source_resolve_sync (source,
                           GRL_MEDIA (audio),
                           keys,
                           options,
                           &error);
  g_assert_no_error (error);

  lyrics = g_strdup (grl_media_get_lyrics (audio));

  g_list_free (keys);
  g_object_unref (options);
  g_object_unref (audio);
  return lyrics;
}

static void
test_resolve_metrolyrics (void)
{
  GrlSource *source;
  guint i;

  struct {
    gchar *title;
    gchar *artist;
    gchar *lyrics_file;
  } audios[] = {
   { "ring of fire", "johnny cash", LYRICS_RING_OF_FIRE },
   { "back it up", "caro emerald", LYRICS_BACK_IT_UP },
   { "bohemian rhapsody", "queen", LYRICS_BOHEMIAN_RHAPSODY },
   { "nobodys perfect", "jessie j", LYRICS_NOBODYS_PERFECT },
   { "100% pure love", "crystal waters", NULL },
  };

  source = test_lua_factory_get_source (METROLYRICS_ID, METROLYRICS_OPS);

  for (i = 0; i < G_N_ELEMENTS (audios); i++) {
    gchar *lyrics, *data;
    GFile *file;
    gsize size;
    GError *error = NULL;

    lyrics = get_lyrics (source, audios[i].artist, audios[i].title);
    if (audios[i].lyrics_file == NULL) {
        /* We are not interested in comparing this lyrics */
        g_clear_pointer (&lyrics, g_free);
        continue;
    }
    g_assert_nonnull (lyrics);

    file = g_file_new_for_uri (audios[i].lyrics_file);
    g_file_load_contents (file, NULL, &data, &size, NULL, &error);
    g_assert_no_error (error);
    g_clear_pointer (&file, g_object_unref);

    if (g_ascii_strncasecmp (lyrics, data, size - 1) != 0) {
      g_warning ("Lyrics of '%s' from '%s' changed. Check if metrolyrics.com changed",
                  audios[i].title, audios[i].artist);
    }
    g_clear_pointer (&lyrics, g_free);
    g_clear_pointer (&data, g_free);
  }
}

static void
test_resolve_metrolyrics_bad_request (void)
{
  GrlSource *source;
  guint i;

  struct {
    gchar *title;
    gchar *artist;
    gchar *lyrics_file;
  } audios[] = {
   { "GNOME", "grilo framework", NULL },
  };

  source = test_lua_factory_get_source (METROLYRICS_ID, METROLYRICS_OPS);

  for (i = 0; i < G_N_ELEMENTS (audios); i++) {
    gchar *lyrics;

    g_test_expect_message("Grilo", G_LOG_LEVEL_WARNING, "*Can't fetch element*");
    lyrics = get_lyrics (source, audios[i].artist, audios[i].title);
    g_assert_null (lyrics);
  }
}

gint
main (gint argc, gchar **argv)
{
  test_lua_factory_init (&argc, &argv, FALSE);
  test_lua_factory_setup (NULL);

  g_test_add_func ("/lua_factory/sources/metrolyrics", test_resolve_metrolyrics);
  g_test_add_func ("/lua_factory/sources/metrolyrics/bad-request", test_resolve_metrolyrics_bad_request);

  gint result = g_test_run ();

  test_lua_factory_shutdown ();
  test_lua_factory_deinit ();

  return result;
}
