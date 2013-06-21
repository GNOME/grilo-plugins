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

static void
test (void)
{
  GrlMedia *media = NULL;
  GrlOperationOptions *options = NULL;
  GError *error = NULL;

  test_setup_tmdb ();

  /* Doesn't matter. We just need to get it to resolve */
  media = grl_media_video_new ();
  g_assert (media != NULL);
  grl_media_set_title (media, "Non-Empty");

  GrlSource *source = test_get_source();
  g_assert (source);
  options = grl_operation_options_new (NULL);
  g_assert (options != NULL);
  grl_source_resolve_sync (source,
                           media,
                           grl_source_supported_keys (source),
                           options,
                           &error);

  /* Check that the plugin didn't even try to resolve data, otherwise the mock
   * file would have resulted in an error */
  g_assert (error != NULL);

  g_object_unref (media);
  media = NULL;

  g_object_unref (options);
  options = NULL;

  g_error_free (error);
  error = NULL;

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
  g_setenv ("GRL_NET_MOCKED", GRILO_PLUGINS_TESTS_TMDB_DATA_PATH "empty-data.ini", TRUE);

  grl_init (&argc, &argv);
#if !GLIB_CHECK_VERSION(2,32,0)
  g_thread_init (NULL);
#endif

  test ();
}
