/*
 * Copyright (C) 2010, 2011 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
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
#include <libxml/xpath.h>
#include <glib/gi18n-lib.h>

#include "grl-apple-trailers.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT apple_trailers_log_domain
GRL_LOG_DOMAIN_STATIC(apple_trailers_log_domain);

/* ---- Apple Trailers Service ---- */

#define APPLE_TRAILERS_CURRENT_SD                               \
  "http://trailers.apple.com/trailers/home/xml/current_480p.xml"

#define APPLE_TRAILERS_CURRENT_HD                               \
  "http://trailers.apple.com/trailers/home/xml/current_720p.xml"

/* --- Plugin information --- */

#define PLUGIN_ID   APPLE_TRAILERS_PLUGIN_ID

#define SOURCE_ID   "grl-apple-trailers"
#define SOURCE_NAME _("Apple Movie Trailers")
#define SOURCE_DESC _("A plugin for browsing Apple Movie Trailers")

typedef struct {
  GrlSourceBrowseSpec *bs;
  GCancellable *cancellable;
  xmlDocPtr xml_doc;
  xmlNodePtr xml_entries;
} OperationData;

enum {
  PROP_0,
  PROP_HD,
  PROP_LARGE_POSTER,
};

struct _GrlAppleTrailersSourcePriv {
  GrlNetWc *wc;
  gboolean hd;
  gboolean large_poster;
};

#define GRL_APPLE_TRAILERS_SOURCE_GET_PRIVATE(object)		\
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                        \
                               GRL_APPLE_TRAILERS_SOURCE_TYPE,  \
                               GrlAppleTrailersSourcePriv))

static GrlAppleTrailersSource *grl_apple_trailers_source_new (gboolean hd,
                                                              gboolean xlarge);

gboolean grl_apple_trailers_plugin_init (GrlRegistry *registry,
                                         GrlPlugin *plugin,
                                         GList *configs);

static const GList *grl_apple_trailers_source_supported_keys (GrlSource *source);

static void grl_apple_trailers_source_browse (GrlSource *source,
                                              GrlSourceBrowseSpec *bs);

static void grl_apple_trailers_source_cancel (GrlSource *source,
                                              guint operation_id);

/* =================== Apple Trailers Plugin  =============== */

gboolean
grl_apple_trailers_plugin_init (GrlRegistry *registry,
                                GrlPlugin *plugin,
                                GList *configs)
{
  GrlAppleTrailersSource *source;
  gboolean hd = FALSE;
  gboolean xlarge = FALSE;

  GRL_LOG_DOMAIN_INIT (apple_trailers_log_domain, "apple-trailers");

  GRL_DEBUG ("apple_trailers_plugin_init");

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  for (; configs; configs = g_list_next (configs)) {
    GrlConfig *config;
    gchar *definition, *poster_size;

    config = GRL_CONFIG (configs->data);
    definition = grl_config_get_string (config, "definition");
    if (definition) {
      if (*definition != '\0') {
        if (g_str_equal (definition, "hd")) {
          hd = TRUE;
        }
      }
      g_free (definition);
    }

    poster_size = grl_config_get_string (config, "poster-size");
    if (poster_size) {
      if (*poster_size != '\0') {
        if (g_str_equal (poster_size, "xlarge")) {
          xlarge = TRUE;
        }
      }
      g_free (poster_size);
    }
  }

  source = grl_apple_trailers_source_new (hd, xlarge);
  grl_registry_register_source (registry,
                                plugin,
                                GRL_SOURCE (source),
                                NULL);
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_apple_trailers_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== AppleTrailers GObject ================ */

static GrlAppleTrailersSource *
grl_apple_trailers_source_new (gboolean high_definition,
                               gboolean xlarge)
{
  GrlAppleTrailersSource *source;

  GRL_DEBUG ("grl_apple_trailers_source_new%s%s",
             high_definition ? " (HD)" : "",
             xlarge ? " (X-large poster)" : "");
  source = g_object_new (GRL_APPLE_TRAILERS_SOURCE_TYPE,
                         "source-id", SOURCE_ID,
                         "source-name", SOURCE_NAME,
                         "source-desc", SOURCE_DESC,
                         "supported-media", GRL_MEDIA_TYPE_VIDEO,
                         "high-definition", high_definition,
			 "large-poster", xlarge,
                         NULL);

  return source;
}

G_DEFINE_TYPE (GrlAppleTrailersSource, grl_apple_trailers_source, GRL_TYPE_SOURCE);

static void
grl_apple_trailers_source_finalize (GObject *object)
{
  GrlAppleTrailersSource *self;

  self = GRL_APPLE_TRAILERS_SOURCE (object);

  if (self->priv->wc)
    g_object_unref (self->priv->wc);

  G_OBJECT_CLASS (grl_apple_trailers_source_parent_class)->finalize (object);
}

static void
grl_apple_trailers_source_set_property (GObject *object,
                                        guint propid,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
  GrlAppleTrailersSource *self;
  self = GRL_APPLE_TRAILERS_SOURCE (object);

  switch (propid) {
    case PROP_HD:
      self->priv->hd = g_value_get_boolean (value);
      break;
    case PROP_LARGE_POSTER:
      self->priv->large_poster = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, propid, pspec);
    }
}

static void
grl_apple_trailers_source_class_init (GrlAppleTrailersSourceClass * klass)
{
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);
  GObjectClass *g_class = G_OBJECT_CLASS (klass);

  g_class->finalize = grl_apple_trailers_source_finalize;
  g_class->set_property = grl_apple_trailers_source_set_property;

  source_class->cancel = grl_apple_trailers_source_cancel;
  source_class->supported_keys = grl_apple_trailers_source_supported_keys;
  source_class->browse = grl_apple_trailers_source_browse;

  g_object_class_install_property (g_class,
                                   PROP_HD,
                                   g_param_spec_boolean ("high-definition",
                                                         "hd",
                                                         "Hi/Low definition videos",
                                                         TRUE,
                                                         G_PARAM_WRITABLE
                                                         | G_PARAM_CONSTRUCT_ONLY
                                                         | G_PARAM_STATIC_NAME));

  g_object_class_install_property (g_class,
                                   PROP_LARGE_POSTER,
                                   g_param_spec_boolean ("large-poster",
                                                         "xlarge",
                                                         "Pick large poster",
                                                         TRUE,
                                                         G_PARAM_WRITABLE
                                                         | G_PARAM_CONSTRUCT_ONLY
                                                         | G_PARAM_STATIC_NAME));

  g_type_class_add_private (klass, sizeof (GrlAppleTrailersSourcePriv));
}

static void
grl_apple_trailers_source_init (GrlAppleTrailersSource *source)
{
  source->priv = GRL_APPLE_TRAILERS_SOURCE_GET_PRIVATE (source);
  source->priv->hd = TRUE;
}

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

  seconds = 0;
  items = g_strsplit (runtime, ":", -1);
  if (items && items[0] && items[1])
    seconds = 3600 * atoi (items[0]) + 60 * atoi (items[1]);
  g_strfreev (items);

  return seconds;
}
static GrlMedia *
build_media_from_movie (xmlNodePtr node, gboolean xlarge)
{
  GrlMedia * media;
  gchar *movie_author;
  gchar *movie_date;
  gchar *movie_description;
  gchar *movie_duration;
  gchar *movie_genre;
  gchar *movie_id;
  gchar *movie_thumbnail;
  gchar *movie_title;
  gchar *movie_url;
  gchar *movie_rating;
  gchar *movie_studio;
  gchar *movie_copyright;
  GDateTime *date;

  media = grl_media_video_new ();

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
  if (xlarge)
    movie_thumbnail = get_node_value (node_dup, "/movieinfo/poster/xlarge");
  else
    movie_thumbnail = get_node_value (node_dup, "/movieinfo/poster/location");
  movie_url = get_node_value (node_dup, "/movieinfo/preview/large");
  movie_rating = get_node_value (node_dup, "/movieinfo/info/rating");
  movie_studio = get_node_value (node_dup, "/movieinfo/info/studio");
  movie_copyright = get_node_value (node_dup, "/movieinfo/info/copyright");
  xmlFreeDoc (xml_doc);

  grl_media_set_id (media, movie_id);
  grl_media_set_author (media, movie_author);
  if (movie_date)
    date = grl_date_time_from_iso8601 (movie_date);
  else
    date = NULL;
  if (date) {
    grl_media_set_publication_date (media, date);
    g_date_time_unref (date);
  }
  grl_media_set_description (media, movie_description);
  grl_media_set_duration (media, runtime_to_seconds (movie_duration));
  grl_media_set_title (media, movie_title);
  grl_data_set_string (GRL_DATA (media),
                       GRL_METADATA_KEY_GENRE,
                       movie_genre);
  grl_media_set_thumbnail (media, movie_thumbnail);
  grl_media_set_url (media, movie_url);
  grl_media_set_certificate (media, movie_rating);
  grl_media_set_studio (media, movie_studio);

  grl_media_set_mime (media, "video/mp4");

  /* FIXME: Translation */
  grl_media_set_license (media, movie_copyright);

  g_free (movie_id);
  g_free (movie_author);
  g_free (movie_date);
  g_free (movie_description);
  g_free (movie_duration);
  g_free (movie_title);
  g_free (movie_genre);
  g_free (movie_thumbnail);
  g_free (movie_url);
  g_free (movie_rating);
  g_free (movie_studio);
  g_free (movie_copyright);

  return media;
}

static gboolean
send_movie_info (OperationData *op_data)
{
  GrlMedia *media;
  gboolean last = FALSE;

  if (g_cancellable_is_cancelled (op_data->cancellable)) {
    op_data->bs->callback (op_data->bs->source,
                           op_data->bs->operation_id,
                           NULL,
                           0,
                           op_data->bs->user_data,
                           NULL);
    last = TRUE;
  } else {
    GrlAppleTrailersSource *source =
      GRL_APPLE_TRAILERS_SOURCE (op_data->bs->source);
    gint count = grl_operation_options_get_count (op_data->bs->options);

    media = build_media_from_movie (op_data->xml_entries,
                                    source->priv->large_poster);
    last =
      !op_data->xml_entries->next || count == 1;

    op_data->bs->callback (op_data->bs->source,
                           op_data->bs->operation_id,
                           media,
                           last? 0: -1,
                           op_data->bs->user_data,
                           NULL);
    op_data->xml_entries = op_data->xml_entries->next;
    if (!last)
      grl_operation_options_set_count (op_data->bs->options, count - 1);
  }

  if (last) {
    xmlFreeDoc (op_data->xml_doc);
    g_object_unref (op_data->cancellable);
    g_slice_free (OperationData, op_data);
  }

  return !last;
}

static void
xml_parse_result (const gchar *str, OperationData *op_data)
{
  GError *error = NULL;
  xmlNodePtr node;
  guint skip = grl_operation_options_get_skip (op_data->bs->options);

  if (g_cancellable_is_cancelled (op_data->cancellable) ||
      grl_operation_options_get_count (op_data->bs->options) == 0) {
    goto finalize;
  }

  op_data->xml_doc = xmlReadMemory (str, xmlStrlen ((xmlChar*) str), NULL, NULL,
                                    XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  if (!op_data->xml_doc) {
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_BROWSE_FAILED,
                                 _("Failed to parse response"));
    goto finalize;
  }

  node = xmlDocGetRootElement (op_data->xml_doc);
  if (!node || !node->xmlChildrenNode) {
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_BROWSE_FAILED,
                                 _("Empty response"));
    goto finalize;
  }

  node = node->xmlChildrenNode;

  /* Skip elements */
  while (node && skip > 0) {
    node = node->next;
    skip--;
  }
  grl_operation_options_set_skip (op_data->bs->options, skip);

  if (!node) {
    goto finalize;
  } else {
    op_data->xml_entries = node;
    g_idle_add ((GSourceFunc) send_movie_info, op_data);
  }

  return;

 finalize:
  op_data->bs->callback (op_data->bs->source,
                         op_data->bs->operation_id,
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

  g_object_unref (op_data->cancellable);
  g_slice_free (OperationData, op_data);
}

static void
read_done_cb (GObject *source_object,
              GAsyncResult *res,
              gpointer user_data)
{
  GError *error = NULL;
  GError *wc_error = NULL;
  OperationData *op_data = (OperationData *) user_data;
  gchar *content = NULL;

  if (!grl_net_wc_request_finish (GRL_NET_WC (source_object),
                              res,
                              &content,
                              NULL,
                              &wc_error)) {
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_BROWSE_FAILED,
                         _("Failed to connect: %s"),
                         wc_error->message);
    op_data->bs->callback (op_data->bs->source,
                           op_data->bs->operation_id,
                           NULL,
                           0,
                           op_data->bs->user_data,
                           error);
    g_error_free (wc_error);
    g_error_free (error);
    g_object_unref (op_data->cancellable);
    g_slice_free (OperationData, op_data);

    return;
  }

  xml_parse_result (content, op_data);
}

static void
read_url_async (GrlAppleTrailersSource *source,
                const gchar *url,
                OperationData *op_data)
{
  if (!source->priv->wc)
    source->priv->wc = grl_net_wc_new ();

  GRL_DEBUG ("Opening '%s'", url);
  grl_net_wc_request_async (source->priv->wc,
                        url,
                        op_data->cancellable,
                        read_done_cb,
                        op_data);
}

/* ================== API Implementation ================ */

static const GList *
grl_apple_trailers_source_supported_keys (GrlSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_AUTHOR,
                                      GRL_METADATA_KEY_PUBLICATION_DATE,
                                      GRL_METADATA_KEY_DESCRIPTION,
                                      GRL_METADATA_KEY_DURATION,
                                      GRL_METADATA_KEY_GENRE,
                                      GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_THUMBNAIL,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_URL,
                                      GRL_METADATA_KEY_CERTIFICATE,
                                      GRL_METADATA_KEY_STUDIO,
                                      GRL_METADATA_KEY_LICENSE,
                                      NULL);
  }
  return keys;
}

static void
grl_apple_trailers_source_browse (GrlSource *source,
                                  GrlSourceBrowseSpec *bs)
{
  GrlAppleTrailersSource *at_source = GRL_APPLE_TRAILERS_SOURCE (source);
  OperationData *op_data;

  GRL_DEBUG (__FUNCTION__);

  op_data = g_slice_new0 (OperationData);
  op_data->bs = bs;
  op_data->cancellable = g_cancellable_new();
  grl_operation_set_data (bs->operation_id, op_data);

  if (at_source->priv->hd) {
    read_url_async (at_source, APPLE_TRAILERS_CURRENT_HD, op_data);
  } else {
    read_url_async (at_source, APPLE_TRAILERS_CURRENT_SD, op_data);
  }
}

static void
grl_apple_trailers_source_cancel (GrlSource *source, guint operation_id)
{
  OperationData *op_data;

  GRL_DEBUG ("grl_apple_trailers_source_cancel");

  op_data = (OperationData *) grl_operation_get_data (operation_id);
  if (op_data) {
    g_cancellable_cancel (op_data->cancellable);
  }
}
