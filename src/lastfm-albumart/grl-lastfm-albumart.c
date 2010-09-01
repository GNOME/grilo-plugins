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

#include <gio/gio.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <libxml/xpath.h>

#include "grl-lastfm-albumart.h"

/* ---------- Logging ---------- */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "grl-lastfm-albumart"

/* -------- Last.FM API -------- */

#define LASTFM_GET_ALBUM "http://ws.audioscrobbler.com/1.0/album/%s/%s/info.xml"
#define LASTFM_XML_COVER "/album/coverart/medium"

/* ------- Pluging Info -------- */

#define PLUGIN_ID   LASTFM_ALBUMART_PLUGIN_ID

#define SOURCE_ID   "grl-lastfm-albumart"
#define SOURCE_NAME "Album art Provider from Last.FM"
#define SOURCE_DESC "A plugin for getting album arts using Last.FM as backend"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"


static GrlLastfmAlbumartSource *grl_lastfm_albumart_source_new (void);

static void grl_lastfm_albumart_source_resolve (GrlMetadataSource *source,
                                                GrlMetadataSourceResolveSpec *rs);

static const GList *grl_lastfm_albumart_source_supported_keys (GrlMetadataSource *source);

static const GList *grl_lastfm_albumart_source_key_depends (GrlMetadataSource *source,
                                                            GrlKeyID key_id);

gboolean grl_lastfm_albumart_source_plugin_init (GrlPluginRegistry *registry,
                                                 const GrlPluginInfo *plugin,
                                                 GList *configs);


/* =================== Last.FM-AlbumArt Plugin  =============== */

gboolean
grl_lastfm_albumart_source_plugin_init (GrlPluginRegistry *registry,
                                        const GrlPluginInfo *plugin,
                                        GList *configs)
{
  g_debug ("grl_lastfm_albumart_source_plugin_init");
  GrlLastfmAlbumartSource *source = grl_lastfm_albumart_source_new ();
  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source));
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_lastfm_albumart_source_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== Last.FM-AlbumArt GObject ================ */

static GrlLastfmAlbumartSource *
grl_lastfm_albumart_source_new (void)
{
  g_debug ("grl_lastfm_albumart_source_new");
  return g_object_new (GRL_LASTFM_ALBUMART_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       NULL);
}

static void
grl_lastfm_albumart_source_class_init (GrlLastfmAlbumartSourceClass * klass)
{
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);
  metadata_class->supported_keys = grl_lastfm_albumart_source_supported_keys;
  metadata_class->key_depends = grl_lastfm_albumart_source_key_depends;
  metadata_class->resolve = grl_lastfm_albumart_source_resolve;
}

static void
grl_lastfm_albumart_source_init (GrlLastfmAlbumartSource *source)
{
}

G_DEFINE_TYPE (GrlLastfmAlbumartSource,
               grl_lastfm_albumart_source,
               GRL_TYPE_METADATA_SOURCE);

/* ======================= Utilities ==================== */

static gchar *
xml_get_image (const gchar *xmldata)
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

  xpath_res = xmlXPathEvalExpression ((xmlChar *) LASTFM_XML_COVER,
                                      xpath_ctx);
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

  return image;
}

static void
read_done_cb (GObject *source_object,
              GAsyncResult *res,
              gpointer user_data)
{
  GrlMetadataSourceResolveSpec *rs =
    (GrlMetadataSourceResolveSpec *) user_data;
  GError *error = NULL;
  GError *vfs_error = NULL;
  gchar *content = NULL;
  gchar *image = NULL;

  if (!g_file_load_contents_finish (G_FILE (source_object),
                                    res,
                                    &content,
                                    NULL,
                                    NULL,
                                    &vfs_error)) {
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_RESOLVE_FAILED,
                         "Failed to connect to Last.FM: '%s'",
                         vfs_error->message);
    rs->callback (rs->source, rs->media, rs->user_data, error);
    g_error_free (vfs_error);
    g_error_free (error);
    g_object_unref (source_object);
    return;
  }

  g_object_unref (source_object);
  image = xml_get_image (content);
  g_free (content);
  if (image) {
    grl_data_set_string (GRL_DATA (rs->media),
                         GRL_METADATA_KEY_THUMBNAIL,
                         image);
    g_free (image);
  }

  rs->callback (rs->source, rs->media, rs->user_data, NULL);
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
grl_lastfm_albumart_source_supported_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;

  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_THUMBNAIL,
                                      NULL);
  }

  return keys;
}

static const GList *
grl_lastfm_albumart_source_key_depends (GrlMetadataSource *source,
                                        GrlKeyID key_id)
{
  static GList *deps = NULL;

  if (!deps) {
    deps = grl_metadata_key_list_new (GRL_METADATA_KEY_ARTIST,
                                      GRL_METADATA_KEY_ALBUM,
                                      NULL);
  }

  if (key_id == GRL_METADATA_KEY_THUMBNAIL) {
    return deps;
  }

  return  NULL;
}

static void
grl_lastfm_albumart_source_resolve (GrlMetadataSource *source,
                                    GrlMetadataSourceResolveSpec *rs)
{
  const gchar *artist = NULL;
  const gchar *album = NULL;
  gchar *esc_artist = NULL;
  gchar *esc_album = NULL;
  gchar *url = NULL;

  g_debug ("grl_lastfm_albumart_source_resolve");

  GList *iter;

  /* Check that albumart is requested */
  iter = rs->keys;
  while (iter) {
    if (iter->data == GRL_METADATA_KEY_THUMBNAIL) {
      break;
    } else {
      iter = g_list_next (iter);
    }
  }

  if (iter == NULL) {
    g_debug ("No supported key was requested");
    rs->callback (source, rs->media, rs->user_data, NULL);
  } else {
    artist = grl_data_get_string (GRL_DATA (rs->media),
                                  GRL_METADATA_KEY_ARTIST);

    album = grl_data_get_string (GRL_DATA (rs->media),
                                 GRL_METADATA_KEY_ALBUM);

    if (!artist || !album) {
      g_debug ("Missing dependencies");
      rs->callback (source, rs->media, rs->user_data, NULL);
    } else {
      esc_artist = g_uri_escape_string (artist, NULL, TRUE);
      esc_album = g_uri_escape_string (album, NULL, TRUE);
      url = g_strdup_printf (LASTFM_GET_ALBUM, esc_artist, esc_album);
      read_url_async (url, rs);
      g_free (esc_artist);
      g_free (esc_album);
      g_free (url);
    }
  }
}
