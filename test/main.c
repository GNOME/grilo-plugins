#include <glib.h>
#include <string.h>

#include "fake-source.h"
#include "youtube-source.h"
#include "fake-metadata-source.h"
#include "ms-plugin-registry.h"

static void
print_metadata (MsContent *content, MsKeyID key_id)
{
  /* Do not print "comment" */
  if (key_id == MS_METADATA_KEY_DESCRIPTION) {
    return;
  }

  MsPluginRegistry *registry = ms_plugin_registry_get_instance ();
  const MsMetadataKey *key = 
    ms_plugin_registry_lookup_metadata_key (registry, GPOINTER_TO_UINT (key_id));

  const GValue *value = ms_content_get (MS_CONTENT(content), key_id);
  if (value && G_VALUE_HOLDS_STRING (value)) {
    g_print ("\t%s: %s\n", MS_METADATA_KEY_GET_NAME (key), g_value_get_string (value));
  } else if (value && G_VALUE_HOLDS_INT (value)) {
    g_print ("\t%s: %d\n",  MS_METADATA_KEY_GET_NAME (key), g_value_get_int (value));
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

  g_print ("  browse/search result callback (%d)\n", browse_id);

  if (!media)
    return;

  keys = ms_content_get_keys (media, &size);
  for (i = 0; i < size; i++) {
    print_metadata (media, keys[i]);
  }
  g_free (keys);
  g_object_unref (media);
}

static void
metadata_cb (MsMetadataSource *source,
	     MsContent *media,
	     gpointer user_data,
	     const GError *error)
{
  gint size;
  gint i;

  g_print ("  metadata result callback\n");

  if (!media)
    return;

  MsKeyID *keys = ms_content_get_keys (media, &size);
  for (i = 0; i < size; i++) {
    print_metadata (media, keys[i]);
  }
  g_free (keys);
  g_object_unref (media);
}

gint
main (void)
{
  gchar *name;
  GList *keys;

  g_type_init ();

  keys = ms_metadata_key_list_new (MS_METADATA_KEY_TITLE,
                                   MS_METADATA_KEY_URL,
                                   MS_METADATA_KEY_ALBUM,
                                   MS_METADATA_KEY_ARTIST,
                                   MS_METADATA_KEY_GENRE,
                                   MS_METADATA_KEY_THUMBNAIL,
                                   MS_METADATA_KEY_LYRICS,
                                   NULL);

  g_print ("start\n");

  g_print ("loading plugins\n");

  MsPluginRegistry *registry = ms_plugin_registry_get_instance ();
  ms_plugin_registry_load_all (registry);

  g_print ("Obtaining sources\n");

  MsMediaSource *source = 
    (MsMediaSource *) ms_plugin_registry_lookup_source (registry, "FakeMediaSourceId");
  MsMetadataSource *metadata_source = 
    (MsMetadataSource *) ms_plugin_registry_lookup_source (registry, "FakeMetadataSourceId");
  MsMediaSource *youtube = 
    (MsMediaSource *) ms_plugin_registry_lookup_source (registry, "YoutubeSourceId");

  g_assert (source && metadata_source && youtube);

  g_print ("sources created\n");

  g_print ("Testing methods\n");

  if (0) ms_media_source_browse (source, NULL, NULL, 0, 0, 0, browse_cb, NULL);
  if (0) ms_media_source_search (source, NULL, NULL, NULL, 0, 0, MS_METADATA_RESOLUTION_FULL, browse_cb, NULL);
  if (0) ms_metadata_source_get (MS_METADATA_SOURCE (source), NULL, NULL, 0, metadata_cb, NULL);

  if (0) ms_media_source_browse (youtube, NULL, keys, 0, 0, MS_METADATA_RESOLUTION_FULL, browse_cb, NULL);
  if (0) ms_media_source_browse (youtube, NULL, keys, 0, 0, 0, browse_cb, NULL);
  if (0) ms_media_source_search (youtube, "igalia", keys, NULL, 0, 0, MS_METADATA_RESOLUTION_FULL, browse_cb, NULL);
  if (1) ms_metadata_source_get (MS_METADATA_SOURCE (youtube), "IQJx4YL3Pl8", keys, MS_METADATA_RESOLUTION_FULL, metadata_cb, NULL);

  if (0) ms_metadata_source_get (metadata_source, NULL, NULL, MS_METADATA_RESOLUTION_FULL, metadata_cb, NULL);

  g_print ("testing properties\n");
  
  g_object_get (source, "source-name", &name, NULL);
  g_print ("  Source Name: %s\n", name);

  g_object_get (youtube, "source-name", &name, NULL);
  g_print ("  Source Name: %s\n", name);

  g_object_get (metadata_source, "source-name", &name, NULL);
  g_print ("  Source Name: %s\n", name);

  g_print ("done\n");

  return 0;
}
