#include <glib.h>
#include <string.h>

#include "../src/plugin-registry.h"

static void 
search_cb (MediaSource *source,
	   guint browse_id,
           Content *media,
	   guint remaining,
	   gpointer user_data,
	   const GError *error)
{
  const GValue *value = NULL;

  if (media) {
    value = content_get (media, METADATA_KEY_ID);
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

  PluginRegistry *registry = plugin_registry_get_instance ();

  plugin_registry_load_all (registry);

  g_print ("Looking up youtube source\n");

  MediaSource *yt = 
    (MediaSource *) plugin_registry_lookup_source (registry, "YoutubeSourceId");

  if (yt) {
    g_print ("Found! Plugin properties:\n");
    g_print ("  Plugin ID: %s\n", media_plugin_get_id (MEDIA_PLUGIN (yt)));
    g_print ("  Plugin Name: %s\n", media_plugin_get_name (MEDIA_PLUGIN (yt)));
    g_print ("  Plugin Desc: %s\n", media_plugin_get_description (MEDIA_PLUGIN (yt)));
    g_print ("  Plugin License: %s\n", media_plugin_get_license (MEDIA_PLUGIN (yt)));
    g_print ("  Plugin Version: %s\n", media_plugin_get_version (MEDIA_PLUGIN (yt)));
   } else {
    g_print ("Not Found\n");
    goto exit;
  }

  g_print ("searching igalia on youtube\n");

  if(0) media_source_search (yt, "igalia", NULL, 0, 0, 0, search_cb, NULL);

  plugin_registry_unload (registry, "youtube-plugin-id");

 exit:
  g_print ("done\n");

  return 0;
}
