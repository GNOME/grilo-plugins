/*
 * Copyright (C) 2011 Igalia S.L.
 *
 * Contact: Guillaume Emont <gemont@igalia.com>
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

/*
 * A small program to test the local-metadata plugin.
 */

#define TEST_DATA "./test_data"
#define TEST_IMAGE1 TEST_DATA"/image1.png"
#define TEST_IMAGE2 TEST_DATA"/image2.png"
#define TEST_THUMBNAIL TEST_DATA"/thumbnail.png"
#define HOME "HOME"
#define THUMBNAIL_DIR ".thumbnails/normal"
#define LOCAL_SOURCE_ID   "grl-local-metadata"

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <string.h>
#include <unistd.h>

#include <grilo.h>

/*
 * It's a test program, global variables are allowed!
 */

GrlSource *local_source = NULL;
GMainLoop *loop = NULL;

static gchar *
image_thumbnail_path (const gchar *path)
{
  gchar *file_uri, *checksum, *thumbnail_path, *thumbnail_filename;
  const gchar *home;
  GFile *file;

  file = g_file_new_for_path (path);
  if (!file)
    return NULL;

  file_uri = g_file_get_uri (file);
  g_object_unref (file);
  if (!file_uri)
    return NULL;

  checksum = g_compute_checksum_for_string (G_CHECKSUM_MD5, file_uri,
                                            strlen (file_uri));
  g_free (file_uri);
  if (!checksum)
    return NULL;

  home = g_getenv (HOME);
  if (!home) {
    g_free (checksum);
    return NULL;
  }

  thumbnail_filename = g_strconcat (checksum, ".png", NULL);
  g_free (checksum);
  if (!thumbnail_filename)
    return NULL;

  thumbnail_path = g_build_filename (home, THUMBNAIL_DIR, thumbnail_filename, NULL);
  g_free (thumbnail_filename);

  return thumbnail_path;
}

static gboolean
install_thumbnail (const gchar *path)
{
  gchar *thumbnail_path;
  gboolean ret = FALSE;

  thumbnail_path = image_thumbnail_path (path);
  if (thumbnail_path) {
    GFile *source, *destination;

    source = g_file_new_for_path (TEST_THUMBNAIL);
    destination = g_file_new_for_path (thumbnail_path);
    g_free (thumbnail_path);

    ret = g_file_copy (source, destination, G_FILE_COPY_NONE,
                       NULL, NULL, NULL, NULL);

    g_object_unref (source);
    g_object_unref (destination);
  }


  return ret;
}

static GrlMedia *
make_grl_image (const gchar *path)
{
  GFile *file;
  GrlMedia *media;
  gchar *full_path, *uri;


  file = g_file_new_for_path (path);

  media = grl_media_image_new ();

  full_path = g_file_get_path (file);
  grl_media_set_id (media, full_path);
  g_free (full_path);

  uri = g_file_get_uri (file);
  grl_media_set_url (media, uri);
  g_free (uri);

  return media;
}

/*
 * return TRUE if it worked, FALSE otherwise, in which case it would have
 * cleaned its mess
 */
static gboolean
setup (void)
{
  gboolean ret;
  GrlRegistry *registry;
  ret = install_thumbnail (TEST_IMAGE1);
  if (!ret)
    goto finish;

  registry = grl_registry_get_default ();

  GError *error = NULL;
  grl_registry_load_all_plugins (registry, &error);
  g_assert_no_error (error);

  local_source = grl_registry_lookup_source (registry, LOCAL_SOURCE_ID);

finish:
  return ret;
}

static void
fs_teardown (void)
{
  gchar *thumbnail_path;

  thumbnail_path = image_thumbnail_path (TEST_IMAGE1);
  if (thumbnail_path) {
    g_unlink (thumbnail_path);
    g_free (thumbnail_path);
  }
}

static void
teardown (void)
{
  fs_teardown ();
}

static gboolean
test_image_thumbnail (void)
{
  GrlMedia *media = NULL;
  GrlOperationOptions *options;
  GError *error = NULL;
  GList *keys = NULL;
  gboolean ret = TRUE;
  gchar *expected_thumbnail_path = NULL;
  gchar *expected_thumbnail_uri = NULL;
  const gchar *thumbnail_uri;

  media = make_grl_image (TEST_IMAGE1);
  expected_thumbnail_path = image_thumbnail_path (grl_media_get_id (media));
  expected_thumbnail_uri = g_filename_to_uri (expected_thumbnail_path,
                                              NULL, NULL);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_THUMBNAIL, NULL);

  options = grl_operation_options_new (NULL);
  grl_operation_options_set_flags (options, GRL_RESOLVE_NORMAL);

  grl_source_resolve_sync (local_source, media, keys, options, &error);
  if (error) {
    g_clear_error (&error);
    ret = FALSE;
    goto cleanup;
  }


  thumbnail_uri = grl_media_get_thumbnail (media);
  ret = 0 == g_strcmp0 (thumbnail_uri, expected_thumbnail_uri);
  if (!ret) {
    g_print ("expected \"%s\", got \"%s\"\n", expected_thumbnail_uri,
             thumbnail_uri);
  }

cleanup:
  if (media)
    g_object_unref (media);
  if (error)
    g_error_free (error);
  if (keys)
    g_list_free (keys);
  if (expected_thumbnail_path)
    g_free (expected_thumbnail_path);
  if (expected_thumbnail_uri)
    g_free (expected_thumbnail_uri);
  g_object_unref (options);
  return ret;
}

static gboolean
test_image_no_thumbnail (void)
{
  GrlMedia *media = NULL;
  GrlOperationOptions *options;
  GError *error = NULL;
  GList *keys = NULL;
  gboolean ret = TRUE;
  const gchar *thumbnail_uri;

  media = make_grl_image (TEST_IMAGE2);

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_THUMBNAIL, NULL);

  options = grl_operation_options_new (NULL);
  grl_operation_options_set_flags (options, GRL_RESOLVE_NORMAL);
  media = grl_source_resolve_sync (local_source, media, keys, options, &error);
  if (error) {
    g_clear_error (&error);
    ret = FALSE;
    goto cleanup;
  }

  thumbnail_uri = grl_media_get_thumbnail (media);
  /* Note: this test will fail if you accidentely created a thumbnail image for
   * TEST_IMAGE2 */
  ret = NULL == thumbnail_uri;

cleanup:
  if (media)
    g_object_unref (media);
  if (keys)
    g_list_free (keys);
  g_object_unref (options);
  return ret;
}

static void
run_test(gboolean (*test_func)(void), const gchar *test_name)
{
  gboolean ret;

  access (test_name, F_OK);
  ret = test_func ();
  access (test_name, X_OK);
  g_print ("%s: %s\n", test_name, ret?"PASS":"FAIL");
}

static gboolean
do_test (gpointer data)
{
  if (!setup ()) {
    g_printerr ("Error in setup()!\n");
    goto finish;
  }

  run_test (test_image_thumbnail, "test_image_thumbnail");
  run_test (test_image_no_thumbnail, "test_image_no_thumbnail");

  teardown ();

finish:
  g_main_loop_quit (loop);
  return FALSE;
}

int
main(int argc, char **argv)
{
  g_setenv ("GRL_PLUGIN_PATH", GRILO_PLUGINS_TESTS_LOCAL_METADATA_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", LOCAL_SOURCE_ID, TRUE);

  grl_init (&argc, &argv);
#if !GLIB_CHECK_VERSION(2,32,0)
  g_thread_init (NULL);
#endif

  loop = g_main_loop_new (g_main_context_default (), TRUE);

  g_idle_add (do_test, NULL);

  g_main_loop_run (loop);


  teardown ();
}
