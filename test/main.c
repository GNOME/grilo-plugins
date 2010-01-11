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
}

static void
print_metadata (MsContent *content, MsKeyID key_id)
{
  if (key_id == MS_METADATA_KEY_DESCRIPTION) {
    return;
  }

  MsPluginRegistry *registry = ms_plugin_registry_get_instance ();
  const MsMetadataKey *key = 
    ms_plugin_registry_lookup_metadata_key (registry,
					    GPOINTER_TO_UINT (key_id));

  const GValue *value = ms_content_get (MS_CONTENT(content), key_id);
  if (value && G_VALUE_HOLDS_STRING (value)) {
    g_debug ("\t%s: %s", MS_METADATA_KEY_GET_NAME (key),
	     g_value_get_string (value));
  } else if (value && G_VALUE_HOLDS_INT (value)) {
    g_debug ("\t%s: %d",  MS_METADATA_KEY_GET_NAME (key),
	     g_value_get_int (value));
  }
}

static void 
browse_cb (MsMediaSource *source,
	   guint browse_id,
           MsContent *media,
	   guint remaining,
	   gpointer user_data,
	   const GError *error)
{
  MsKeyID *keys;
  gint size;
  gint i;
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
	   IS_MS_CONTENT_BOX(media) ? "yes" : "no");

  keys = ms_content_get_keys (media, &size);
  for (i = 0; i < size; i++) {
    print_metadata (media, keys[i]);
  }
  g_free (keys);
  g_object_unref (media);

  if (remaining == 0) {
    g_debug ("  Browse operation finished");
  }
}

static void 
metadata_cb (MsMediaSource *source,
	     MsContent *media,
	     gpointer user_data,
	     const GError *error)
{
  MsKeyID *keys;
  gint size;
  gint i;

  g_debug ("  metadata_cb");

  if (error) {
    g_debug ("Error: %s", error->message);
    return;
  }
  
  g_debug ("    Got metadata for object '%s'",
	   ms_content_media_get_id (MS_CONTENT_MEDIA (media)));

  g_debug ("\tContainer: %s",
	   IS_MS_CONTENT_BOX(media) ? "yes" : "no");

  keys = ms_content_get_keys (media, &size);
  for (i = 0; i < size; i++) {
    print_metadata (media, keys[i]);
  }
  g_free (keys);
  g_object_unref (media);

  g_debug ("  Metadata operation finished");
}

gint
main (void)
{
  GList *keys;

  g_type_init ();

  ms_log_init ("*:warning,test-main:*,ms-youtube:*,ms-filesystem:*");

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
  ms_plugin_registry_load (registry, "../plugins/fake-metadata/.libs/libfakemetadata.so");

  g_debug ("Obtaining sources");

  MsMediaSource *youtube = 
    (MsMediaSource *) ms_plugin_registry_lookup_source (registry, "ms-youtube");

  MsMediaSource *fs = 
    (MsMediaSource *) ms_plugin_registry_lookup_source (registry, "ms-filesystem");

  MsMetadataSource *fake = 
    (MsMetadataSource *) ms_plugin_registry_lookup_source (registry, "ms-fake-metadata");

  g_assert (youtube && fs && fake);

  g_debug ("Supported operations");

  print_supported_ops (MS_METADATA_SOURCE (youtube));
  print_supported_ops (MS_METADATA_SOURCE (fs));
  print_supported_ops (fake);

  g_debug ("testing");

  if (0) ms_media_source_browse (youtube, NULL, keys, 0, 5, MS_RESOLVE_IDLE_RELAY , browse_cb, NULL);
  if (0) ms_media_source_browse (youtube, "standard-feeds", keys, 0, 10, MS_RESOLVE_IDLE_RELAY | MS_RESOLVE_FULL | MS_RESOLVE_FAST_ONLY , browse_cb, NULL);
  if (0) ms_media_source_browse (youtube, "categories/Sports", keys,  0, 4, MS_RESOLVE_FULL, browse_cb, NULL);
  if (0) ms_media_source_search (youtube, "igalia", keys, NULL, 1, 3, MS_RESOLVE_FULL, browse_cb, NULL);
  if (1) ms_media_source_search (youtube, "igalia", keys, NULL, 1, 10, MS_RESOLVE_FULL | MS_RESOLVE_IDLE_RELAY | MS_RESOLVE_FAST_ONLY, browse_cb, NULL);
  if (0) ms_media_source_metadata (youtube, NULL, keys, 0, metadata_cb, NULL);
  if (0) ms_media_source_metadata (youtube, "okVW_YSHSPU", keys, MS_RESOLVE_IDLE_RELAY | MS_RESOLVE_FAST_ONLY | MS_RESOLVE_FULL, metadata_cb, NULL);
  if (0) ms_media_source_metadata (youtube, "categories", keys, MS_RESOLVE_IDLE_RELAY | MS_RESOLVE_FAST_ONLY , metadata_cb, NULL);
  if (0) ms_media_source_metadata (youtube, "categories", keys, 0, metadata_cb, NULL);
  if (1) ms_media_source_browse (fs, "/home", keys, 0, 100, MS_RESOLVE_IDLE_RELAY | MS_RESOLVE_FULL, browse_cb, NULL);
  if (0) ms_media_source_metadata (fs, "/home", keys, MS_RESOLVE_IDLE_RELAY | MS_RESOLVE_FULL, metadata_cb, NULL);

  g_debug ("Running main loop");

  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_debug ("done");

  return 0;
}
