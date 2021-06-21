/*
 * Copyright (C) 2010, 2011 Igalia S.L.
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
#include <net/grl-net.h>
#include <glib/gi18n-lib.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "grl-shoutcast.h"

#define EXPIRE_CACHE_TIMEOUT 300

#define SHOUTCAST_DEV_KEY "dev-key"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT shoutcast_log_domain
GRL_LOG_DOMAIN_STATIC(shoutcast_log_domain);

/* ------ SHOUTcast API ------ */

#define SHOUTCAST_API_BASE_ENTRY "http://api.shoutcast.com/legacy/"
#define SHOUTCAST_YP_BASE_ENTRY  "http://yp.shoutcast.com/sbin/"

#define SHOUTCAST_GET_GENRES    SHOUTCAST_API_BASE_ENTRY "genrelist?k=%s"
#define SHOUTCAST_GET_RADIOS    SHOUTCAST_API_BASE_ENTRY "genresearch?k=%s&genre=%s&limit=%u"
#define SHOUTCAST_SEARCH_RADIOS SHOUTCAST_API_BASE_ENTRY "stationsearch?k=%s&search=%s&limit=%u"
#define SHOUTCAST_TUNE          SHOUTCAST_YP_BASE_ENTRY  "tunein-station.pls?id=%s"

/* --- Plugin information --- */

#define SOURCE_ID   "grl-shoutcast"
#define SOURCE_NAME "SHOUTcast"
#define SOURCE_DESC _("A source for browsing SHOUTcast radios")

struct _GrlShoutcastSourcePriv {
  gchar *dev_key;
  GrlNetWc *wc;
  GCancellable *cancellable;
  gchar *cached_page;
  gboolean cached_page_expired;
};

typedef struct {
  GrlMedia *media;
  GrlSource *source;
  GrlSourceResolveCb resolve_cb;
  GrlSourceResultCb result_cb;
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

static GrlShoutcastSource *grl_shoutcast_source_new (const gchar *dev_key);

gboolean grl_shoutcast_plugin_init (GrlRegistry *registry,
                                    GrlPlugin *plugin,
                                    GList *configs);

static const GList *grl_shoutcast_source_supported_keys (GrlSource *source);

static void grl_shoutcast_source_resolve (GrlSource *source,
                                          GrlSourceResolveSpec *rs);

static void grl_shoutcast_source_browse (GrlSource *source,
                                         GrlSourceBrowseSpec *bs);

static void grl_shoutcast_source_search (GrlSource *source,
                                         GrlSourceSearchSpec *ss);

static void grl_shoutcast_source_cancel (GrlSource *source,
                                         guint operation_id);

static void read_url_async (GrlShoutcastSource *source,
                            const gchar *url,
                            OperationData *op_data);

static void grl_shoutcast_source_finalize (GObject *object);

/* =================== SHOUTcast Plugin  =============== */

gboolean
grl_shoutcast_plugin_init (GrlRegistry *registry,
                           GrlPlugin *plugin,
                           GList *configs)
{
  gchar *dev_key;
  GrlConfig *config;
  gint config_count;
  GrlShoutcastSource *source;

  GRL_LOG_DOMAIN_INIT (shoutcast_log_domain, "shoutcast");

  GRL_DEBUG ("shoutcast_plugin_init");

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  if (!configs) {
    GRL_INFO ("Configuration not provided! Plugin not loaded");
    return FALSE;
  }

  config_count = g_list_length (configs);
  if (config_count > 1) {
    GRL_INFO ("Provided %d configs, but will only use one", config_count);
  }

  config = GRL_CONFIG (configs->data);
  dev_key = grl_config_get_string (config, SHOUTCAST_DEV_KEY);
  if (!dev_key) {
    GRL_INFO ("Missin API Dev Key, cannot load plugin");
    return FALSE;
  }

  source = grl_shoutcast_source_new (dev_key);
  grl_registry_register_source (registry,
                                plugin,
                                GRL_SOURCE (source),
                                NULL);

  g_free (dev_key);

  return TRUE;
}

GRL_PLUGIN_DEFINE (GRL_MAJOR,
                   GRL_MINOR,
                   SHOUTCAST_PLUGIN_ID,
                   "SHOUTcast",
                   "A plugin for browsing SHOUTcast radios",
                   "Igalia S.L.",
                   VERSION,
                   "LGPL-2.1-or-later",
                   "http://www.igalia.com",
                   grl_shoutcast_plugin_init,
                   NULL,
                   NULL);

/* ================== SHOUTcast GObject ================ */

G_DEFINE_TYPE_WITH_PRIVATE (GrlShoutcastSource, grl_shoutcast_source, GRL_TYPE_SOURCE)

static GrlShoutcastSource *
grl_shoutcast_source_new (const gchar *dev_key)
{
  GrlShoutcastSource *source;
  const char *tags[] = {
    "net:internet",
    NULL
  };

  GRL_DEBUG (__FUNCTION__);

  source =  g_object_new (GRL_SHOUTCAST_SOURCE_TYPE,
                          "source-id", SOURCE_ID,
                          "source-name", SOURCE_NAME,
                          "source-desc", SOURCE_DESC,
                          "supported-media", GRL_SUPPORTED_MEDIA_AUDIO,
                          "source-tags", tags,
                          NULL);

  source->priv->dev_key = g_strdup (dev_key);

  return source;
}

static void
grl_shoutcast_source_class_init (GrlShoutcastSourceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

  gobject_class->finalize = grl_shoutcast_source_finalize;

  source_class->cancel = grl_shoutcast_source_cancel;
  source_class->supported_keys = grl_shoutcast_source_supported_keys;
  source_class->resolve = grl_shoutcast_source_resolve;
  source_class->browse = grl_shoutcast_source_browse;
  source_class->search = grl_shoutcast_source_search;
}

static void
grl_shoutcast_source_init (GrlShoutcastSource *source)
{
  source->priv = grl_shoutcast_source_get_instance_private (source);
  source->priv->cached_page_expired = TRUE;
}

static void
grl_shoutcast_source_finalize (GObject *object)
{
  GrlShoutcastSource *self = GRL_SHOUTCAST_SOURCE (object);

  g_clear_object (&self->priv->wc);
  g_clear_pointer (&self->priv->cancellable, g_cancellable_cancel);
  g_clear_pointer (&self->priv->cached_page, g_free);
  g_clear_pointer (&self->priv->dev_key, g_free);

  G_OBJECT_CLASS (grl_shoutcast_source_parent_class)->finalize (object);
}

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
    media = grl_media_container_new ();
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
  gchar **station_genres = NULL;
  gchar *media_id;
  gchar *media_url;
  gchar *station_bitrate;
  gchar *station_genre;
  gchar *station_genre_field;
  gchar *station_id;
  gchar *station_mime;
  gchar *station_name;

  station_name = (gchar *) xmlGetProp (op_data->xml_entries,
                                       (const xmlChar *) "name");
  station_mime = (gchar *) xmlGetProp (op_data->xml_entries,
                                       (const xmlChar *) "mt");
  station_id = (gchar *) xmlGetProp (op_data->xml_entries,
                                     (const xmlChar *) "id");
  station_bitrate = (gchar *) xmlGetProp (op_data->xml_entries,
                                          (const xmlChar *) "br");
  if (op_data->media) {
    media = op_data->media;
  } else {
    media = grl_media_audio_new ();
  }

  if (op_data->genre) {
    station_genre = op_data->genre;
  } else {
    station_genre_field = (gchar *) xmlGetProp (op_data->xml_entries,
                                                (const xmlChar *) "genre");
    station_genres = g_strsplit (station_genre_field, " ", -1);
    g_free (station_genre_field);
    station_genre = station_genres[0];
  }

  media_id = g_strconcat (station_genre, "/", station_id, NULL);
  media_url = g_strdup_printf (SHOUTCAST_TUNE, station_id);

  grl_media_set_id (media, media_id);
  grl_media_set_title (media, station_name);
  grl_media_set_mime (media, station_mime);
  grl_media_set_genre (media, station_genre);
  grl_media_set_url (media, media_url);
  grl_media_set_bitrate (media, atoi (station_bitrate));

  g_free (station_name);
  g_free (station_mime);
  g_free (station_id);
  g_free (station_bitrate);
  g_free (media_id);
  g_free (media_url);
  g_clear_pointer (&station_genres, g_strfreev);

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
  } else {
    op_data->result_cb (op_data->source,
                        op_data->operation_id,
                        NULL,
                        0,
                        op_data->user_data,
                        NULL);
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
  guint id;

  if (op_data->cancelled) {
    op_data->result_cb (op_data->source,
                        op_data->operation_id,
                        NULL,
                        0,
                        op_data->user_data,
                        NULL);
    g_slice_free (OperationData, op_data);
    return;
  }

  op_data->xml_doc = xmlReadMemory (str, xmlStrlen ((xmlChar*) str), NULL, NULL,
                                    XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  if (!op_data->xml_doc) {
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 op_data->error_code,
                                 _("Failed to parse response"));
    goto finalize;
  }

  node = xmlDocGetRootElement (op_data->xml_doc);
  if  (!node) {
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 op_data->error_code,
                                 _("Empty response"));
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

      if (xpath_res && xpath_res->nodesetval->nodeTab &&
          xpath_res->nodesetval->nodeTab[0]) {
        op_data->xml_entries = xpath_res->nodesetval->nodeTab[0];
        if (stationlist_result) {
          build_media_from_station (op_data);
        } else {
          build_media_from_genre (op_data);
        }
      } else {
        error = g_error_new (GRL_CORE_ERROR,
                             op_data->error_code,
                             _("Cannot find media %s"),
                             grl_media_get_id (op_data->media));
      }
      g_clear_pointer (&xpath_res, xmlXPathFreeObject);
      xmlXPathFreeContext (xpath_ctx);
    } else {
      error = g_error_new_literal (GRL_CORE_ERROR,
                                   op_data->error_code,
                                   _("Failed to parse response"));
    }

    op_data->resolve_cb (op_data->source,
                         op_data->operation_id,
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
    id = g_idle_add ((GSourceFunc) send_stationlist_entries, op_data);
    g_source_set_name_by_id (id, "[shoutcast] send_stationlist_entries");
  } else {
    id = g_idle_add ((GSourceFunc) send_genrelist_entries, op_data);
    g_source_set_name_by_id (id, "[shoutcast] send_genrelist_entries");
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
  g_clear_pointer (&op_data->xml_doc, xmlFreeDoc);
  g_clear_pointer (&op_data->filter_entry, g_free);
  g_clear_error (&error);

  g_slice_free (OperationData, op_data);
}

static gboolean
expire_cache (gpointer user_data)
{
  GrlShoutcastSource *source = GRL_SHOUTCAST_SOURCE (user_data);

  GRL_DEBUG ("Cached page expired");
  source->priv->cached_page_expired = TRUE;
  return FALSE;
}

static void
read_done_cb (GObject *source_object,
              GAsyncResult *res,
              gpointer user_data)
{
  GError *error = NULL;
  GError *wc_error = NULL;
  OperationData *op_data = (OperationData *) user_data;
  GrlShoutcastSource *source = GRL_SHOUTCAST_SOURCE (op_data->source);
  gboolean cache;
  gchar *content = NULL;

  if (!grl_net_wc_request_finish (GRL_NET_WC (source_object),
                            res,
                            &content,
                            NULL,
                            &wc_error)) {
    error = g_error_new (GRL_CORE_ERROR,
                         op_data->error_code,
                         _("Failed to connect: %s"),
                         wc_error->message);
    op_data->result_cb (op_data->source,
                        op_data->operation_id,
                        NULL,
                        0,
                        op_data->user_data,
                        error);
    g_error_free (wc_error);
    g_error_free (error);
    g_slice_free (OperationData, op_data);

    return;
  }

  cache = op_data->cache;
  xml_parse_result (content, op_data);
  if (cache && source->priv->cached_page_expired) {
    guint id;
    GRL_DEBUG ("Caching page");
    g_free (source->priv->cached_page);
    source->priv->cached_page = g_strdup (content);
    source->priv->cached_page_expired = FALSE;
    id = g_timeout_add_seconds (EXPIRE_CACHE_TIMEOUT, expire_cache, source);
    g_source_set_name_by_id (id, "[shoutcast] expire_cache");
  }
}

static gboolean
read_cached_page (OperationData *op_data)
{
  gchar *cached_page = GRL_SHOUTCAST_SOURCE (op_data->source)->priv->cached_page;
  xml_parse_result (cached_page, op_data);
  return FALSE;
}

static void
read_url_async (GrlShoutcastSource *source,
                const gchar *url,
                OperationData *op_data)
{
  if (op_data->cache && !source->priv->cached_page_expired) {
    guint id;
    GRL_DEBUG ("Using cached page");
    id = g_idle_add ((GSourceFunc) read_cached_page, op_data);
    g_source_set_name_by_id (id, "[shoutcast] read_cached_page");
  } else {
    if (!source->priv->wc)
      source->priv->wc = grl_net_wc_new ();

    source->priv->cancellable = g_cancellable_new ();
    grl_net_wc_request_async (source->priv->wc, url,
                              source->priv->cancellable,
                              read_done_cb, op_data);
  }
}

/* ================== API Implementation ================ */

static const GList *
grl_shoutcast_source_supported_keys (GrlSource *source)
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
grl_shoutcast_source_resolve (GrlSource *source,
                              GrlSourceResolveSpec *rs)
{
  const gchar *media_id;
  gchar **id_tokens;
  gchar *url = NULL;
  OperationData *data = NULL;
  GrlShoutcastSource *shoutcast_source = GRL_SHOUTCAST_SOURCE (source);

  /* Unfortunately, shoutcast does not have an API to get information about a
     station.  Thus, steps done to obtain the Content must be repeated. For
     instance, if we have a Media with id "Pop/1321", it means that it is
     station #1321 that was obtained after browsing "Pop" category. Thus we have
     repeat the Pop browsing and get the result with station id 1321. If it
     doesn't exist (think in results obtained from a search), we do nothing */

  media_id = grl_media_get_id (rs->media);

  /* Check if we need to report about root category */
  if (!media_id) {
    grl_media_set_title (rs->media, "SHOUTcast");
  } else {
    data = g_slice_new0 (OperationData);
    data->source = source;
    data->count = 1;
    data->resolve_cb = rs->callback;
    data->user_data = rs->user_data;
    data->error_code = GRL_CORE_ERROR_RESOLVE_FAILED;
    data->media = rs->media;
    data->operation_id = rs->operation_id;

    id_tokens = g_strsplit (media_id, "/", -1);

    /* Check if Content is a media */
    if (id_tokens[1]) {
      data->filter_entry = g_strdup (id_tokens[1]);

      /* Check if result is from a previous search */
      if (id_tokens[0][0] == '?') {
        url = g_strdup_printf (SHOUTCAST_SEARCH_RADIOS,
                               shoutcast_source->priv->dev_key,
                               id_tokens[0]+1,
                               G_MAXINT);
      } else {
        url = g_strdup_printf (SHOUTCAST_GET_RADIOS,
                               shoutcast_source->priv->dev_key,
                               id_tokens[0],
                               G_MAXINT);
      }
    } else {
      data->filter_entry = g_strdup (id_tokens[0]);
      data->cache = TRUE;
      url = g_strdup_printf (SHOUTCAST_GET_GENRES,
                             shoutcast_source->priv->dev_key);
    }

    g_strfreev (id_tokens);
  }

  if (url) {
    read_url_async (shoutcast_source, url, data);
    g_free (url);
  } else {
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
  }
}

static void
grl_shoutcast_source_browse (GrlSource *source,
                             GrlSourceBrowseSpec *bs)
{
  OperationData *data;
  const gchar *container_id;
  gchar *url;
  GrlShoutcastSource *shoutcast_source = GRL_SHOUTCAST_SOURCE (source);

  GRL_DEBUG ("grl_shoutcast_source_browse");

  data = g_slice_new0 (OperationData);
  data->source = source;
  data->operation_id = bs->operation_id;
  data->result_cb = bs->callback;
  data->skip = grl_operation_options_get_skip (bs->options);
  data->count = grl_operation_options_get_count (bs->options);
  data->user_data = bs->user_data;
  data->error_code = GRL_CORE_ERROR_BROWSE_FAILED;

  container_id = grl_media_get_id (bs->container);

  /* If it's root category send list of genres; else send list of radios */
  if (!container_id) {
    data->cache = TRUE;
    url = g_strdup_printf (SHOUTCAST_GET_GENRES,
                           shoutcast_source->priv->dev_key);
  } else {
    url = g_strdup_printf (SHOUTCAST_GET_RADIOS,
                           shoutcast_source->priv->dev_key,
                           container_id,
                           data->skip + data->count);
    data->genre = g_strdup (container_id);
  }

  grl_operation_set_data (bs->operation_id, data);

  read_url_async (shoutcast_source, url, data);

  g_free (url);
}

static void
grl_shoutcast_source_search (GrlSource *source,
                             GrlSourceSearchSpec *ss)
{
  GError *error;
  OperationData *data;
  gchar *url;
  GrlShoutcastSource *shoutcast_source = GRL_SHOUTCAST_SOURCE (source);

  /* Check if there is text to search */
  if (!ss->text || ss->text[0] == '\0') {
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_SEARCH_NULL_UNSUPPORTED,
                         _("Failed to search: %s"),
                         _("non-NULL search text is required"));
    ss->callback (ss->source,
                  ss->operation_id,
                  NULL,
                  0,
                  ss->user_data,
                  error);

    g_error_free (error);
    return;
  }

  data = g_slice_new0 (OperationData);
  data->source = source;
  data->operation_id = ss->operation_id;
  data->result_cb = ss->callback;
  data->skip = grl_operation_options_get_skip (ss->options);
  data->count = grl_operation_options_get_count (ss->options);
  data->user_data = ss->user_data;
  data->error_code = GRL_CORE_ERROR_SEARCH_FAILED;

  grl_operation_set_data (ss->operation_id, data);

  url = g_strdup_printf (SHOUTCAST_SEARCH_RADIOS,
                         shoutcast_source->priv->dev_key,
                         ss->text,
                         data->skip + data->count);

  read_url_async (GRL_SHOUTCAST_SOURCE (source), url, data);

  g_free (url);
}

static void
grl_shoutcast_source_cancel (GrlSource *source, guint operation_id)
{
  OperationData *op_data;
  GrlShoutcastSourcePrivate *priv = GRL_SHOUTCAST_SOURCE(source)->priv;

  GRL_DEBUG ("grl_shoutcast_source_cancel");

  if (priv->cancellable && G_IS_CANCELLABLE (priv->cancellable)) {
    g_cancellable_cancel (priv->cancellable);
  }
  priv->cancellable = NULL;

  op_data = (OperationData *) grl_operation_get_data (operation_id);

  if (op_data) {
    op_data->cancelled = TRUE;
  }
}
