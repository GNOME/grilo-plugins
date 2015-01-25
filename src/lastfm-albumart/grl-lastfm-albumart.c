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

#include <net/grl-net.h>
#include <glib/gi18n-lib.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <libxml/xpath.h>

#include "grl-lastfm-albumart.h"

/* ---------- Logging ---------- */

#define GRL_LOG_DOMAIN_DEFAULT lastfm_albumart_log_domain
GRL_LOG_DOMAIN_STATIC(lastfm_albumart_log_domain);

/* -------- Last.FM API -------- */

#define LASTFM_GET_ALBUM "https://ws.audioscrobbler.com/1.0/album/%s/%s/info.xml"

#define LASTFM_DEFAULT_IMAGE "http://cdn.last.fm/flatness/catalogue/noimage/2/default_album_medium.png"
#define LASTFM_BASE_IMAGE    "http://userserve-ak.last.fm/serve/%s/%s"

#define LASTFM_XML_COVER_MEDIUM "/album/coverart/medium"
#define LASTFM_XML_COVER_LARGE  "/album/coverart/large"
#define LASTFM_XML_COVER_SMALL  "/album/coverart/small"
#define LASTFM_XML_COVER_EXTRA  "/album/coverart/extralarge"
#define LASTFM_XML_COVER_MEGA   "/album/coverart/mega"

/* ------- Pluging Info -------- */

#define PLUGIN_ID   LASTFM_ALBUMART_PLUGIN_ID

#define SOURCE_ID   "grl-lastfm-albumart"
#define SOURCE_NAME _("Album art Provider from Last.FM")
#define SOURCE_DESC _("A plugin for getting album arts using Last.FM as backend")

static GrlNetWc *wc;

static GrlLastfmAlbumartSource *grl_lastfm_albumart_source_new (void);

static void grl_lastfm_albumart_source_finalize (GObject *object);

static void grl_lastfm_albumart_source_resolve (GrlSource *source,
                                                GrlSourceResolveSpec *rs);

static const GList *grl_lastfm_albumart_source_supported_keys (GrlSource *source);

static gboolean grl_lastfm_albumart_source_may_resolve (GrlSource *source,
                                                        GrlMedia *media,
                                                        GrlKeyID key_id,
                                                        GList **missing_keys);

static void grl_lastfm_albumart_source_cancel (GrlSource *source,
                                               guint operation_id);

gboolean grl_lastfm_albumart_source_plugin_init (GrlRegistry *registry,
                                                 GrlPlugin *plugin,
                                                 GList *configs);


/* =================== Last.FM-AlbumArt Plugin  =============== */

gboolean
grl_lastfm_albumart_source_plugin_init (GrlRegistry *registry,
                                        GrlPlugin *plugin,
                                        GList *configs)
{
  GRL_LOG_DOMAIN_INIT (lastfm_albumart_log_domain, "lastfm-albumart");

  GRL_DEBUG ("grl_lastfm_albumart_source_plugin_init");

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  GrlLastfmAlbumartSource *source = grl_lastfm_albumart_source_new ();
  grl_registry_register_source (registry,
                                plugin,
                                GRL_SOURCE (source),
                                NULL);
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_lastfm_albumart_source_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== Last.FM-AlbumArt GObject ================ */

static GrlLastfmAlbumartSource *
grl_lastfm_albumart_source_new (void)
{
  const char *tags[] = {
    "net:internet",
    NULL
  };
  GRL_DEBUG ("grl_lastfm_albumart_source_new");
  return g_object_new (GRL_LASTFM_ALBUMART_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       "source-tags", tags,
		       NULL);
}

static void
grl_lastfm_albumart_source_class_init (GrlLastfmAlbumartSourceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

  source_class->supported_keys = grl_lastfm_albumart_source_supported_keys;
  source_class->cancel = grl_lastfm_albumart_source_cancel;
  source_class->may_resolve = grl_lastfm_albumart_source_may_resolve;
  source_class->resolve = grl_lastfm_albumart_source_resolve;

  gobject_class->finalize = grl_lastfm_albumart_source_finalize;
}

static void
grl_lastfm_albumart_source_init (GrlLastfmAlbumartSource *source)
{
}

G_DEFINE_TYPE (GrlLastfmAlbumartSource,
               grl_lastfm_albumart_source,
               GRL_TYPE_SOURCE);

static void
grl_lastfm_albumart_source_finalize (GObject *object)
{
  g_clear_object (&wc);

  G_OBJECT_CLASS (grl_lastfm_albumart_source_parent_class)->finalize (object);
}

/* ======================= Utilities ==================== */

static gchar *
xml_get_image (const gchar *xmldata, const gchar *image_node)
{
  xmlDocPtr doc;
  xmlXPathContextPtr xpath_ctx;
  xmlXPathObjectPtr xpath_res;
  gchar *image = NULL;

  doc = xmlReadMemory (xmldata, xmlStrlen ((xmlChar*) xmldata), NULL, NULL,
                       XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  if (!doc) {
    return NULL;
  }

  xpath_ctx = xmlXPathNewContext (doc);
  if (!xpath_ctx) {
    xmlFreeDoc (doc);
    return NULL;
  }

  xpath_res = xmlXPathEvalExpression ((xmlChar *) image_node, xpath_ctx);
  if (!xpath_res) {
    xmlXPathFreeContext (xpath_ctx);
    xmlFreeDoc (doc);
    return NULL;
  }

  if (xpath_res->nodesetval->nodeTab) {
    image =
      (gchar *) xmlNodeListGetString (doc,
                                      xpath_res->nodesetval->nodeTab[0]->xmlChildrenNode,
                                      1);
  }
  xmlXPathFreeObject (xpath_res);
  xmlXPathFreeContext (xpath_ctx);
  xmlFreeDoc (doc);

  if (g_strcmp0 (image, LASTFM_DEFAULT_IMAGE) == 0) {
    g_clear_pointer (&image, g_free);
  }

  return image;
}

static gchar *
get_image_id (gchar **image, gint size)
{
  gint i;

  for (i = 0; i < size; i++) {
    if (image[i]) {
      return g_path_get_basename(image[i]);
    }
  }

  return NULL;
}

static void
read_done_cb (GObject *source_object,
              GAsyncResult *res,
              gpointer user_data)
{
  GrlSourceResolveSpec *rs = (GrlSourceResolveSpec *) user_data;
  GCancellable *cancellable;
  GError *error = NULL;
  GError *wc_error = NULL;
  GrlRelatedKeys *relkeys;
  gchar *content = NULL;
  gchar *image[5] = { NULL };
  gchar *image_id;
  gint i;

  /* Get rid of stored operation data */
  cancellable = grl_operation_get_data (rs->operation_id);
  g_clear_object (&cancellable);

  if (!grl_net_wc_request_finish (GRL_NET_WC (source_object),
                              res,
                              &content,
                              NULL,
                              &wc_error)) {
    if (wc_error->code == GRL_NET_WC_ERROR_CANCELLED) {
      g_propagate_error (&error, wc_error);
    } else {
      error = g_error_new (GRL_CORE_ERROR,
                           GRL_CORE_ERROR_RESOLVE_FAILED,
                           _("Failed to connect: %s"),
                           wc_error->message);
      g_error_free (wc_error);
    }
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);
    g_error_free (error);

    return;
  }

  image[0] = xml_get_image (content, LASTFM_XML_COVER_MEGA);
  image[1] = xml_get_image (content, LASTFM_XML_COVER_EXTRA);
  image[2] = xml_get_image (content, LASTFM_XML_COVER_LARGE);
  image[3] = xml_get_image (content, LASTFM_XML_COVER_MEDIUM);
  image[4] = xml_get_image (content, LASTFM_XML_COVER_SMALL);

  image_id = get_image_id (image, G_N_ELEMENTS (image));

  /* Sometimes "mega" and "extra" values are not returned; let's hardcode them */
  if (!image[0] && image_id) {
    image[0] = g_strdup_printf (LASTFM_BASE_IMAGE, "500", image_id);
  }
  if (!image[1] && image_id) {
    image[1] = g_strdup_printf (LASTFM_BASE_IMAGE, "252", image_id);
  }
  g_free (image_id);

  for (i = 0; i < G_N_ELEMENTS (image); i++) {
    if (image[i]) {
      relkeys = grl_related_keys_new_with_keys (GRL_METADATA_KEY_THUMBNAIL,
                                                image[i],
                                                NULL);
      grl_data_add_related_keys (GRL_DATA (rs->media), relkeys);
      g_free (image[i]);
    }
  }

  rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
}

static void
read_url_async (GrlSource *source,
                const gchar *url,
                GrlSourceResolveSpec *rs)
{
  GCancellable *cancellable;

  if (!wc)
    wc = grl_net_wc_new ();

  cancellable = g_cancellable_new ();
  grl_operation_set_data (rs->operation_id, cancellable);

  GRL_DEBUG ("Opening '%s'", url);
  grl_net_wc_request_async (wc, url, cancellable, read_done_cb, rs);
}

/* ================== API Implementation ================ */

static const GList *
grl_lastfm_albumart_source_supported_keys (GrlSource *source)
{
  static GList *keys = NULL;

  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_THUMBNAIL,
                                      NULL);
  }

  return keys;
}

static gboolean
grl_lastfm_albumart_source_may_resolve (GrlSource *source,
                                        GrlMedia *media,
                                        GrlKeyID key_id,
                                        GList **missing_keys)
{
  gboolean have_artist = FALSE, have_album = FALSE;

  if (key_id != GRL_METADATA_KEY_THUMBNAIL)
    return FALSE;

  if (media) {
    if (grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_ARTIST))
      have_artist = TRUE;
    if (grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_ALBUM))
      have_album = TRUE;
  }

  if (have_artist && have_album)
    return TRUE;

  if (missing_keys) {
    GList *result = NULL;
    if (!have_artist)
      result =
          g_list_append (result, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ARTIST));
    if (!have_album)
      result =
          g_list_append (result, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ALBUM));

    if (result)
      *missing_keys = result;
  }

  return FALSE;
}

static void
grl_lastfm_albumart_source_resolve (GrlSource *source,
                                    GrlSourceResolveSpec *rs)
{
  const gchar *artist = NULL;
  const gchar *album = NULL;
  gchar *esc_artist = NULL;
  gchar *esc_album = NULL;
  gchar *url = NULL;

  GRL_DEBUG (__FUNCTION__);

  GList *iter;

  /* Check that albumart is requested */
  iter = rs->keys;
  while (iter) {
    GrlKeyID key = GRLPOINTER_TO_KEYID (iter->data);
    if (key == GRL_METADATA_KEY_THUMBNAIL) {
      break;
    } else {
      iter = g_list_next (iter);
    }
  }

  if (iter == NULL) {
    GRL_DEBUG ("No supported key was requested");
    rs->callback (source, rs->operation_id, rs->media, rs->user_data, NULL);
  } else {
    artist = grl_data_get_string (GRL_DATA (rs->media),
                                  GRL_METADATA_KEY_ARTIST);

    album = grl_data_get_string (GRL_DATA (rs->media),
                                 GRL_METADATA_KEY_ALBUM);

    if (!artist || !album) {
      GRL_DEBUG ("Missing dependencies");
      rs->callback (source, rs->operation_id, rs->media, rs->user_data, NULL);
    } else {
      esc_artist = g_uri_escape_string (artist, NULL, TRUE);
      esc_album = g_uri_escape_string (album, NULL, TRUE);
      url = g_strdup_printf (LASTFM_GET_ALBUM, esc_artist, esc_album);
      read_url_async (source, url, rs);
      g_free (esc_artist);
      g_free (esc_album);
      g_free (url);
    }
  }
}

static void
grl_lastfm_albumart_source_cancel (GrlSource *source,
                                   guint operation_id)
{
  GCancellable *cancellable =
    (GCancellable *) grl_operation_get_data (operation_id);

  if (cancellable) {
    g_cancellable_cancel (cancellable);
  }
}
