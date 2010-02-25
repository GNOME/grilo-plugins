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

#include "grl-apple-trailers.h"

/* --------- Logging  -------- */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "grl-apple-trailers"

/* ---- Apple Trailers Service ---- */

#define APPLE_TRAILERS_CURRENT_SD                               \
  "http://trailers.apple.com/trailers/home/xml/current_480p.xml"

#define APPLE_TRAILERS_CURRENT_HD                               \
  "http://trailers.apple.com/trailers/home/xml/current_720p.xml"

/* --- Plugin information --- */

#define PLUGIN_ID   "grl-apple-trailers"
#define PLUGIN_NAME "Apple Movie Trailers"
#define PLUGIN_DESC "A plugin for browsing Apple Movie Trailers"

#define SOURCE_ID   "grl-apple-trailers"
#define SOURCE_NAME "Apple Movie Trailers"
#define SOURCE_DESC "A plugin for browsing Apple Movie Trailers"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

typedef struct {
  GrlMediaSourceBrowseSpec *bs;
  xmlDocPtr xml_doc;
  xmlNodePtr xml_entries;
  gboolean cancelled;
} OperationData;

static GrlAppleTrailersSource *grl_apple_trailers_source_new (void);

gboolean grl_apple_trailers_plugin_init (GrlPluginRegistry *registry,
                                         const GrlPluginInfo *plugin);

static const GList *grl_apple_trailers_source_supported_keys (GrlMetadataSource *source);

static void grl_apple_trailers_source_browse (GrlMediaSource *source,
                                              GrlMediaSourceBrowseSpec *bs);

static void grl_apple_trailers_source_cancel (GrlMediaSource *source,
                                              guint operation_id);

/* =================== Apple Trailers Plugin  =============== */

gboolean
grl_apple_trailers_plugin_init (GrlPluginRegistry *registry,
                                const GrlPluginInfo *plugin)
{
  g_debug ("apple_trailers_plugin_init\n");

  GrlAppleTrailersSource *source = grl_apple_trailers_source_new ();
  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source));
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_apple_trailers_plugin_init,
                     NULL,
                     PLUGIN_ID,
                     PLUGIN_NAME,
                     PLUGIN_DESC,
                     PACKAGE_VERSION,
                     AUTHOR,
                     LICENSE,
                     SITE);

/* ================== AppleTrailers GObject ================ */

static GrlAppleTrailersSource *
grl_apple_trailers_source_new (void)
{
  g_debug ("grl_apple_trailers_source_new");
  return g_object_new (GRL_APPLE_TRAILERS_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       NULL);
}

static void
grl_apple_trailers_source_class_init (GrlAppleTrailersSourceClass * klass)
{
  GrlMediaSourceClass *source_class = GRL_MEDIA_SOURCE_CLASS (klass);
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);
  source_class->browse = grl_apple_trailers_source_browse;
  source_class->cancel = grl_apple_trailers_source_cancel;
  metadata_class->supported_keys = grl_apple_trailers_source_supported_keys;
}

static void
grl_apple_trailers_source_init (GrlAppleTrailersSource *source)
{
}

G_DEFINE_TYPE (GrlAppleTrailersSource, grl_apple_trailers_source, GRL_TYPE_MEDIA_SOURCE);

/* ==================== Private ==================== */

static gchar *
get_node_value (xmlNodePtr node, const gchar *node_id)
{
  gchar *value;
  xmlXPathContextPtr xpath_ctx;
  xmlXPathObjectPtr xpath_res;

  xpath_ctx = xmlXPathNewContext (node->doc);
  if (!xpath_ctx) {
    return NULL;
  }

  xpath_res = xmlXPathEvalExpression ((xmlChar *) node_id, xpath_ctx);
  if (!xpath_res) {
    xmlXPathFreeContext (xpath_ctx);
    return NULL;
  }

  if (xpath_res->nodesetval->nodeTab) {
    value =
      (gchar *) xmlNodeListGetString (node->doc,
                                      xpath_res->nodesetval->nodeTab[0]->xmlChildrenNode,
                                      1);
  } else {
    value = NULL;
  }

  xmlXPathFreeObject (xpath_res);
  xmlXPathFreeContext (xpath_ctx);

  return value;
}

static gint
runtime_to_seconds (const gchar *runtime)
{
  gchar **items;
  gint seconds;

  if (!runtime) {
    return 0;
  }

  items = g_strsplit (runtime, ":", -1);
  seconds = 3600 * atoi (items[0]) + 60 * atoi (items[1]);
  g_strfreev (items);

  return seconds;
}
static GrlContentMedia *
build_media_from_movie (xmlNodePtr node)
{
  GrlContentMedia * media;
  gchar *movie_author;
  gchar *movie_date;
  gchar *movie_description;
  gchar *movie_duration;
  gchar *movie_genre;
  gchar *movie_id;
  gchar *movie_thumbnail;
  gchar *movie_title;
  gchar *movie_url;

  media = grl_content_video_new ();

  movie_id = (gchar *) xmlGetProp (node, (const xmlChar *) "id");

  /* HACK: as get_node_value applies xpath expression from root node, but we
     want to do from current node, dup the node and mark it as root node */

  xmlDocPtr xml_doc = xmlNewDoc ((const xmlChar *) "1.0");
  xmlNodePtr node_dup = xmlCopyNode (node, 1);
  xmlDocSetRootElement (xml_doc, node_dup);
  movie_author = get_node_value (node_dup, "/movieinfo/info/director");
  movie_date = get_node_value (node_dup, "/movieinfo/info/releasedate");
  movie_description = get_node_value (node_dup, "/movieinfo/info/description");
  movie_duration = get_node_value (node_dup, "/movieinfo/info/runtime");
  movie_title = get_node_value (node_dup, "/movieinfo/info/title");
  movie_genre = get_node_value (node_dup, "/movieinfo/genre/name");
  movie_thumbnail = get_node_value (node_dup, "/movieinfo/poster/location");
  movie_url = get_node_value (node_dup, "/movieinfo/preview/large");
  xmlFreeDoc (xml_doc);

  grl_content_media_set_id (media, movie_id);
  grl_content_media_set_author (media, movie_author);
  grl_content_media_set_date (media, movie_date);
  grl_content_media_set_description (media, movie_description);
  grl_content_media_set_duration (media, runtime_to_seconds (movie_duration));
  grl_content_media_set_title (media, movie_title);
  grl_content_set_string (GRL_CONTENT (media),
                          GRL_METADATA_KEY_GENRE,
                          movie_genre);
  grl_content_media_set_thumbnail (media, movie_thumbnail);
  grl_content_media_set_url (media, movie_url);

  g_free (movie_id);
  g_free (movie_author);
  g_free (movie_date);
  g_free (movie_description);
  g_free (movie_duration);
  g_free (movie_title);
  g_free (movie_genre);
  g_free (movie_thumbnail);
  g_free (movie_url);

  return media;
}

static gboolean
send_movie_info (OperationData *op_data)
{
  GrlContentMedia *media;
  gboolean last = FALSE;

  if (op_data->cancelled) {
    last = TRUE;
  } else {
    media = build_media_from_movie (op_data->xml_entries);
    last =
      !op_data->xml_entries->next  ||
      op_data->bs->count == 1;

    op_data->bs->callback (op_data->bs->source,
                           op_data->bs->browse_id,
                           media,
                           last? 0: -1,
                           op_data->bs->user_data,
                           NULL);
    op_data->xml_entries = op_data->xml_entries->next;
    op_data->bs->count--;
  }

  if (last) {
    xmlFreeDoc (op_data->xml_doc);
    g_free (op_data);
  }

  return !last;
}

static void
xml_parse_result (const gchar *str, OperationData *op_data)
{
  GError *error = NULL;
  xmlNodePtr node;

  if (op_data->cancelled) {
    g_free (op_data);
    return;
  }

  if (op_data->bs->count == 0) {
    goto finalize;
  }

  op_data->xml_doc = xmlRecoverDoc ((xmlChar *) str);
  if (!op_data->xml_doc) {
    error = g_error_new (GRL_ERROR,
                         GRL_ERROR_BROWSE_FAILED,
                         "Failed to parse response");
    goto finalize;
  }

  node = xmlDocGetRootElement (op_data->xml_doc);
  if (!node || !node->xmlChildrenNode) {
    error = g_error_new (GRL_ERROR,
                         GRL_ERROR_BROWSE_FAILED,
                         "Empty response from Apple Trailers");
    goto finalize;
  }

  node = node->xmlChildrenNode;

  /* Skip elements */
  while (node && op_data->bs->skip > 0) {
    node = node->next;
    op_data->bs->skip--;
  }

  if (!node) {
    goto finalize;
  } else {
    op_data->xml_entries = node;
    g_idle_add ((GSourceFunc) send_movie_info, op_data);
  }

  return;

 finalize:
  op_data->bs->callback (op_data->bs->source,
                         op_data->bs->browse_id,
                         NULL,
                         0,
                         op_data->bs->user_data,
                         error);

  if (op_data->xml_doc) {
    xmlFreeDoc (op_data->xml_doc);
  }

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
                         "Failed to connect Apple Trailers: '%s'",
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
grl_apple_trailers_source_supported_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_AUTHOR,
                                      GRL_METADATA_KEY_DATE,
                                      GRL_METADATA_KEY_DESCRIPTION,
                                      GRL_METADATA_KEY_DURATION,
                                      GRL_METADATA_KEY_GENRE,
                                      GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_THUMBNAIL,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_URL,
                                      NULL);
  }
  return keys;
}

static void
grl_apple_trailers_source_browse (GrlMediaSource *source,
                                  GrlMediaSourceBrowseSpec *bs)
{
  OperationData *op_data;

  g_debug ("grl_apple_trailers_source_browse");

  op_data = g_new0 (OperationData, 1);
  op_data->bs = bs;
  grl_media_source_set_operation_data (source, bs->browse_id, op_data);

  read_url_async (APPLE_TRAILERS_CURRENT_SD, op_data);
}

static void
grl_apple_trailers_source_cancel (GrlMediaSource *source, guint operation_id)
{
  OperationData *op_data;

  g_debug ("grl_apple_trailers_source_cancel");

  op_data = (OperationData *) grl_media_source_get_operation_data (source, operation_id);

  if (op_data) {
    op_data->cancelled = TRUE;
  }
}
