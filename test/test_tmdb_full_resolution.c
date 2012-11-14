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

#include <grilo.h>
#include "test_tmdb_utils.h"
#include <math.h>
#include <float.h>

#define TMDB_PLUGIN_ID "grl-tmdb"

/** Compare the floats.
 * A simple == will fail on values that are effectively the same,
 * due to rounding issues.
 */
static gboolean compare_floats (gfloat a, gfloat b)
{
  return fabs(a - b) < DBL_EPSILON;
}

const char *iso_date (const GDateTime *date, char **strbuf)
{
  if (*strbuf)
    g_free (*strbuf);

  return (*strbuf = g_date_time_format ((GDateTime *) date, "%F"));
}

#define DESCRIPTION \
"In a dynamic new portrayal of Arthur Conan Doyle’s most famous characters, “Sherlock Holmes” sends Holmes and his stalwart partner Watson on their latest challenge. Revealing fighting skills as lethal as his legendary intellect, Holmes will battle as never before to bring down a new nemesis and unravel a deadly plot that could destroy England."

static void
test (void)
{
  GError *error = NULL;
  GrlRegistry *registry;
  GrlKeyID backdrop, posters, imdb_id;
  GrlOperationOptions *options = NULL;
  GrlMedia *media = NULL;
  const GDateTime *date;
  const char *cert;
  char *tmp = NULL;

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

  GrlSource *source = test_get_source();
  g_assert (source);
  options = grl_operation_options_new (NULL);
  g_assert (options != NULL);
  grl_source_resolve_sync (source,
                           media,
                           grl_source_supported_keys (source),
                           options,
                           &error);
  g_assert (error == NULL);

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
  g_assert_cmpint (grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_GENRE), ==, 6);
  g_assert_cmpint (grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_STUDIO), ==, 3);

  g_assert_cmpstr (grl_media_get_description (media), ==, DESCRIPTION);

  g_assert_cmpstr (grl_data_get_string (GRL_DATA (media), imdb_id), ==, "tt0988045");
  g_assert_cmpint (grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_KEYWORD), ==, 15);

  g_assert_cmpint (grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_PERFORMER), ==, 10);

  g_assert_cmpint (grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_PRODUCER), ==, 9);

  g_assert_cmpint (grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_DIRECTOR), ==, 1);
  g_assert_cmpstr (grl_data_get_string (GRL_DATA (media), GRL_METADATA_KEY_DIRECTOR), ==, "Guy Ritchie");

  g_assert_cmpint (grl_data_length (GRL_DATA (media), GRL_METADATA_KEY_REGION), ==, 8);

  g_assert_cmpstr (grl_media_get_region_data_nth (media, 0, &date, &cert), ==, "GB");
  g_assert_cmpstr (iso_date (date, &tmp), ==, "2009-12-26");
  g_assert_cmpstr (cert, ==, "12A");
  g_assert_cmpstr (grl_media_get_region_data_nth (media, 1, &date, &cert), ==, "NL");
  g_assert_cmpstr (iso_date (date, &tmp), ==, "2010-01-07");
  g_assert_cmpstr (cert, ==, "12");
  g_assert_cmpstr (grl_media_get_region_data_nth (media, 2, &date, &cert), ==, "BG");
  g_assert_cmpstr (iso_date (date, &tmp), ==, "2010-01-01");
  g_assert_cmpstr (cert, ==, "C");
  g_assert_cmpstr (grl_media_get_region_data_nth (media, 3, &date, &cert), ==, "HU");
  g_assert_cmpstr (iso_date (date, &tmp), ==, "2010-01-07");
  g_assert_cmpstr (cert, ==, "16");
  g_assert_cmpstr (grl_media_get_region_data_nth (media, 4, &date, &cert), ==, "DE");
  g_assert_cmpstr (iso_date (date, &tmp), ==, "2010-01-28");
  g_assert_cmpstr (cert, ==, "12");
  g_assert_cmpstr (grl_media_get_region_data_nth (media, 5, &date, &cert), ==, "FR");
  g_assert_cmpstr (iso_date (date, &tmp), ==, "2010-02-03");
  g_assert_cmpstr (cert, ==, "");
  g_assert_cmpstr (grl_media_get_region_data_nth (media, 6, &date, &cert), ==, "DK");
  g_assert_cmpstr (iso_date (date, &tmp), ==, "2009-12-25");
  g_assert_cmpstr (cert, ==, "15");
  g_assert_cmpstr (grl_media_get_region_data_nth (media, 7, &date, &cert), ==, "US");
  g_assert_cmpstr (iso_date (date, &tmp), ==, "2009-12-25");
  g_assert_cmpstr (cert, ==, "PG-13");

  g_object_unref (media);
  media = NULL;

  g_object_unref (options);
  options = NULL;

  test_shutdown_tmdb ();
}


int
main(int argc, char **argv)
{
  g_setenv ("GRL_PLUGIN_PATH", GRILO_PLUGINS_TESTS_TMDB_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", TMDB_PLUGIN_ID, TRUE);

  /* We must set this before calling grl_init.
   * See https://bugzilla.gnome.org/show_bug.cgi?id=685967#c17
   */
  g_setenv ("GRL_NET_MOCKED", GRILO_PLUGINS_TESTS_TMDB_DATA_PATH "sherlock.ini", TRUE);

  grl_init (&argc, &argv);
#if !GLIB_CHECK_VERSION(2,32,0)
  g_thread_init (NULL);
#endif

  test ();
}
