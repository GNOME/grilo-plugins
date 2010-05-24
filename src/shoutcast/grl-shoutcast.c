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
#include <libxml/xpath.h>
#include <string.h>
#include <stdlib.h>

#include "grl-shoutcast.h"

#define EXPIRE_CACHE_TIMEOUT 300

/* --------- Logging  -------- */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "grl-shoutcast"

/* ------ SHOUTcast API ------ */

#define SHOUTCAST_BASE_ENTRY "http://yp.shoutcast.com"

#define SHOUTCAST_GET_GENRES    SHOUTCAST_BASE_ENTRY "/sbin/newxml.phtml"
#define SHOUTCAST_GET_RADIOS    SHOUTCAST_GET_GENRES "?genre=%s&limit=%d"
#define SHOUTCAST_SEARCH_RADIOS SHOUTCAST_GET_GENRES "?search=%s&limit=%d"
#define SHOUTCAST_TUNE          SHOUTCAST_BASE_ENTRY "/sbin/tunein-station.pls?id=%s"

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
  GrlMedia *media;
  GrlMediaSource *source;
  GrlMediaSourceMetadataCb metadata_cb;
  GrlMediaSourceResultCb result_cb;
  gboolean cancelled;
  gboolean cache;
  gchar *filter_entry;
  gchar *genre;
  gint error_code;
  gint operation_id;
  gint to_send;
  gpointer user_data;
  guint count;
  guint skip;
  xmlDocPtr xml_doc;
  xmlNodePtr xml_entries;
} OperationData;

static gchar *cached_page = NULL;
static gboolean cached_page_expired = TRUE;

static GrlShoutcastSource *grl_shoutcast_source_new (void);

gboolean grl_shoutcast_plugin_init (GrlPluginRegistry *registry,
                                    const GrlPluginInfo *plugin,
                                    GList *configs);

static const GList *grl_shoutcast_source_supported_keys (GrlMetadataSource *source);

static void grl_shoutcast_source_metadata (GrlMediaSource *source,
                                           GrlMediaSourceMetadataSpec *ms);

static void grl_shoutcast_source_browse (GrlMediaSource *source,
                                         GrlMediaSourceBrowseSpec *bs);

static void grl_shoutcast_source_search (GrlMediaSource *source,
                                         GrlMediaSourceSearchSpec *ss);

static void grl_shoutcast_source_cancel (GrlMediaSource *source,
                                         guint operation_id);

static void read_url_async (const gchar *url, OperationData *op_data);

/* =================== SHOUTcast Plugin  =============== */

gboolean
grl_shoutcast_plugin_init (GrlPluginRegistry *registry,
                           const GrlPluginInfo *plugin,
                           GList *configs)
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
  source_class->metadata = grl_shoutcast_source_metadata;
  source_class->browse = grl_shoutcast_source_browse;
  source_class->search = grl_shoutcast_source_search;
  source_class->cancel = grl_shoutcast_source_cancel;
  metadata_class->supported_keys = grl_shoutcast_source_supported_keys;
}

static void
grl_shoutcast_source_init (GrlShoutcastSource *source)
{
}

G_DEFINE_TYPE (GrlShoutcastSource, grl_shoutcast_source, GRL_TYPE_MEDIA_SOURCE);

/* ======================= Private ==================== */

static gint
xml_count_nodes (xmlNodePtr node)
{
  gint count = 0;

  while (node) {
    count++;
    node = node->next;
  }

  return count;
}

static GrlMedia *
build_media_from_genre (OperationData *op_data)
{
  GrlMedia *media;
  gchar *genre_name;

  if (op_data->media) {
    media = op_data->media;
  } else {
    media = grl_media_box_new ();
  }

  genre_name = (gchar *) xmlGetProp (op_data->xml_entries,
                                     (const xmlChar *) "name");

  grl_media_set_id (media, genre_name);
  grl_media_set_title (media, genre_name);
  grl_data_set_string (GRL_DATA (media),
                       GRL_METADATA_KEY_GENRE,
                       genre_name);
  g_free (genre_name);

  return media;
}

static GrlMedia *
build_media_from_station (OperationData *op_data)
{
  GrlMedia *media;
  gchar *media_id;
  gchar *media_url;
  gchar *station_bitrate;
  gchar *station_genre;
  gchar *station_id;
  gchar *station_mime;
  gchar *station_name;

  station_name = (gchar *) xmlGetProp (op_data->xml_entries,
                                       (const xmlChar *) "name");
  station_mime = (gchar *) xmlGetProp (op_data->xml_entries,
                                       (const xmlChar *) "mt");
  station_id = (gchar *) xmlGetProp (op_data->xml_entries,
                                     (const xmlChar *) "id");
  station_genre = (gchar *) xmlGetProp (op_data->xml_entries,
                                        (const xmlChar *) "genre");
  station_bitrate = (gchar *) xmlGetProp (op_data->xml_entries,
                                          (const xmlChar *) "br");
  media_id = g_strconcat (op_data->genre, "/", station_id, NULL);
  media_url = g_strdup_printf (SHOUTCAST_TUNE, station_id);

  if (op_data->media) {
    media = op_data->media;
  } else {
    media = grl_media_audio_new ();
  }

  grl_media_set_id (media, media_id);
  grl_media_set_title (media, station_name);
  grl_media_set_mime (media, station_mime);
  grl_media_audio_set_genre (GRL_MEDIA_AUDIO (media), station_genre);
  grl_media_set_url (media, media_url);
  grl_media_audio_set_bitrate (GRL_MEDIA_AUDIO (media),
                               atoi (station_bitrate));

  g_free (station_name);
  g_free (station_mime);
  g_free (station_id);
  g_free (station_genre);
  g_free (station_bitrate);
  g_free (media_id);
  g_free (media_url);

  return media;
}

static gboolean
send_media (OperationData *op_data, GrlMedia *media)
{
  if (!op_data->cancelled) {
    op_data->result_cb (op_data->source,
                        op_data->operation_id,
                        media,
                        --op_data->to_send,
                        op_data->user_data,
                        NULL);

    op_data->xml_entries = op_data->xml_entries->next;
  }

  if (op_data->to_send == 0 || op_data->cancelled) {
    xmlFreeDoc (op_data->xml_doc);
    g_slice_free (OperationData, op_data);
    return FALSE;
  } else {
    return TRUE;
  }
}

static gboolean
send_genrelist_entries (OperationData *op_data)
{
  return send_media (op_data,
                     build_media_from_genre (op_data));
}

static gboolean
send_stationlist_entries (OperationData *op_data)
{
  return send_media (op_data,
                     build_media_from_station (op_data));
}

static void
xml_parse_result (const gchar *str, OperationData *op_data)
{
  GError *error = NULL;
  gboolean stationlist_result;
  gchar *xpath_expression;
  xmlNodePtr node;
  xmlXPathContextPtr xpath_ctx;
  xmlXPathObjectPtr xpath_res;

  if (op_data->cancelled) {
    g_slice_free (OperationData, op_data);
    return;
  }

  op_data->xml_doc = xmlReadMemory (str, xmlStrlen ((xmlChar*) str), NULL, NULL,
                                    XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  if (!op_data->xml_doc) {
    error = g_error_new (GRL_ERROR,
                         op_data->error_code,
                         "Failed to parse SHOUTcast's response");
    goto finalize;
  }

  node = xmlDocGetRootElement (op_data->xml_doc);
  if  (!node) {
    error = g_error_new (GRL_ERROR,
                         op_data->error_code,
                         "Empty response from SHOUTcast");
    goto finalize;
  }

  stationlist_result = (xmlStrcmp (node->name,
                                   (const xmlChar *) "stationlist") == 0);

  op_data->xml_entries = node->xmlChildrenNode;

  /* Check if we are interesting only in updating a media (that is, a metadata()
     operation) or just browsing/searching */
  if (op_data->media) {

    /* Search for node */
    xpath_ctx = xmlXPathNewContext (op_data->xml_doc);
    if (xpath_ctx) {
      if (stationlist_result) {
        xpath_expression = g_strdup_printf ("//station[@id = \"%s\"]",
                                            op_data->filter_entry);
      } else {
        xpath_expression = g_strdup_printf ("//genre[@name = \"%s\"]",
                                            op_data->filter_entry);
      }
      xpath_res = xmlXPathEvalExpression ((xmlChar *) xpath_expression,
                                          xpath_ctx);
      g_free (xpath_expression);

      if (xpath_res && xpath_res->nodesetval->nodeTab[0]) {
        op_data->xml_entries = xpath_res->nodesetval->nodeTab[0];
        if (stationlist_result) {
          build_media_from_station (op_data);
        } else {
          build_media_from_genre (op_data);
        }
      } else {
        error = g_error_new (GRL_ERROR,
                             op_data->error_code,
                             "Can not find media '%s'",
                             grl_media_get_id (op_data->media));
      }
      if (xpath_res) {
        xmlXPathFreeObject (xpath_res);
      }
      xmlXPathFreeContext (xpath_ctx);
    } else {
      error = g_error_new (GRL_ERROR,
                           op_data->error_code,
                           "Can not build xpath context");
    }

    op_data->metadata_cb (
                          op_data->source,
                          op_data->media,
                          op_data->user_data,
                          error);
    goto free_resources;
  }

  if (stationlist_result) {
    /* First node is "tunein"; skip it */
    op_data->xml_entries = op_data->xml_entries->next;
  }

  /* Skip elements */
  while (op_data->xml_entries && op_data->skip > 0) {
    op_data->xml_entries = op_data->xml_entries->next;
    op_data->skip--;
  }

  /* Check if there are elements to send*/
  if (!op_data->xml_entries || op_data->count == 0) {
    goto finalize;
  }

  /* Compute how many items are to be sent */
  op_data->to_send = xml_count_nodes (op_data->xml_entries);
  if (op_data->to_send > op_data->count) {
    op_data->to_send = op_data->count;
  }

  if (stationlist_result) {
    g_idle_add ((GSourceFunc) send_stationlist_entries, op_data);
  } else {
    g_idle_add ((GSourceFunc) send_genrelist_entries, op_data);
  }

  return;

 finalize:
  op_data->result_cb (op_data->source,
                      op_data->operation_id,
                      NULL,
                      0,
                      op_data->user_data,
                      error);

 free_resources:
  if (op_data->xml_doc) {
    xmlFreeDoc (op_data->xml_doc);
  }

  if (op_data->filter_entry) {
    g_free (op_data->filter_entry);
  }

  if (error) {
    g_error_free (error);
  }
  g_slice_free (OperationData, op_data);
}

static gboolean
expire_cache (gpointer user_data)
{
  g_debug ("Cached page expired");
  cached_page_expired = TRUE;
  return FALSE;
}

static void
read_done_cb (GObject *source_object,
              GAsyncResult *res,
              gpointer user_data)
{
  GError *error = NULL;
  GError *vfs_error = NULL;
  OperationData *op_data = (OperationData *) user_data;
  gboolean cache;
  gchar *content = NULL;

  if (!g_file_load_contents_finish (G_FILE (source_object),
                                    res,
                                    &content,
                                    NULL,
                                    NULL,
                                    &vfs_error)) {
    error = g_error_new (GRL_ERROR,
                         op_data->error_code,
                         "Failed to connect SHOUTcast: '%s'",
                         vfs_error->message);
    op_data->result_cb (op_data->source,
                        op_data->operation_id,
                        NULL,
                        0,
                        op_data->user_data,
                        error);
    g_error_free (error);
    g_slice_free (OperationData, op_data);

    return;
  }

  cache = op_data->cache;
  xml_parse_result (content, op_data);
  if (cache && cached_page_expired) {
    g_debug ("Caching page");
    g_free (cached_page);
    cached_page = content;
    cached_page_expired = FALSE;
    g_timeout_add_seconds (EXPIRE_CACHE_TIMEOUT, expire_cache, NULL);
  } else {
    g_free (content);
  }
}

static gboolean
read_cached_page (OperationData *op_data)
{
  xml_parse_result (cached_page, op_data);
  return FALSE;
}

static void
read_url_async (const gchar *url, OperationData *op_data)
{
  GVfs *vfs;
  GFile *uri;

  if (op_data->cache && !cached_page_expired) {
    g_debug ("Using cached page");
    g_idle_add ((GSourceFunc) read_cached_page, op_data);
  } else {
    vfs = g_vfs_get_default ();
    g_debug ("Opening '%s'", url);
    uri = g_vfs_get_file_for_uri (vfs, url);
    g_file_load_contents_async (uri, NULL, read_done_cb, op_data);
  }
}

/* ================== API Implementation ================ */

static const GList *
grl_shoutcast_source_supported_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_BITRATE,
                                      GRL_METADATA_KEY_GENRE,
                                      GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_MIME,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_URL,
                                      NULL);
  }
  return keys;
}

static void
grl_shoutcast_source_metadata (GrlMediaSource *source,
                               GrlMediaSourceMetadataSpec *ms)
{
  const gchar *media_id;
  gchar **id_tokens;
  gchar *url = NULL;
  OperationData *data = NULL;

  /* Unfortunately, shoutcast does not have an API to get information about a
     station.  Thus, steps done to obtain the Content must be repeated. For
     instance, if we have a Media with id "Pop/1321", it means that it is
     station #1321 that was obtained after browsing "Pop" category. Thus we have
     repeat the Pop browsing and get the result with station id 1321. If it
     doesn't exist (think in results obtained from a search), we do nothing */

  media_id = grl_media_get_id (ms->media);

  /* Check if we need to report about root category */
  if (!media_id) {
    grl_media_set_title (ms->media, "SHOUTcast");
  } else {
    data = g_slice_new0 (OperationData);
    data->source = source;
    data->count = 1;
    data->metadata_cb = ms->callback;
    data->user_data = ms->user_data;
    data->error_code = GRL_ERROR_METADATA_FAILED;
    data->media = ms->media;

    id_tokens = g_strsplit (media_id, "/", -1);

    /* Check if Content is a media */
    if (id_tokens[1]) {
      data->filter_entry = g_strdup (id_tokens[1]);

      /* Check if result is from a previous search */
      if (id_tokens[0][0] == '?') {
        url = g_strdup_printf (SHOUTCAST_SEARCH_RADIOS,
                               id_tokens[0]+1,
                               G_MAXINT);
      } else {
        url = g_strdup_printf (SHOUTCAST_GET_RADIOS,
                               id_tokens[0],
                               G_MAXINT);
      }
    } else {
      data->filter_entry = g_strdup (id_tokens[0]);
      data->cache = TRUE;
      url = g_strdup (SHOUTCAST_GET_GENRES);
    }

    g_strfreev (id_tokens);
  }

  if (url) {
    read_url_async (url, data);
    g_free (url);
  } else {
    ms->callback (ms->source, ms->media, ms->user_data, NULL);
  }
}

static void
grl_shoutcast_source_browse (GrlMediaSource *source,
                             GrlMediaSourceBrowseSpec *bs)
{
  OperationData *data;
  const gchar *container_id;
  gchar *url;

  g_debug ("grl_shoutcast_source_browse");

  data = g_slice_new0 (OperationData);
  data->source = source;
  data->operation_id = bs->browse_id;
  data->result_cb = bs->callback;
  data->skip = bs->skip;
  data->count = bs->count;
  data->user_data = bs->user_data;
  data->error_code = GRL_ERROR_BROWSE_FAILED;

  container_id = grl_media_get_id (bs->container);

  /* If it's root category send list of genres; else send list of radios */
  if (!container_id) {
    data->cache = TRUE;
    url = g_strdup (SHOUTCAST_GET_GENRES);
  } else {
    url = g_strdup_printf (SHOUTCAST_GET_RADIOS,
                           container_id,
                           bs->skip + bs->count);
    data->genre = g_strdup (container_id);
  }

  grl_media_source_set_operation_data (source, bs->browse_id, data);

  read_url_async (url, data);

  g_free (url);
}

static void
grl_shoutcast_source_search (GrlMediaSource *source,
                             GrlMediaSourceSearchSpec *ss)
{
  GError *error;
  OperationData *data;
  gchar *url;

  /* Check if there is text to search */
  if (!ss->text || ss->text[0] == '\0') {
    error = g_error_new (GRL_ERROR,
                         GRL_ERROR_SEARCH_FAILED,
                         "Search text not specified");
    ss->callback (ss->source,
                  ss->search_id,
                  NULL,
                  0,
                  ss->user_data,
                  error);

    g_error_free (error);
    return;
  }

  data = g_slice_new0 (OperationData);
  data->source = source;
  data->operation_id = ss->search_id;
  data->result_cb = ss->callback;
  data->skip = ss->skip;
  data->count = ss->count;
  data->user_data = ss->user_data;
  data->error_code = GRL_ERROR_SEARCH_FAILED;
  data->genre = g_strconcat ("?", ss->text, NULL);

  grl_media_source_set_operation_data (source, ss->search_id, data);

  url = g_strdup_printf (SHOUTCAST_SEARCH_RADIOS,
                         ss->text,
                         ss->skip + ss->count);

  read_url_async (url, data);

  g_free (url);
}

static void
grl_shoutcast_source_cancel (GrlMediaSource *source, guint operation_id)
{
  OperationData *op_data;

  g_debug ("grl_shoutcast_source_cancel");

  op_data = (OperationData *) grl_media_source_get_operation_data (source, operation_id);

  if (op_data) {
    op_data->cancelled = TRUE;
  }
}
