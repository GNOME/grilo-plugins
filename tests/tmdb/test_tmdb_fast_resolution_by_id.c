/*
 * Copyright (C) 2012 Openismus GmbH
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

#include <locale.h>
#include <grilo.h>
#include "test_tmdb_utils.h"
#include <math.h>
#include <float.h>

#define TMDB_PLUGIN_ID "grl-tmdb"

/** Compare the floats.
 * A simple == will fail on values that are effectively the same,
 * due to rounding issues.
 */
static gboolean compare_floats(gfloat a, gfloat b)
{
   return fabs(a - b) < DBL_EPSILON;
}

static void
test_fast_resolution_by_id (void)
{
  GrlKeyID backdrop, posters, tmdb_id;
  GrlRegistry *registry;
  GrlOperationOptions *options = NULL;
  GrlMedia *media = NULL;
  GError *error = NULL;
  GrlSource *source;

  test_setup_tmdb ();

  registry = grl_registry_get_default ();
  backdrop = grl_registry_lookup_metadata_key (registry, "tmdb-backdrop");
  g_assert_cmpint (backdrop, !=, GRL_METADATA_KEY_INVALID);
  posters = grl_registry_lookup_metadata_key (registry, "tmdb-poster");
  g_assert_cmpint (posters, !=, GRL_METADATA_KEY_INVALID);
  tmdb_id = grl_registry_lookup_metadata_key (registry, "tmdb-id");
  g_assert_cmpint (tmdb_id, !=, GRL_METADATA_KEY_INVALID);

  options = grl_operation_options_new (NULL);
  g_assert (options != NULL);
  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_FAST_ONLY);

  media = grl_media_video_new ();
  g_assert (media != NULL);
  grl_data_set_string (GRL_DATA (media), tmdb_id, "10528");

  source = test_get_source();
  g_assert (source);
  grl_source_resolve_sync (source,
                           media,
                           grl_source_supported_keys (source),
                           options,
                           &error);

  /* Fast resolution must not result in an error if the details are missing */
  g_assert (error == NULL);

  /* Check if we have everything we need */
  g_assert (compare_floats (grl_media_get_rating (media), 3.8f));
  g_assert_cmpstr (grl_data_get_string (GRL_DATA (media), GRL_METADATA_KEY_ORIGINAL_TITLE), ==,
                   "Sherlock Holmes");
  /* There's only one poster/backdrop in the search result */
  g_assert_cmpstr (grl_data_get_string (GRL_DATA (media), backdrop), ==,
                   "http://cf2.imgobject.com/t/p/original/uM414ugc1B910bTvGEIzsucfMMC.jpg");

  g_assert_cmpstr (grl_data_get_string (GRL_DATA (media), posters), ==,
                   "http://cf2.imgobject.com/t/p/original/22ngurXbLqab7Sko6aTSdwOCe5W.jpg");

  g_assert (grl_media_get_publication_date (media) == NULL);

  g_clear_object (&media);
  g_clear_object (&options);

  test_shutdown_tmdb ();
}


int
main(int argc, char **argv)
{
  gint result;

  setlocale (LC_ALL, "");

  g_setenv ("GRL_PLUGIN_PATH", GRILO_PLUGINS_TESTS_TMDB_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", TMDB_PLUGIN_ID, TRUE);

  /* We must set this before calling grl_init.
   * See https://bugzilla.gnome.org/show_bug.cgi?id=685967#c17
   */
  g_setenv ("GRL_NET_MOCKED", GRILO_PLUGINS_TESTS_TMDB_DATA_PATH "fast-by-id.ini", TRUE);

  grl_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/tmdb/fast-resolution-by-id", test_fast_resolution_by_id);

  result = g_test_run ();

  grl_deinit ();

  return result;
}
