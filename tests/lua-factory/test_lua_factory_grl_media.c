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

#include <locale.h>
#include <grilo.h>
#include <string.h>
#include <json-glib/json-glib.h>

#define GRESOURCE_PREFIX "resource:///org/gnome/grilo/plugins/test/lua-factory/data/"

#define TEST_GRL_MEDIA_URL "http://grl.media.test/lua-factory/"

#define LUA_FACTORY_ID "grl-lua-factory"
#define FAKE_SOURCE_ID "test-source-grl-media"


static void
test_lua_factory_setup (GrlConfig *config)
{
  GrlRegistry *registry;
  GError *error = NULL;

  registry = grl_registry_get_default ();

  if (config != NULL) {
    grl_registry_add_config (registry, config, &error);
    g_assert_no_error (error);
  }

  grl_registry_load_all_plugins (registry, FALSE, NULL);
  grl_registry_activate_plugin_by_id (registry, LUA_FACTORY_ID, &error);
  g_assert_no_error (error);
}

static GrlSource*
test_lua_factory_get_source (gchar *source_id,
                             GrlSupportedOps source_ops)
{
  GrlRegistry *registry = grl_registry_get_default ();
  GrlSource *source = grl_registry_lookup_source (registry, source_id);
  g_assert_nonnull (source);
  g_assert (grl_source_supported_operations (source) & source_ops);
  return source;
}

static void
test_lua_factory_shutdown (void)
{
  GrlRegistry *registry;
  GError *error = NULL;

  registry = grl_registry_get_default ();
  grl_registry_unload_plugin (registry, LUA_FACTORY_ID, &error);
  g_assert_no_error (error);
}

static void
check_metadata (GrlMedia *media,
                GrlKeyID key_id,
                JsonReader *reader,
                gint64 index)
{
  GrlRegistry *registry = grl_registry_get_default ();
  GType type = grl_registry_lookup_metadata_key_type (registry, key_id);

  switch (type) {
  case G_TYPE_INT:
  case G_TYPE_INT64: {
    gint64 from_json = json_reader_get_int_value (reader);
    gint64 from_media = (type == G_TYPE_INT) ?
        grl_data_get_int (GRL_DATA (media), key_id) :
        grl_data_get_int64 (GRL_DATA (media), key_id);
    g_assert_cmpint (from_json, ==, from_media);
  }
  break;

  case G_TYPE_FLOAT: {
    gfloat from_json = (gfloat) json_reader_get_double_value (reader);
    gfloat from_media = grl_data_get_float (GRL_DATA (media), key_id);
    g_assert_cmpfloat (from_json, ==, from_media);
  }
  break;

  case G_TYPE_STRING:
    if (json_reader_is_array (reader)) {
      GList *list = grl_data_get_single_values_for_key_string (GRL_DATA (media),
                                                               key_id);
      guint num_elements = json_reader_count_elements (reader);
      guint i;
      for (i = 0; i < num_elements; i++) {
        GList *it = NULL;
        json_reader_read_element (reader, i);
        const gchar *from_json = json_reader_get_string_value (reader);
        json_reader_end_element (reader);
        for (it = list; it != NULL; it = it->next) {
          const gchar *from_media = it->data;
          if (g_strcmp0(from_json, from_media) == 0)
            break;
        }
        list = g_list_delete_link (list, it);
      }
      g_assert_null (list);

    } else {
      GrlRelatedKeys *relkeys;
      const gchar *from_media;
      const gchar *from_json;

      from_json = json_reader_get_string_value (reader);
      relkeys = grl_data_get_related_keys (GRL_DATA (media), key_id, index);
      g_assert (relkeys);
      from_media = grl_related_keys_get_string (relkeys, key_id);
      g_assert_cmpstr (from_json, ==, from_media);
    }
  break;

  case G_TYPE_BOOLEAN: {
    gboolean from_json = json_reader_get_boolean_value (reader);
    gboolean from_media = grl_data_get_boolean (GRL_DATA (media), key_id);
    g_assert_cmpint (from_json, ==, from_media);
  }
  break;

  default:
    /* Non-fundamental types don't reduce to ints, so can't be
     * in the switch statement */
    if (type == G_TYPE_DATE_TIME) {
      const gchar *date_str = json_reader_get_string_value (reader);
      GDateTime *from_json = grl_date_time_from_iso8601 (date_str);
      GDateTime *from_media = grl_data_get_boxed (GRL_DATA (media), key_id);

      /* Try a number of seconds since Epoch */
      if (!from_json) {
        gint64 date_int = g_ascii_strtoll (date_str, NULL, 0);
        if (date_int)
          from_json = g_date_time_new_from_unix_utc (date_int);
      }

      g_assert_true (g_date_time_compare (from_json, from_media) == 0);
      g_date_time_unref (from_json);

    } else if (type == G_TYPE_BYTE_ARRAY) {
       gsize size;
       const gpointer from_media = (const gpointer) grl_data_get_binary (GRL_DATA (media), key_id, &size);
       const gpointer from_json = (const gpointer) json_reader_get_string_value (reader);
       g_assert_true (memcmp(from_media, from_json, size) == 0);

    } else {
      g_assert_not_reached ();
    }
  }
}

static void
test_metadata_from_media (GrlMedia *media,
                          const gchar *input)
{
  JsonParser *parser = NULL;
  JsonReader *reader = NULL;
  GError *error = NULL;

  GrlRegistry *registry = grl_registry_get_default ();
  parser = json_parser_new ();
  json_parser_load_from_data (parser, input, -1, &error);
  g_assert_no_error (error);

  /* FIXME: use https://bugzilla.gnome.org/show_bug.cgi?id=755448 */
  reader = json_reader_new (json_parser_get_root (parser));
  if (json_reader_is_object (reader)) {
    guint i, len;
    len = json_reader_count_members (reader);

    for (i = 0; i < len; i++) {
      GrlKeyID key_id = GRL_METADATA_KEY_INVALID;
      const gchar *key_name;

      json_reader_read_element (reader, i);
      key_name = json_reader_get_member_name (reader);
      if (g_strcmp0 (key_name, "related-keys") == 0) {
        gint rel_key_index;
        gint rel_keys_nr = json_reader_count_elements (reader);

        for (rel_key_index = 0; rel_key_index < rel_keys_nr; rel_key_index++) {
          const gchar *rel_key_name;
          guint key_index;
          guint len_rel_key;

          json_reader_read_element (reader, rel_key_index);
          len_rel_key = json_reader_count_members (reader);
          for (key_index = 0; key_index < len_rel_key; key_index++) {
            json_reader_read_element (reader, key_index);
            rel_key_name = json_reader_get_member_name (reader);
            key_id = grl_registry_lookup_metadata_key (registry, rel_key_name);
            g_assert (key_id != GRL_METADATA_KEY_INVALID);
            check_metadata (media, key_id, reader, rel_key_index);
            json_reader_end_element (reader);
          }
          json_reader_end_element (reader);
        }
      } else if (g_strcmp0 (key_name, "type") != 0) {
        key_id = grl_registry_lookup_metadata_key (registry, key_name);
        g_assert (key_id != GRL_METADATA_KEY_INVALID);
        check_metadata (media, key_id, reader, 0);
      }
      json_reader_end_element (reader);
    }
  }

  g_object_unref (reader);
  g_object_unref (parser);
}

static GrlMedia *
get_media (GrlSource *source,
           GrlMedia *media)
{
  GrlOperationOptions *options;
  GList *keys;

  /* Keys can't be NULL, by default we ask for title */
  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_TITLE, NULL);

  options = grl_operation_options_new (NULL);
  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_NORMAL);

  grl_source_resolve_sync (source, media, keys, options, NULL);

  g_list_free (keys);
  g_object_unref (options);
  return media;
}

static void
resolve_fake_src (const gchar *input,
                  const gchar *url)
{
  GrlSource *source = NULL;
  GrlMedia *media = NULL;

  source = test_lua_factory_get_source (FAKE_SOURCE_ID, GRL_OP_RESOLVE);
  g_assert_nonnull (source);

  media = grl_media_new ();
  grl_media_set_url (media, url);
  media = get_media (source, media);
  g_assert_nonnull (media);
  test_metadata_from_media (media, input);
  g_object_unref (media);
}

static void
test_build_media (void)
{
  gint i;

  struct {
    gchar *uri;
    gchar *url;
  } media_tests[] = {
    /* This is a basic test to check all metadata keys provided by core in the
     * simplest way possible. Boolean, strings, numbers, arrays. */
    { GRESOURCE_PREFIX "grl-media-test-all-metadata.json",
      TEST_GRL_MEDIA_URL "all-metadata.json" }
  };

  for (i = 0; i < G_N_ELEMENTS (media_tests); i++) {
    GFile *file;
    gchar *input;
    GError *error = NULL;

    file = g_file_new_for_uri (media_tests[i].uri);
    g_file_load_contents (file, NULL, &input, NULL, NULL, &error);
    g_assert_no_error (error);
    g_object_unref (file);
    resolve_fake_src (input, media_tests[i].url);
    g_free (input);
  }
}

static void
test_related_keys (void)
{
  gint i;

  struct {
    gchar *uri;
    gchar *url;
  } media_tests[] = {
    /* This is a basic test to check that related keys are correctly set. */
    { GRESOURCE_PREFIX "grl-media-test-related-keys.json",
      TEST_GRL_MEDIA_URL "related-keys.json" }
  };

  for (i = 0; i < G_N_ELEMENTS (media_tests); i++) {
    GFile *file;
    gchar *input;
    GError *error = NULL;

    file = g_file_new_for_uri (media_tests[i].uri);
    g_file_load_contents (file, NULL, &input, NULL, NULL, &error);
    g_assert_no_error (error);
    g_object_unref (file);
    resolve_fake_src (input, media_tests[i].url);
    g_free (input);
  }
}

gint
main (gint argc, gchar **argv)
{
  setlocale (LC_ALL, "");

  g_setenv ("GRL_PLUGIN_PATH", LUA_FACTORY_PLUGIN_PATH, TRUE);
  g_setenv ("GRL_PLUGIN_LIST", LUA_FACTORY_ID, TRUE);
  g_setenv ("GRL_NET_MOCKED", LUA_FACTORY_DATA_PATH "config.ini", TRUE);
  g_setenv ("GRL_LUA_SOURCES_PATH", LUA_FACTORY_DATA_PATH, TRUE);

  grl_init (&argc, &argv);
  g_test_init (&argc, &argv, NULL);

  test_lua_factory_setup (NULL);

  /* Check if metadata-keys are created with value we expect with no errors */
  g_test_add_func ("/lua-factory/lua-library/metadata-keys", test_build_media);

  /* test GrlRelatedKeys */
  g_test_add_func ("/lua-factory/lua-library/related-keys", test_related_keys);

  /* TODO:
   * keys with array of all provided by grl_data_add_* (binary, boxed, float,..)
   */

  gint result = g_test_run ();

  test_lua_factory_shutdown ();
  grl_deinit ();

  return result;
}
