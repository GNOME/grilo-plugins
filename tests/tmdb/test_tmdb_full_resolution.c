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

static gboolean
compare_floats(gfloat a, gfloat b)
{
  return fabs(a - b) < DBL_EPSILON;
}

static const gchar*
get_region_certificate (GrlMedia *media, const gchar *region)
{
  guint count = grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_REGION);
  guint i;
  for (i = 0; i < count; ++i) {
    const GDateTime* publication_date = NULL;
    const gchar *certificate = NULL;
    const gchar *this_region =
    grl_media_get_region_data_nth (media, i,
      &publication_date, &certificate);

    /* printf("idnex=%d, region=%s, cert=%s\n", i, this_region, certificate); */

    if(g_strcmp0 (region, this_region) == 0)
      return certificate;
  }

  return NULL;
}

static void
test_region_certificate (GrlMedia *media, const gchar *region, const gchar *expected_certificate)
{
  const gchar *certificate = get_region_certificate (media, region);
  g_assert_cmpstr(certificate, ==, expected_certificate);
}

#define DESCRIPTION \
"Eccentric consulting detective Sherlock Holmes and Doctor John Watson battle to bring down a new nemesis and unravel a deadly plot that could destroy England."

static void
test_full_resolution (void)
{
  GError *error = NULL;
  GrlRegistry *registry;
  GrlKeyID backdrop, posters, imdb_id;
  GrlOperationOptions *options = NULL;
  GrlMedia *media = NULL;
  GrlSource *source;
  guint count;

  test_setup_tmdb ();

  registry = grl_registry_get_default ();
  backdrop = grl_registry_lookup_metadata_key (registry, "tmdb-backdrop");
  g_assert_cmpint (backdrop, !=, GRL_METADATA_KEY_INVALID);
  posters = grl_registry_lookup_metadata_key (registry, "tmdb-poster");
  g_assert_cmpint (posters, !=, GRL_METADATA_KEY_INVALID);
  imdb_id = grl_registry_lookup_metadata_key (registry, "tmdb-imdb-id");
  g_assert_cmpint (imdb_id, !=, GRL_METADATA_KEY_INVALID);

  media = grl_media_video_new ();
  g_assert (media != NULL);
  grl_media_set_title (media, "Sherlock Holmes");

  source = test_get_source();
  g_assert (source);
  options = grl_operation_options_new (NULL);
  g_assert (options != NULL);
  grl_source_resolve_sync (source,
                           media,
                           grl_source_supported_keys (source),
                           options,
                           &error);
  g_assert_no_error (error);

  /* Check if we got everything we need for the fast resolution */
  g_assert (compare_floats (grl_media_get_rating (media), 3.8f));
  g_assert_cmpstr (grl_data_get_string (GRL_DATA (media), GRL_METADATA_KEY_ORIGINAL_TITLE), ==,
                   "Sherlock Holmes");
  /* There's only one poster/backdrop in the search result */
  g_assert_cmpstr (grl_data_get_string (GRL_DATA (media), backdrop), ==,
                   "http://cf2.imgobject.com/t/p/original/uM414ugc1B910bTvGEIzsucfMMC.jpg");

  g_assert_cmpstr (grl_data_get_string (GRL_DATA (media), posters), ==,
                   "http://cf2.imgobject.com/t/p/original/22ngurXbLqab7Sko6aTSdwOCe5W.jpg");
  g_assert (grl_media_get_publication_date (media) != NULL);

  /* And now the slow properties */
  g_assert_cmpstr (grl_media_get_site (media), ==,
                   "http://sherlock-holmes-movie.warnerbros.com/");
  g_assert_cmpint (grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_GENRE), ==, 7);
  g_assert_cmpint (grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_STUDIO), ==, 3);

  g_assert_cmpstr (grl_media_get_description (media), ==, DESCRIPTION);

  g_assert_cmpstr (grl_data_get_string (GRL_DATA (media), imdb_id), ==, "tt0988045");
  g_assert_cmpint (grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_KEYWORD), ==, 12);

  g_assert_cmpint (grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_PERFORMER), ==, 54);

  g_assert_cmpint (grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_PRODUCER), ==, 10);

  g_assert_cmpint (grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_DIRECTOR), ==, 2);
  g_assert_cmpstr (grl_data_get_string (GRL_DATA (media), GRL_METADATA_KEY_DIRECTOR), ==, "Guy Ritchie");
  count = grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_REGION);
  g_assert_cmpint (count, ==, 8);
  test_region_certificate (media, "GB", "12A");
  test_region_certificate (media, "FR", ""); /* TODO: Should this be here? */
  test_region_certificate (media, "NL", "12");
  test_region_certificate (media, "BG", "C");
  test_region_certificate (media, "HU", "16");
  test_region_certificate (media, "DE", "12");
  test_region_certificate (media, "DK", "15");
  test_region_certificate (media, "US", "PG-13");

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
  g_setenv ("GRL_NET_MOCKED", GRILO_PLUGINS_TESTS_TMDB_DATA_PATH "sherlock.ini", TRUE);

  grl_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/tmdb/full-resolution", test_full_resolution);

  result = g_test_run ();

  grl_deinit ();

  return result;
}
