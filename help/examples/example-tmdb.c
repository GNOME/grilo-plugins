/*
 * Copyright (C) 2012 Canonical Ltd.
 *
 * Author: Jens Georg <jensg@openismus.com>
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
#include <stdio.h>

/**
 * This TMDB key is just for testing.
 * For real-world use, please request your own key from
 * http://api.themoviedb.org
 */
#define TMDB_KEY "719b9b296835b04cd919c4bf5220828a"

#define TMDB_PLUGIN_ID "grl-tmdb"

GMainLoop *loop = NULL;
GrlKeyID director_key = 0;

static void
resolve_cb (GrlSource *src, guint operation_id, GrlMedia *media, gpointer user_data, const GError *error)
{
  const char *title, *studio;

  g_assert_no_error (error);
  g_assert (media);

  title = grl_media_get_title (media);
  studio = grl_media_get_studio (media);
  printf ("Media: Title='%s', Studio='%s'\n",
    title, studio);

  if (director_key != 0) {
    const gchar *director =
      grl_data_get_string (GRL_DATA (media), director_key);
    printf ("  Director=%s\n", director);
  }

  g_main_loop_quit (loop);
}

int main (int argc, char *argv[])
{
  GrlRegistry *reg;
  GrlConfig *config;
  GError *error = NULL;
  GrlSource *src;
  gboolean plugin_activated;
  GrlCaps *caps;
  GrlOperationOptions *options;
  GrlMedia *media;
  const GList *keys;
  const GList* l;

  grl_init (&argc, &argv);

  /*
   * Set the TMDB API key:
   * You must use your own TMDB API key in your own application.
   */
  reg = grl_registry_get_default ();
  config = grl_config_new (TMDB_PLUGIN_ID, NULL);
  grl_config_set_api_key (config, TMDB_KEY);
  grl_registry_add_config (reg, config, NULL);
  grl_registry_load_all_plugins (reg, FALSE, NULL);

  /*
   * Get the plugin:
   */
  error = NULL;
  plugin_activated =
    grl_registry_activate_plugin_by_id (reg, TMDB_PLUGIN_ID, &error);
  g_assert (plugin_activated);
  g_assert_no_error (error);

  /*
   * Get the Grilo source:
   */
  src = grl_registry_lookup_source (reg, TMDB_PLUGIN_ID);

  /*
   * Check that it has the expected capability:
   */
  g_assert (grl_source_supported_operations (src) & GRL_OP_RESOLVE);
  caps = grl_source_get_caps (src, GRL_OP_RESOLVE);
  g_assert (caps);

  options = grl_operation_options_new (caps);

  /*
   * A media item that we will give to the TMDB plugin,
   * to discover its details.
   */
  media = grl_media_video_new ();
  grl_media_set_title (media, "Sherlock Holmes");

  /*
   * Discover what keys are provided by the source:
   */
  keys = grl_source_supported_keys (src);
  for (l = keys; l != NULL; l = l->next) {
    const gchar *name;
    GrlKeyID id = GPOINTER_TO_INT (l->data);

    g_assert (id);

    name = grl_metadata_key_get_name (id);
    printf ("Supported key: %s\n", name);

    /*
     * Remember this for later use:
     * You may instead use grl_registry_lookup_metadata_key_name().
     */
    if (g_strcmp0 (name, "tmdb-director") == 0) {
      director_key = id;
    }
  }

  /*
   * Ask the TMDB plugin for the media item's details,
   * from the TMDB online service:
   */
  grl_source_resolve (src, media,
    keys, options,
    resolve_cb, NULL);

  /*
   * Start the main loop so our callback can be called:
   */
  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  /*
   * Release objects:
   */
  g_object_unref (media);
  g_object_unref (config);
  g_object_unref (options);

  /*
   * Deinitialize Grilo:
   */
  grl_deinit ();
}


