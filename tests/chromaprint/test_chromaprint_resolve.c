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

#include "test_chromaprint_utils.h"
#include <locale.h>
#include <grilo.h>

static void
get_chromaprint_fingerprint (GrlSource   *source,
                             const gchar *url,
                             gint        *duration,
                             gchar      **fingerprint)
{
  GrlMedia *audio;
  GrlOperationOptions *options;
  GList *keys;
  GrlRegistry *registry;
  GrlKeyID key_fingerprint;

  registry = grl_registry_get_default ();

  key_fingerprint = grl_registry_lookup_metadata_key (registry, "chromaprint");
  g_assert_cmpint (key_fingerprint, !=, GRL_METADATA_KEY_INVALID);

  audio = grl_media_audio_new ();
  grl_media_set_url (audio, url);

  keys = grl_metadata_key_list_new (key_fingerprint,
                                    GRL_METADATA_KEY_DURATION,
                                    GRL_METADATA_KEY_INVALID);
  options = grl_operation_options_new (NULL);
  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_NORMAL);

  grl_source_resolve_sync (source,
                           GRL_MEDIA (audio),
                           keys,
                           options,
                           NULL);

  if (!grl_data_has_key (GRL_DATA (audio), GRL_METADATA_KEY_DURATION))
    g_error ("Necessary audio decoders not installed, verify your installation");

  *duration = grl_media_get_duration (GRL_MEDIA (audio));
  *fingerprint = g_strdup (grl_data_get_string (GRL_DATA (audio),
                                                key_fingerprint));
  g_list_free (keys);
  g_object_unref (options);
  g_object_unref (audio);
}

static void
test_fingerprint (void)
{
  GrlSource *source;
  guint i;

  struct {
    gchar *url;
    gint   duration;
    gchar *fingerprint;
  } audios[] = {
    { CHROMAPRINT_PLUGIN_TEST_DATA_PATH "sample.flac", 5,
      "AQAAF1miRNLCKAkafjhUscij5DjFpOgz44N39Mmx-xHyHOfmCNOPZjJOLceP5tA6KRf-473xJMUPRoIDAkqCBEEGCCYAUpYQAQgxSAE" },
    { CHROMAPRINT_PLUGIN_TEST_DATA_PATH "sample.ogg", 4,
      "AQAADlQ0JaGihBgpvWicED98hOTxhNCRb7i24RYZNMmEakpY_AGAABTMS0eIAw" }
  };

  source = test_get_source ();
  g_assert (source);

  for (i = 0; i < G_N_ELEMENTS (audios); i++) {
    gchar *fingerprint = NULL;
    gint duration;

    get_chromaprint_fingerprint (source, audios[i].url, &duration, &fingerprint);
    g_assert_cmpint (audios[i].duration, ==, duration);
    g_assert_cmpstr (audios[i].fingerprint, ==, fingerprint);
    g_clear_pointer (&fingerprint, g_free);
  }
}

gint
main (gint argc, gchar **argv)
{
  setlocale (LC_ALL, "");

  g_setenv ("GRL_PLUGIN_PATH", CHROMAPRINT_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", CHROMAPRINT_ID, TRUE);

  grl_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

#if !GLIB_CHECK_VERSION(2,32,0)
  g_thread_init (NULL);
#endif

  test_setup_chromaprint ();

  g_test_add_func ("/chromaprint/resolve/fingerprint", test_fingerprint);

  gint result = g_test_run ();

  test_shutdown_chromaprint ();

  grl_deinit ();

  return result;
}
