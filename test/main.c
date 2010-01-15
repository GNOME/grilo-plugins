#include <glib.h>
#include <string.h>

#include <media-store.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "test-main"

static void
print_supported_ops (MsMetadataSource *source)
{
  g_debug ("  Operations available in '%s'",
	   ms_metadata_source_get_name (source));

  MsSupportedOps caps = ms_metadata_source_supported_operations (source);

  if (caps & MS_OP_METADATA) {
    g_debug ("    + Metadata");
  }
  if (caps & MS_OP_RESOLVE) {
    g_debug ("    + Resolution");
  }
  if (caps & MS_OP_BROWSE) {
    g_debug ("    + Browse");
  }
  if (caps & MS_OP_SEARCH) {
    g_debug ("    + Search");
  }
  if (caps & MS_OP_QUERY) {
    g_debug ("    + Query");
  }
}

static void
print_metadata (gpointer key, MsContent *content)
{
  MsKeyID key_id = POINTER_TO_MSKEYID(key);

  if (key_id == MS_METADATA_KEY_DESCRIPTION) {
    return;
  }

  MsPluginRegistry *registry = ms_plugin_registry_get_instance ();
  const MsMetadataKey *mkey =
    ms_plugin_registry_lookup_metadata_key (registry, key_id);

  const GValue *value = ms_content_get (content, key_id);
  if (value && G_VALUE_HOLDS_STRING (value)) {
    g_debug ("\t%s: %s", MS_METADATA_KEY_GET_NAME (mkey),
	     g_value_get_string (value));
  } else if (value && G_VALUE_HOLDS_INT (value)) {
    g_debug ("\t%s: %d",  MS_METADATA_KEY_GET_NAME (mkey),
	     g_value_get_int (value));
  }
}

static MsContentMedia *
media_from_id (const gchar *id)
{
  MsContentMedia *media;
  media = ms_content_media_new ();
  ms_content_media_set_id (media, id);
  return media;
}

static void
browse_cb (MsMediaSource *source,
	   guint browse_id,
           MsContentMedia *media,
	   guint remaining,
	   gpointer user_data,
	   const GError *error)
{
  GList *keys;
  static guint index = 0;

  g_debug ("  browse result (%d - %d|%d)",
	   browse_id, index++, remaining);

  if (error) {
    g_error ("Got error from browse: %s", error->message);
  }

  if (!media && remaining == 0) {
    g_debug ("  No results");
    return;
  }

  g_debug ("\tContainer: %s",
	   MS_IS_CONTENT_BOX(media) ? "yes" : "no");

  keys = ms_content_get_keys (MS_CONTENT (media));
  g_list_foreach (keys, (GFunc) print_metadata, MS_CONTENT (media));
  g_list_free (keys);
  g_object_unref (media);

  if (remaining == 0) {
    g_debug ("  Browse operation finished");
  }
}

static void
metadata_cb (MsMediaSource *source,
	     MsContentMedia *media,
	     gpointer user_data,
	     const GError *error)
{
  GList *keys;

  g_debug ("  metadata_cb");

  if (error) {
    g_debug ("Error: %s", error->message);
    return;
  }

  g_debug ("    Got metadata for object '%s'",
	   ms_content_media_get_id (MS_CONTENT_MEDIA (media)));

  g_debug ("\tContainer: %s",
	   MS_IS_CONTENT_BOX(media) ? "yes" : "no");

  keys = ms_content_get_keys (MS_CONTENT (media));
  g_list_foreach (keys, (GFunc) print_metadata, MS_CONTENT (media));
  g_list_free (keys);
  g_object_unref (media);

  g_debug ("  Metadata operation finished");
}

gint
main (void)
{
  GList *keys;

  g_type_init ();

  ms_log_init ("*:warning,test-main:*,ms-youtube:*,ms-filesystem:*,ms-jamendo:*");

  keys = ms_metadata_key_list_new (MS_METADATA_KEY_ID,
				   MS_METADATA_KEY_TITLE,
				   MS_METADATA_KEY_URL,
                                   MS_METADATA_KEY_ALBUM,
                                   MS_METADATA_KEY_ARTIST,
                                   MS_METADATA_KEY_GENRE,
                                   MS_METADATA_KEY_THUMBNAIL,
				   MS_METADATA_KEY_DESCRIPTION,
				   MS_METADATA_KEY_AUTHOR,
                                   MS_METADATA_KEY_LYRICS,
				   MS_METADATA_KEY_DURATION,
				   MS_METADATA_KEY_CHILDCOUNT,
				   MS_METADATA_KEY_MIME,
                                   NULL);

  g_debug ("start");

  g_debug ("loading plugins");

  MsPluginRegistry *registry = ms_plugin_registry_get_instance ();
  ms_plugin_registry_load (registry, "../plugins/youtube/.libs/libmsyoutube.so");
  ms_plugin_registry_load (registry, "../plugins/filesystem/.libs/libmsfilesystem.so");
  ms_plugin_registry_load (registry, "../plugins/jamendo/.libs/libmsjamendo.so");
  ms_plugin_registry_load (registry, "../plugins/fake-metadata/.libs/libfakemetadata.so");

  g_debug ("Obtaining sources");

  MsMediaSource *youtube = 
    (MsMediaSource *) ms_plugin_registry_lookup_source (registry, "ms-youtube");

  MsMediaSource *fs = 
    (MsMediaSource *) ms_plugin_registry_lookup_source (registry, "ms-filesystem");

  MsMediaSource *jamendo =
    (MsMediaSource *) ms_plugin_registry_lookup_source (registry, "ms-jamendo");

  MsMetadataSource *fake = 
    (MsMetadataSource *) ms_plugin_registry_lookup_source (registry, "ms-fake-metadata");

  g_assert (youtube && fs && jamendo && fake);

  g_debug ("Supported operations");

  print_supported_ops (MS_METADATA_SOURCE (youtube));
  print_supported_ops (MS_METADATA_SOURCE (fs));
  print_supported_ops (MS_METADATA_SOURCE (jamendo));
  print_supported_ops (fake);

  g_debug ("testing");

  if (0) ms_media_source_browse (youtube, NULL, keys, 0, 5, MS_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) ms_media_source_browse (youtube, NULL, keys, 0, 5, MS_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) ms_media_source_browse (youtube, media_from_id ("standard-feeds/most-viewed"), keys, 0, 10, MS_RESOLVE_FAST_ONLY , browse_cb, NULL);
  if (0) ms_media_source_browse (youtube, media_from_id ("categories/Sports"), keys,  0, 173, MS_RESOLVE_FAST_ONLY, browse_cb, NULL);
  if (0) ms_media_source_search (youtube, "igalia", keys, 6, 10, MS_RESOLVE_FAST_ONLY, browse_cb, NULL);
  if (0) ms_media_source_search (youtube, "igalia", keys, 1, 10, MS_RESOLVE_FULL | MS_RESOLVE_IDLE_RELAY | MS_RESOLVE_FAST_ONLY, browse_cb, NULL);
  if (0) ms_media_source_metadata (youtube, NULL, keys, 0, metadata_cb, NULL);
  if (0) ms_media_source_metadata (youtube, NULL, keys, MS_RESOLVE_IDLE_RELAY | MS_RESOLVE_FAST_ONLY | MS_RESOLVE_FULL, metadata_cb, NULL);
  if (0) ms_media_source_metadata (youtube, NULL, keys, MS_RESOLVE_IDLE_RELAY | MS_RESOLVE_FAST_ONLY , metadata_cb, NULL);
  if (0) ms_media_source_metadata (youtube, NULL, keys, 0, metadata_cb, NULL);
  if (0) ms_media_source_browse (fs, media_from_id ("/home"), keys, 0, 100, MS_RESOLVE_IDLE_RELAY | MS_RESOLVE_FULL, browse_cb, NULL);
  if (0) ms_media_source_metadata (fs, media_from_id ("/home"), keys, MS_RESOLVE_IDLE_RELAY | MS_RESOLVE_FULL, metadata_cb, NULL);
  if (0) ms_media_source_browse (jamendo, NULL, keys, 0, 5, MS_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) ms_media_source_browse (jamendo, media_from_id("1"), keys, 0, 5, MS_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) ms_media_source_browse (jamendo, media_from_id("1/9"), keys, 0, 5, MS_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) ms_media_source_browse (jamendo, media_from_id("2"), keys, -1, 2, MS_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) ms_media_source_browse (jamendo, media_from_id("2/25"), keys, -1, 2, MS_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) ms_media_source_browse (jamendo, media_from_id("2/1225"), keys, -1, 2, MS_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) ms_media_source_browse (jamendo, media_from_id("3/174"), keys, -1, 2, MS_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) ms_media_source_metadata (jamendo, NULL, keys, 0, metadata_cb, NULL);
  if (0) ms_media_source_metadata (jamendo, media_from_id("1"), keys, 0, metadata_cb, NULL);
  if (0) ms_media_source_metadata (jamendo, media_from_id("1/9"), keys, 0, metadata_cb, NULL);
  if (1) ms_media_source_metadata (jamendo, media_from_id("2/1225"), keys, 0, metadata_cb, NULL);
  if (0) ms_media_source_metadata (jamendo, media_from_id("3/174"), keys, 0, metadata_cb, NULL);
  if (0) ms_media_source_query (jamendo, "artist=shake da", keys, 0, 5, MS_RESOLVE_FAST_ONLY, browse_cb, NULL);
  if (0) ms_media_source_query (jamendo, "album=Nick", keys, 0, 5, MS_RESOLVE_FAST_ONLY, browse_cb, NULL);
  if (0) ms_media_source_query (jamendo, "track=asylum mind", keys, 0, 5, MS_RESOLVE_FAST_ONLY, browse_cb, NULL);
  if (0) ms_media_source_search (jamendo, "next", keys, 0, 5, MS_RESOLVE_FAST_ONLY, browse_cb, NULL);
  
  g_debug ("Running main loop");

  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_debug ("done");

  return 0;
}
