#include <glib.h>
#include <string.h>

#include "fake-source.h"
#include "youtube-source.h"
#include "fake-metadata-source.h"
#include "plugin-registry.h"

static void
print_hmetadata (gpointer k, gpointer v, gpointer user_data)
{
  PluginRegistry *registry = plugin_registry_get_instance ();
  const MetadataKey *key = 
    plugin_registry_lookup_metadata_key (registry, GPOINTER_TO_UINT (k));
  g_print ("    %s: %s\n", METADATA_KEY_GET_NAME (key), (gchar *) v);
}

static void
print_metadata (Content *content, KeyID key)
{
  /* Do not print "comment" */
  if (key == METADATA_KEY_DESCRIPTION) {
    return;
  }

  const GValue *value = content_get (CONTENT(content), key);
  if (value && G_VALUE_HOLDS_STRING (value)) {
    g_print ("\t%" KEYID_FORMAT ": %s\n", key, g_value_get_string (value));
  } else if (value && G_VALUE_HOLDS_INT (value)) {
    g_print ("\t%" KEYID_FORMAT ": %d\n", key, g_value_get_int (value));
  }
}

static void 
browse_cb (MediaSource *source,
	   guint browse_id,
           Content *media,
	   guint remaining,
	   gpointer user_data,
	   const GError *error)
{
  KeyID *keys;
  gint size;
  gint i;

  g_print ("  browse/search result callback (%d)\n", browse_id);

  if (!media)
    return;

  keys = content_get_keys (media, &size);
  for (i = 0; i < size; i++) {
    print_metadata (media, keys[i]);
  }
  g_free (keys);
  g_object_unref (media);
}

static void
metadata_cb (MetadataSource *source,
	     const gchar *media_id,
	     GHashTable *metadata,
	     gpointer user_data,
	     const GError *error)
{
  g_print ("  metadata result callback (%s)\n", media_id);

  if (!metadata)
    return;

  g_hash_table_foreach (metadata, print_hmetadata, NULL);
}

gint
main (void)
{
  gchar *name;
  KeyID keys[] = { METADATA_KEY_TITLE, METADATA_KEY_URL, METADATA_KEY_ALBUM,
		   METADATA_KEY_ARTIST, METADATA_KEY_GENRE, 
		   METADATA_KEY_THUMBNAIL, METADATA_KEY_LYRICS, 0};

  g_type_init ();

  g_print ("start\n");

  g_print ("loading plugins\n");

  PluginRegistry *registry = plugin_registry_get_instance ();
  plugin_registry_load_all (registry);

  g_print ("Obtaining sources\n");

  MediaSource *source = 
    (MediaSource *) plugin_registry_lookup_source (registry, "FakeMediaSourceId");
  MetadataSource *metadata_source = 
    (MetadataSource *) plugin_registry_lookup_source (registry, "FakeMetadataSourceId");
  MediaSource *youtube = 
    (MediaSource *) plugin_registry_lookup_source (registry, "YoutubeSourceId");

  g_assert (source && metadata_source && youtube);

  g_print ("sources created\n");

  g_print ("Testing methods\n");

  if (0) media_source_browse (source, NULL, NULL, 0, 0, 0, browse_cb, NULL);
  if (0) media_source_search (source, NULL, NULL, 0, 0, 0, browse_cb, NULL);
  if (0) metadata_source_get (METADATA_SOURCE (source), NULL, NULL, metadata_cb, NULL);

  media_source_browse (youtube, NULL, keys, 0, 0, METADATA_RESOLUTION_FULL, browse_cb, NULL);
  if (0) media_source_search (youtube, "igalia", NULL, 0, 0, 0, browse_cb, NULL);
  if (0) metadata_source_get (METADATA_SOURCE (youtube), "IQJx4YL3Pl8", NULL, metadata_cb, NULL);

  metadata_source_get (metadata_source, NULL, NULL, metadata_cb, NULL);

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
