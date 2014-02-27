/*
 * Copyright (C) 2014 Red Hat Inc.
 *
 * Author: Bastien Nocera <hadess@hadess.net>
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

#include <grilo.h>

#define LOCAL_METADATA_ID "grl-local-metadata"

static void
test_setup (void)
{
  GError *error = NULL;
  GrlRegistry *registry;

  registry = grl_registry_get_default ();
  grl_registry_load_all_plugins (registry, &error);
  g_assert_no_error (error);
}

static char *
get_show_for_title (GrlSource  *source,
		    const char *title,
		    int        *season,
		    int        *episode)
{
  GrlMedia *media;
  GrlOperationOptions *options;
  GList *keys;
  char *show;

  media = grl_media_video_new ();
  grl_media_set_title (media, title);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_SHOW,
				    GRL_METADATA_KEY_SEASON,
				    GRL_METADATA_KEY_EPISODE,
				    NULL);
  options = grl_operation_options_new (NULL);
  grl_operation_options_set_flags (options, GRL_RESOLVE_FULL);

  grl_source_resolve_sync (source,
			   media,
			   keys,
			   options,
			   NULL);

  g_list_free (keys);
  g_object_unref (options);

  *season = grl_data_get_int (GRL_DATA (media), GRL_METADATA_KEY_SEASON);
  *episode = grl_data_get_int (GRL_DATA (media), GRL_METADATA_KEY_EPISODE);
  show = g_strdup (grl_data_get_string (GRL_DATA (media), GRL_METADATA_KEY_SHOW));

  g_object_unref (media);

  return show;
}

static void
test_episodes (void)
{
  GrlRegistry *registry;
  GrlSource *source;
  guint i;

  struct {
    char *title;
    char *show;
    int season;
    int episode;
  } episode_tests[] = {
    { "The.Slap.S01E01.Hector.WS.PDTV.XviD-BWB.avi", "The Slap", 1, 1 },
    { "metalocalypse.s02e01.dvdrip.xvid-ffndvd.avi", "metalocalypse", 2, 1 },
    { "Boardwalk.Empire.S04E01.HDTV.x264-2HD.mp4", "Boardwalk Empire", 4, 1 },
  };


  registry = grl_registry_get_default ();
  source = grl_registry_lookup_source (registry, "grl-local-metadata");
  g_assert (source);

  for (i = 0; i < G_N_ELEMENTS(episode_tests); i++) {
    char *show;
    int season, episode;

    show = get_show_for_title (source, episode_tests[i].title, &season, &episode);
    g_assert_cmpstr (episode_tests[i].show, ==, show);
    if (show != NULL) {
      g_assert_cmpint (episode_tests[i].season, ==, season);
      g_assert_cmpint (episode_tests[i].episode, ==, episode);
    }
    g_free (show);
  }
}

int
main(int argc, char **argv)
{
  g_setenv ("GRL_PLUGIN_PATH", LOCAL_METADATA_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", LOCAL_METADATA_ID, TRUE);

  grl_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

#if !GLIB_CHECK_VERSION(2,32,0)
  g_thread_init (NULL);
#endif

  test_setup ();

  g_test_add_func ("/local-metadata/resolve/episodes", test_episodes);

  gint result = g_test_run ();

  grl_deinit ();

  return result;
}
