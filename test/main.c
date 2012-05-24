/*
 * Copyright (C) 2010, 2011 Igalia S.L.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
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

#include <glib.h>
#include <string.h>

#include <grilo.h>

#define GRL_LOG_DOMAIN_DEFAULT test_main_log_domain
GRL_LOG_DOMAIN_STATIC(test_main_log_domain);

#define YOUTUBE_KEY "AI39si4EfscPllSfUy1IwexMf__kntTL_G5dfSr2iUEVN45RHGq92Aq0lX25OlnOkG6KTN-4soVAkAf67fWYXuHfVADZYr7S1A"

static void
print_supported_ops (GrlMetadataSource *source)
{
  GRL_DEBUG ("  Operations available in '%s'",
             grl_metadata_source_get_name (source));

  GrlSupportedOps caps = grl_metadata_source_supported_operations (source);

  if (caps & GRL_OP_METADATA) {
    GRL_DEBUG ("    + Metadata");
  }
  if (caps & GRL_OP_RESOLVE) {
    GRL_DEBUG ("    + Resolution");
  }
  if (caps & GRL_OP_BROWSE) {
    GRL_DEBUG ("    + Browse");
  }
  if (caps & GRL_OP_SEARCH) {
    GRL_DEBUG ("    + Search");
  }
  if (caps & GRL_OP_QUERY) {
    GRL_DEBUG ("    + Query");
  }
  if (caps & GRL_OP_STORE_PARENT) {
    GRL_DEBUG ("    + Store (parent)");
  } else  if (caps & GRL_OP_STORE) {
    GRL_DEBUG ("    + Store");
  }
  if (caps & GRL_OP_REMOVE) {
    GRL_DEBUG ("    + Remove");
  }
  if (caps & GRL_OP_SET_METADATA) {
    GRL_DEBUG ("    + Set Metadata");
  }
}

static void
print_metadata (gpointer key, GrlData *content)
{
  if (key == GRL_METADATA_KEY_DESCRIPTION) {
    return;
  }

  const GValue *value = grl_data_get (content, key);
  if (value && G_VALUE_HOLDS_STRING (value)) {
    GRL_DEBUG ("\t%s: %s", GRL_METADATA_KEY_GET_NAME (key),
               g_value_get_string (value));
  } else if (value && G_VALUE_HOLDS_INT (value)) {
    GRL_DEBUG ("\t%s: %d",  GRL_METADATA_KEY_GET_NAME (key),
               g_value_get_int (value));
  }
}

static GrlMedia *
media_from_id (const gchar *id)
{
  GrlMedia *media;
  media = grl_media_new ();
  grl_media_set_id (media, id);
  return media;
}

static GrlMedia *
box_from_id (const gchar *id)
{
  GrlMedia *media;
  media = grl_media_box_new ();
  grl_media_set_id (media, id);
  return media;
}

static void
browse_cb (GrlMediaSource *source,
	   guint browse_id,
           GrlMedia *media,
	   guint remaining,
	   gpointer user_data,
	   const GError *error)
{
  GList *keys;
  static guint index = 0;

  GRL_DEBUG ("  browse result (%d - %d|%d)", browse_id, index++, remaining);

  if (error) {
    g_error ("Got error from browse: %s", error->message);
  }

  if (!media && remaining == 0) {
    GRL_DEBUG ("  No results");
    return;
  }

  GRL_DEBUG ("\tContainer: %s", GRL_IS_MEDIA_BOX(media) ? "yes" : "no");

  keys = grl_data_get_keys (GRL_DATA (media));
  g_list_foreach (keys, (GFunc) print_metadata, GRL_DATA (media));
  g_list_free (keys);
  g_object_unref (media);

  if (remaining == 0) {
    GRL_DEBUG ("  Browse operation finished");
  }
}

static void
metadata_cb (GrlMediaSource *source,
             guint operation_id,
	     GrlMedia *media,
	     gpointer user_data,
	     const GError *error)
{
  GList *keys;

  GRL_DEBUG ("  metadata_cb");

  if (error) {
    GRL_DEBUG ("Error: %s", error->message);
    return;
  }

  GRL_DEBUG ("    Got metadata for object '%s'",
             grl_media_get_id (GRL_MEDIA (media)));

  GRL_DEBUG ("\tContainer: %s", GRL_IS_MEDIA_BOX(media) ? "yes" : "no");

  keys = grl_data_get_keys (GRL_DATA (media));
  g_list_foreach (keys, (GFunc) print_metadata, GRL_DATA (media));
  g_list_free (keys);
  g_object_unref (media);

  GRL_DEBUG ("  Metadata operation finished");
}

static void
resolve_cb (GrlMetadataSource *source,
            guint resolve_id,
            GrlMedia *media,
            gpointer user_data,
            const GError *error)
{
  metadata_cb (NULL, 0, media, user_data, error);
}

static void
set_cb (GrlMetadataSource *source,
	GrlMedia *media,
	GList *failed_keys,
	gpointer user_data,
	const GError *error)
{
  if (error) {
    g_critical ("%s: %d keys not written",
                error->message, g_list_length (failed_keys));
  }
}

gint
main (void)
{
  GList *keys;

  g_type_init ();
  grl_init (NULL, NULL);

  GRL_LOG_DOMAIN_INIT (test_main_log_domain, "test-main");

  grl_log_configure ("*:warning,test-main:*,"
                     "grl-youtube:*,"
                     "grl-filesystem:*,"
                     "grl-jamendo:*,"
                     "grl-shoutcast:*,"
                     "grl-apple-trailers:*,"
                     "grl-lastfm-albumart:*,"
                     "grl-flickr:*,"
                     "grl-metadata-store:*");

  keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
                                    GRL_METADATA_KEY_TITLE,
                                    GRL_METADATA_KEY_URL,
                                    GRL_METADATA_KEY_ALBUM,
                                    GRL_METADATA_KEY_ARTIST,
                                    GRL_METADATA_KEY_GENRE,
                                    GRL_METADATA_KEY_THUMBNAIL,
                                    GRL_METADATA_KEY_DESCRIPTION,
                                    GRL_METADATA_KEY_AUTHOR,
                                    GRL_METADATA_KEY_LYRICS,
                                    GRL_METADATA_KEY_DURATION,
                                    GRL_METADATA_KEY_CHILDCOUNT,
                                    GRL_METADATA_KEY_MIME,
                                    GRL_METADATA_KEY_PLAY_COUNT,
                                    GRL_METADATA_KEY_LAST_PLAYED,
                                    GRL_METADATA_KEY_LAST_POSITION,
                                    GRL_METADATA_KEY_RATING,
                                    NULL);

  GRL_DEBUG ("start");

  GRL_DEBUG ("loading plugins");

  GrlPluginRegistry *registry = grl_plugin_registry_get_default ();

  GrlConfig *config = grl_config_new ("grl-youtube", NULL);
  grl_config_set_api_key (config, YOUTUBE_KEY);
  grl_plugin_registry_add_config (registry, config, NULL);

  grl_plugin_registry_load (registry,
                            "../src/media/youtube/.libs/libgrlyoutube.so",
                            NULL);
  grl_plugin_registry_load (registry,
                            "../src/media/filesystem/.libs/libgrlfilesystem.so",
                            NULL);
  grl_plugin_registry_load (registry,
                            "../src/media/jamendo/.libs/libgrljamendo.so",
                            NULL);
  grl_plugin_registry_load (registry,
                            "../src/media/shoutcast/.libs/libgrlshoutcast.so",
                            NULL);
  grl_plugin_registry_load (registry,
                            "../src/media/apple-trailers/.libs/libgrlappletrailers.so",
                            NULL);
  grl_plugin_registry_load (registry,
                            "../src/metadata/fake-metadata/.libs/libgrlfakemetadata.so",
                            NULL);
  grl_plugin_registry_load (registry,
                            "../src/metadata/lastfm-albumart/.libs/libgrllastfm-albumart.so",
                            NULL);
  grl_plugin_registry_load (registry,
                            "../src/media/flickr/.libs/libgrlflickr.so",
                            NULL);
  grl_plugin_registry_load (registry,
                            "../src/metadata/metadata-store/.libs/libgrlmetadatastore.so",
                            NULL);
  grl_plugin_registry_load (registry,
                            "../src/media/bookmarks/.libs/libgrlbookmarks.so",
                            NULL);
  grl_plugin_registry_load (registry,
                            "../src/media/podcasts/.libs/libgrlpodcasts.so",
                            NULL);

  GRL_DEBUG ("Obtaining sources");

  GrlMediaSource *youtube =
    (GrlMediaSource *) grl_plugin_registry_lookup_source (registry,
                                                          "grl-youtube");

  GrlMediaSource *fs =
    (GrlMediaSource *) grl_plugin_registry_lookup_source (registry,
                                                          "grl-filesystem");

  GrlMediaSource *flickr =
    (GrlMediaSource *) grl_plugin_registry_lookup_source (registry,
                                                          "grl-flickr");

  GrlMediaSource *jamendo =
    (GrlMediaSource *) grl_plugin_registry_lookup_source (registry,
                                                          "grl-jamendo");

  GrlMediaSource *shoutcast =
    (GrlMediaSource *) grl_plugin_registry_lookup_source (registry,
                                                          "grl-shoutcast");

  GrlMediaSource *apple_trailers =
    (GrlMediaSource *) grl_plugin_registry_lookup_source (registry,
                                                          "grl-apple-trailers");

  GrlMetadataSource *fake =
    (GrlMetadataSource *) grl_plugin_registry_lookup_source (registry,
                                                             "grl-fake-metadata");

  GrlMetadataSource *lastfm =
    (GrlMetadataSource *) grl_plugin_registry_lookup_source (registry,
                                                             "grl-lastfm-albumart");

  GrlMetadataSource *metadata_store =
    (GrlMetadataSource *) grl_plugin_registry_lookup_source (registry,
                                                             "grl-metadata-store");

  GrlMediaSource *bookmarks =
    (GrlMediaSource *) grl_plugin_registry_lookup_source (registry,
                                                          "grl-bookmarks");

  GrlMediaSource *podcasts =
    (GrlMediaSource *) grl_plugin_registry_lookup_source (registry,
                                                          "grl-podcasts");

  g_assert (youtube);
  g_assert (fs);
  g_assert (jamendo);
  g_assert (shoutcast);
  g_assert (apple_trailers);
  g_assert (lastfm);
  g_assert (metadata_store);
  g_assert (bookmarks);
  g_assert (podcasts);
  GRL_DEBUG ("Supported operations");

  print_supported_ops (GRL_METADATA_SOURCE (youtube));
  print_supported_ops (GRL_METADATA_SOURCE (fs));
  if (flickr) print_supported_ops (GRL_METADATA_SOURCE (flickr));
  print_supported_ops (GRL_METADATA_SOURCE (jamendo));
  print_supported_ops (GRL_METADATA_SOURCE (apple_trailers));
  print_supported_ops (GRL_METADATA_SOURCE (shoutcast));
  if (fake) print_supported_ops (fake);
  print_supported_ops (GRL_METADATA_SOURCE (podcasts));
  print_supported_ops (GRL_METADATA_SOURCE (bookmarks));
  print_supported_ops (lastfm);
  print_supported_ops (metadata_store);

  GRL_DEBUG ("testing");

  if (0) grl_media_source_browse (youtube, NULL, keys, 0, 5, GRL_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) grl_media_source_browse (youtube, NULL, keys, 0, 5, GRL_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) grl_media_source_browse (youtube, media_from_id ("standard-feeds/most-viewed"), keys, 0, 10, GRL_RESOLVE_FAST_ONLY , browse_cb, NULL);
  if (0) grl_media_source_browse (youtube, media_from_id ("categories/Sports"), keys,  0, 5, GRL_RESOLVE_FAST_ONLY, browse_cb, NULL);
  if (0) grl_media_source_search (youtube, "igalia", keys, 0, 5, GRL_RESOLVE_NORMAL, browse_cb, NULL);
  if (0) grl_media_source_search (youtube, "igalia", keys, 1, 10, GRL_RESOLVE_FULL | GRL_RESOLVE_IDLE_RELAY | GRL_RESOLVE_FAST_ONLY, browse_cb, NULL);
  if (0) grl_media_source_metadata (youtube, NULL, keys, 0, metadata_cb, NULL);
  if (0) grl_media_source_metadata (youtube, NULL, keys, GRL_RESOLVE_IDLE_RELAY | GRL_RESOLVE_FAST_ONLY | GRL_RESOLVE_FULL, metadata_cb, NULL);
  if (0) grl_media_source_metadata (youtube, NULL, keys, GRL_RESOLVE_IDLE_RELAY | GRL_RESOLVE_FAST_ONLY , metadata_cb, NULL);
  if (0) grl_media_source_metadata (youtube, NULL, keys, 0, metadata_cb, NULL);
  if (0) grl_media_source_browse (fs, media_from_id ("/home"), keys, 0, 100, GRL_RESOLVE_IDLE_RELAY | GRL_RESOLVE_FULL, browse_cb, NULL);
  if (0) grl_media_source_metadata (fs, media_from_id ("/home"), keys, GRL_RESOLVE_IDLE_RELAY | GRL_RESOLVE_FULL, metadata_cb, NULL);
  if (0) grl_media_source_search (flickr, "igalia", keys, 1, 10, GRL_RESOLVE_FAST_ONLY, browse_cb, NULL);
  if (0) grl_media_source_metadata (flickr, media_from_id ("4201406347"), keys, GRL_RESOLVE_IDLE_RELAY | GRL_RESOLVE_FULL, metadata_cb, NULL);
  if (0) grl_media_source_browse (jamendo, NULL, keys, 0, 5, GRL_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) grl_media_source_browse (jamendo, media_from_id("1"), keys, 0, 5, GRL_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) grl_media_source_browse (jamendo, media_from_id("1/9"), keys, 0, 5, GRL_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) grl_media_source_browse (jamendo, media_from_id("2"), keys, -1, 2, GRL_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) grl_media_source_browse (jamendo, media_from_id("2/25"), keys, -1, 2, GRL_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) grl_media_source_browse (jamendo, media_from_id("2/1225"), keys, -1, 2, GRL_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) grl_media_source_browse (jamendo, media_from_id("3/174"), keys, -1, 2, GRL_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) grl_media_source_metadata (jamendo, NULL, keys, 0, metadata_cb, NULL);
  if (0) grl_media_source_metadata (jamendo, media_from_id("1"), keys, 0, metadata_cb, NULL);
  if (0) grl_media_source_metadata (jamendo, media_from_id("1/9"), keys, 0, metadata_cb, NULL);
  if (0) grl_media_source_metadata (jamendo, media_from_id("2/1225"), keys, 0, metadata_cb, NULL);
  if (0) grl_media_source_metadata (jamendo, media_from_id("3/174"), keys, 0, metadata_cb, NULL);
  if (0) grl_media_source_query (jamendo, "artist=shake da", keys, 0, 5, GRL_RESOLVE_FAST_ONLY, browse_cb, NULL);
  if (0) grl_media_source_query (jamendo, "album=Nick", keys, 0, 5, GRL_RESOLVE_FAST_ONLY, browse_cb, NULL);
  if (0) grl_media_source_query (jamendo, "track=asylum mind", keys, 0, 5, GRL_RESOLVE_FAST_ONLY, browse_cb, NULL);
  if (0) grl_media_source_search (jamendo, "next", keys, 0, 5, GRL_RESOLVE_FAST_ONLY, browse_cb, NULL);
  if (0) grl_media_source_browse (shoutcast, NULL, keys, 0, 5, GRL_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) grl_media_source_browse (shoutcast, media_from_id("American"), keys, 2, 5, GRL_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) grl_media_source_search (shoutcast, "Roxette", keys, 0, 5, GRL_RESOLVE_FAST_ONLY, browse_cb, NULL);
  if (0) grl_media_source_metadata (shoutcast, box_from_id("24hs"), keys, 0, metadata_cb, NULL);
  if (0) grl_media_source_metadata (shoutcast, box_from_id("2424hs"), keys, 0, metadata_cb, NULL);
  if (0) grl_media_source_metadata (shoutcast, media_from_id("American/556687"), keys, 0, metadata_cb, NULL);
  if (0) grl_media_source_metadata (shoutcast, media_from_id("American/556682"), keys, 0, metadata_cb, NULL);
  if (0) grl_media_source_browse (apple_trailers, NULL, keys, 0, 5, GRL_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) {
    GrlMedia *media = media_from_id ("test");
    grl_data_set_string (GRL_DATA (media),
                         GRL_METADATA_KEY_ARTIST,
                         "roxette");
    grl_data_set_string (GRL_DATA (media),
                         GRL_METADATA_KEY_ALBUM,
                         "pop hits");
    grl_metadata_source_resolve (lastfm, keys, media, GRL_RESOLVE_IDLE_RELAY, resolve_cb, NULL);
  }
  if (0) {
    GrlMedia *media = media_from_id ("test-id");
    grl_media_set_source (media, "some-source-id");
    grl_media_set_play_count (media,  68);
    grl_media_set_rating (media,  4.5, 5.00);
    grl_media_set_last_position (media, 60);
    grl_media_set_last_played (media, "19/11/2009");
    GList *keys_to_write = grl_metadata_key_list_new (GRL_METADATA_KEY_PLAY_COUNT,
                                                      GRL_METADATA_KEY_RATING,
                                                      GRL_METADATA_KEY_LAST_POSITION,
                                                      GRL_METADATA_KEY_ALBUM,
                                                      GRL_METADATA_KEY_LAST_PLAYED,
						      GRL_METADATA_KEY_TITLE,
                                                      GRL_METADATA_KEY_GENRE,
						      NULL);
    grl_metadata_source_set_metadata (metadata_store, media, keys_to_write,
				      GRL_WRITE_FULL, set_cb, NULL);
  }

  GRL_DEBUG ("Running main loop");

  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  GRL_DEBUG ("done");

  return 0;
}
