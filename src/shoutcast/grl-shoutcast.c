/*
 * Copyright (C) 2010 Igalia S.L.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Juan A. Suarez Romero <jasuarez@igalia.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <grilo.h>
#include <gio/gio.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <string.h>
#include <stdlib.h>

#include "grl-shoutcast.h"

/* --------- Logging  -------- */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "grl-shoutcast"

/* ------ SHOUTcast API ------ */

#define SHOUTCAST_BASE_ENTRY "http://yp.shoutcast.com"

#define SHOUTCAST_GET_GENRES SHOUTCAST_BASE_ENTRY "/sbin/newxml.phtml"
#define SHOUTCAST_GET_RADIOS SHOUTCAST_GET_GENRES "?genre=%s&limit=%d"
#define SHOUTCAST_TUNE       SHOUTCAST_BASE_ENTRY "/sbin/tunein-station.pls?id=%s"

/* --- Plugin information --- */

#define PLUGIN_ID   "grl-shoutcast"
#define PLUGIN_NAME "SHOUTcast"
#define PLUGIN_DESC "A plugin for browsing SHOUTcast radios"

#define SOURCE_ID   "grl-shoutcast"
#define SOURCE_NAME "SHOUTcast"
#define SOURCE_DESC "A source for browsing SHOUTcast radios"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

typedef struct {
  GrlMediaSourceBrowseSpec *bs;
  xmlNodePtr xml_entries;
  xmlDocPtr xml_doc;
  gchar *genre;
} OperationData;

static GrlShoutcastSource *grl_shoutcast_source_new (void);

gboolean grl_shoutcast_plugin_init (GrlPluginRegistry *registry,
                                    const GrlPluginInfo *plugin);

static const GList *grl_shoutcast_source_supported_keys (GrlMetadataSource *source);


static void grl_shoutcast_source_browse (GrlMediaSource *source,
                                         GrlMediaSourceBrowseSpec *bs);


/* =================== SHOUTcast Plugin  =============== */

gboolean
grl_shoutcast_plugin_init (GrlPluginRegistry *registry,
                           const GrlPluginInfo *plugin)
{
  g_debug ("shoutcast_plugin_init\n");

  GrlShoutcastSource *source = grl_shoutcast_source_new ();
  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source));
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_shoutcast_plugin_init,
                     NULL,
                     PLUGIN_ID,
                     PLUGIN_NAME,
                     PLUGIN_DESC,
                     PACKAGE_VERSION,
                     AUTHOR,
                     LICENSE,
                     SITE);

/* ================== SHOUTcast GObject ================ */

static GrlShoutcastSource *
grl_shoutcast_source_new (void)
{
  g_debug ("grl_shoutcast_source_new");
  return g_object_new (GRL_SHOUTCAST_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       NULL);
}

static void
grl_shoutcast_source_class_init (GrlShoutcastSourceClass * klass)
{
  GrlMediaSourceClass *source_class = GRL_MEDIA_SOURCE_CLASS (klass);
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);
  source_class->browse = grl_shoutcast_source_browse;
  metadata_class->supported_keys = grl_shoutcast_source_supported_keys;
}

static void
grl_shoutcast_source_init (GrlShoutcastSource *source)
{
}

G_DEFINE_TYPE (GrlShoutcastSource, grl_shoutcast_source, GRL_TYPE_MEDIA_SOURCE);

/* ======================= Private ==================== */

static void
skip_garbage_nodes (xmlNodePtr *node)
{
  /* Result contains "\n" and "\t" to pretty align XML. Unfortunately, libxml
     doesn't cope very fine with them, and it creates "fakes" nodes with name
     "text" and value those characters. So we need to skip them */
  while ((*node) && xmlStrcmp ((*node)->name, (const xmlChar *) "text") == 0) {
    (*node) = (*node)->next;
  }
}

static gboolean
send_genrelist_entries (OperationData *op_data)
{
  GrlContentMedia *media;
  gchar *genre_name;
  gint remaining;

  media = grl_content_box_new ();
  genre_name = (gchar *) xmlGetProp (op_data->xml_entries,
                                     (const xmlChar *) "name");

  grl_content_media_set_id (media, genre_name);
  grl_content_media_set_title (media, genre_name);
  grl_content_set_string (GRL_CONTENT (media),
                          GRL_METADATA_KEY_GENRE,
                          genre_name);
  g_free (genre_name);

  if (!op_data->xml_entries->next ||
      op_data->bs->count == 1) {
    remaining = 0;
  } else {
    remaining = -1;
  }

  op_data->bs->callback (op_data->bs->source,
                         op_data->bs->browse_id,
                         media,
                         remaining,
                         op_data->bs->user_data,
                         NULL);

  op_data->bs->count--;
  op_data->xml_entries = op_data->xml_entries->next;
  skip_garbage_nodes (&op_data->xml_entries);

  if (!op_data->xml_entries || op_data->bs->count == 0) {
    xmlFreeDoc (op_data->xml_doc);
    g_free (op_data);
    return FALSE;
  } else {
    return TRUE;
  }
}

static gboolean
send_stationlist_entries (OperationData *op_data)
{
  GrlContentMedia *media;
  gchar *media_id;
  gchar *station_genre;
  gchar *station_id;
  gchar *station_mime;
  gchar *station_name;
  gint remaining;

  station_name = (gchar *) xmlGetProp (op_data->xml_entries,
                                       (const xmlChar *) "name");
  station_mime = (gchar *) xmlGetProp (op_data->xml_entries,
                                       (const xmlChar *) "mt");
  station_id = (gchar *) xmlGetProp (op_data->xml_entries,
                                     (const xmlChar *) "id");
  station_genre = (gchar *) xmlGetProp (op_data->xml_entries,
                                        (const xmlChar *) "genre");
  media_id = g_strconcat (op_data->genre, "/", station_id, NULL);

  media = grl_content_audio_new ();

  grl_content_media_set_id (media, media_id);
  grl_content_media_set_title (media, station_name);
  grl_content_media_set_mime (media, station_mime);
  grl_content_audio_set_genre (GRL_CONTENT_AUDIO (media), station_genre);

  g_free (station_name);
  g_free (station_mime);
  g_free (station_id);
  g_free (station_genre);
  g_free (media_id);

  if (!op_data->xml_entries->next ||
      op_data->bs->count == 1) {
    remaining = 0;
  } else {
    remaining = -1;
  }

  op_data->bs->callback (op_data->bs->source,
                         op_data->bs->browse_id,
                         media,
                         remaining,
                         op_data->bs->user_data,
                         NULL);

  op_data->bs->count--;
  op_data->xml_entries = op_data->xml_entries->next;
  skip_garbage_nodes (&op_data->xml_entries);

  if (!op_data->xml_entries || op_data->bs->count == 0) {
    xmlFreeDoc (op_data->xml_doc);
    g_free (op_data);
    return FALSE;
  } else {
    return TRUE;
  }
}

static void
xml_parse_result (const gchar *str, OperationData *op_data)
{
  GError *error = NULL;
  xmlNodePtr node;

  op_data->xml_doc = xmlRecoverDoc ((xmlChar *) str);
  if (!op_data->xml_doc) {
    error = g_error_new (GRL_ERROR,
                         GRL_ERROR_BROWSE_FAILED,
                         "Failed to parse SHOUTcast's response");
    goto finalize;
  }

  node = xmlDocGetRootElement (op_data->xml_doc);
  if  (!node) {
    error = g_error_new (GRL_ERROR,
                         GRL_ERROR_BROWSE_FAILED,
                         "Empty response from SHOUTcast");
    goto finalize;
  }

  op_data->xml_entries = node->xmlChildrenNode;
  skip_garbage_nodes (&op_data->xml_entries);

  if (xmlStrcmp (node->name, (const xmlChar *) "stationlist") == 0) {
    /* First node is "tunein"; skip it */
    op_data->xml_entries = op_data->xml_entries->next;
    skip_garbage_nodes (&op_data->xml_entries);
  }

  /* Skip elements */
  while (op_data->xml_entries && op_data->bs->skip > 0) {
    op_data->xml_entries = op_data->xml_entries->next;
    skip_garbage_nodes (&op_data->xml_entries);
    op_data->bs->skip--;
  }

  /* Check if there are elements to send*/
  if (!op_data->xml_entries || op_data->bs->count == 0) {
    goto finalize;
  }

  if (xmlStrcmp (node->name, (const xmlChar *) "genrelist") == 0) {
    g_idle_add ((GSourceFunc) send_genrelist_entries, op_data);
  } else if (xmlStrcmp (node->name, (const xmlChar *) "stationlist") == 0) {
    g_idle_add ((GSourceFunc) send_stationlist_entries, op_data);
  } else {
    /* Unknown response */
    error = g_error_new (GRL_ERROR,
                         GRL_ERROR_BROWSE_FAILED,
                         "Unkown response from SHOUTcast");
    goto finalize;
  }

  return;

 finalize:
  if (op_data->xml_doc) {
    xmlFreeDoc (op_data->xml_doc);
  }

  op_data->bs->callback (op_data->bs->source,
                         op_data->bs->browse_id,
                         NULL,
                         0,
                         op_data->bs->user_data,
                         error);

  if (error) {
    g_error_free (error);
  }
  g_free (op_data);
}

static void
read_done_cb (GObject *source_object,
              GAsyncResult *res,
              gpointer user_data)
{
  GError *error = NULL;
  GError *vfs_error = NULL;
  OperationData *op_data = (OperationData *) user_data;
  gchar *content = NULL;

  if (!g_file_load_contents_finish (G_FILE (source_object),
                                    res,
                                    &content,
                                    NULL,
                                    NULL,
                                    &vfs_error)) {
    error = g_error_new (GRL_ERROR,
                         GRL_ERROR_BROWSE_FAILED,
                         "Failed to connect SHOUTcast: '%s'",
                         vfs_error->message);
    op_data->bs->callback (op_data->bs->source,
                           op_data->bs->browse_id,
                           NULL,
                           0,
                           op_data->bs->user_data,
                           error);
    g_error_free (error);
    g_free (op_data);

    return;
  }

  xml_parse_result (content, op_data);
  g_free (content);
}

static void
read_url_async (const gchar *url, gpointer user_data)
{
  GVfs *vfs;
  GFile *uri;

  vfs = g_vfs_get_default ();

  g_debug ("Opening '%s'", url);
  uri = g_vfs_get_file_for_uri (vfs, url);
  g_file_load_contents_async (uri, NULL, read_done_cb, user_data);
}

/* ================== API Implementation ================ */

static const GList *
grl_shoutcast_source_supported_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_ARTIST,
                                      GRL_METADATA_KEY_ALBUM,
                                      GRL_METADATA_KEY_GENRE,
                                      GRL_METADATA_KEY_URL,
                                      GRL_METADATA_KEY_DURATION,
                                      GRL_METADATA_KEY_THUMBNAIL,
                                      GRL_METADATA_KEY_SITE,
                                      NULL);
  }
  return keys;
}

static void
grl_shoutcast_source_browse (GrlMediaSource *source,
                             GrlMediaSourceBrowseSpec *bs)
{
  OperationData *data;
  const gchar *container_id;
  gchar *url;

  g_debug ("grl_shoutcast_source_browse");

  data = g_new0 (OperationData, 1);
  data->bs = bs;

  container_id = grl_content_media_get_id (bs->container);

  /* If it's root category send list of genres; else send list of radios */
  if (!container_id) {
    url = g_strdup (SHOUTCAST_GET_GENRES);
  } else {
    url = g_strdup_printf (SHOUTCAST_GET_RADIOS,
                           container_id,
                           bs->skip + bs->count);
    data->genre = g_strdup (container_id);
  }

  read_url_async (url, data);

  g_free (url);
}
