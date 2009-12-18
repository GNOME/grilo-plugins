#include <glib.h>
#include <string.h>

#include "../src/ms-plugin-registry.h"

static void 
search_cb (MsMediaSource *source,
	   guint browse_id,
           MsContent *media,
	   guint remaining,
	   gpointer user_data,
	   const GError *error)
{
  const GValue *value = NULL;

  if (media) {
    value = ms_content_get (media, MS_METADATA_KEY_ID);
  }

  if (value) {
    g_print ("  Got video id: %s\n", g_value_get_string (value));
  }
}

gint
main (void)
{
  g_type_init ();

  g_print ("start\n");

  g_print ("Loading libyoutube.so\n");

  MsPluginRegistry *registry = ms_plugin_registry_get_instance ();

  ms_plugin_registry_load_all (registry);

  g_print ("Looking up youtube source\n");

  MsMediaSource *yt = 
    (MsMediaSource *) ms_plugin_registry_lookup_source (registry, "YoutubeSourceId");

  if (yt) {
    g_print ("Found! Plugin properties:\n");
    g_print ("  Plugin ID: %s\n", ms_media_plugin_get_id (MS_MEDIA_PLUGIN (yt)));
    g_print ("  Plugin Name: %s\n", ms_media_plugin_get_name (MS_MEDIA_PLUGIN (yt)));
    g_print ("  Plugin Desc: %s\n", ms_media_plugin_get_description (MS_MEDIA_PLUGIN (yt)));
    g_print ("  Plugin License: %s\n", ms_media_plugin_get_license (MS_MEDIA_PLUGIN (yt)));
    g_print ("  Plugin Version: %s\n", ms_media_plugin_get_version (MS_MEDIA_PLUGIN (yt)));
   } else {
    g_print ("Not Found\n");
    goto exit;
  }

  g_print ("searching igalia on youtube\n");

  if(0) ms_media_source_search (yt, "igalia", NULL, NULL, 0, 0, 0, search_cb, NULL);

  ms_plugin_registry_unload (registry, "youtube-plugin-id");

 exit:
  g_print ("done\n");

  return 0;
}
