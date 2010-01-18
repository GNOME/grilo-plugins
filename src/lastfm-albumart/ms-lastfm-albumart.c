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

#include <libgnomevfs/gnome-vfs.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <libxml/xpath.h>

#include "ms-lastfm-albumart.h"

/* ---------- Logging ---------- */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "ms-lastfm-albumart"

/* -------- Last.FM API -------- */

#define LASTFM_GET_ALBUM "http://ws.audioscrobbler.com/1.0/album/%s/%s/info.xml"
#define LASTFM_XML_COVER "/album/coverart/medium"

/* ------- Pluging Info -------- */

#define PLUGIN_ID   "ms-lastfm-albumart"
#define PLUGIN_NAME "Album art Provider from Last.FM"
#define PLUGIN_DESC "A plugin for getting album arts using Last.FM as backend"

#define SOURCE_ID   "ms-lastfm-albumart"
#define SOURCE_NAME "Album art Provider from Last.FM"
#define SOURCE_DESC "A plugin for getting album arts using Last.FM as backend"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"


static MsLastfmAlbumartSource *ms_lastfm_albumart_source_new (void);

static void ms_lastfm_albumart_source_resolve (MsMetadataSource *source,
                                               MsMetadataSourceResolveSpec *rs);

static const GList *ms_lastfm_albumart_source_supported_keys (MsMetadataSource *source);

static const GList *ms_lastfm_albumart_source_key_depends (MsMetadataSource *source,
                                                           MsKeyID key_id);

gboolean ms_lastfm_albumart_source_plugin_init (MsPluginRegistry *registry,
                                                const MsPluginInfo *plugin);


/* =================== Last.FM-AlbumArt Plugin  =============== */

gboolean
ms_lastfm_albumart_source_plugin_init (MsPluginRegistry *registry,
                                       const MsPluginInfo *plugin)
{
  g_debug ("ms_lastfm_albumart_source_plugin_init");
  MsLastfmAlbumartSource *source = ms_lastfm_albumart_source_new ();
  ms_plugin_registry_register_source (registry, plugin, MS_MEDIA_PLUGIN (source));
  return TRUE;
}

MS_PLUGIN_REGISTER (ms_lastfm_albumart_source_plugin_init,
                    NULL,
                    PLUGIN_ID,
                    PLUGIN_NAME,
                    PLUGIN_DESC,
                    PACKAGE_VERSION,
                    AUTHOR,
                    LICENSE,
                    SITE);

/* ================== Last.FM-AlbumArt GObject ================ */

static MsLastfmAlbumartSource *
ms_lastfm_albumart_source_new (void)
{
  g_debug ("ms_lastfm_albumart_source_new");
  return g_object_new (MS_LASTFM_ALBUMART_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       NULL);
}

static void
ms_lastfm_albumart_source_class_init (MsLastfmAlbumartSourceClass * klass)
{
  MsMetadataSourceClass *metadata_class = MS_METADATA_SOURCE_CLASS (klass);
  metadata_class->supported_keys = ms_lastfm_albumart_source_supported_keys;
  metadata_class->key_depends = ms_lastfm_albumart_source_key_depends;
  metadata_class->resolve = ms_lastfm_albumart_source_resolve;
}

static void
ms_lastfm_albumart_source_init (MsLastfmAlbumartSource *source)
{
}

G_DEFINE_TYPE (MsLastfmAlbumartSource, ms_lastfm_albumart_source, MS_TYPE_METADATA_SOURCE);

/* ======================= Utilities ==================== */

static gchar *
read_url (const gchar *url)
{
  gchar buffer[1025];
  GnomeVFSFileSize bytes_read;
  GnomeVFSResult r;
  GString *data;
  GnomeVFSHandle *fh;

  /* Open URL */
  g_debug ("Opening '%s'", url);
  r = gnome_vfs_open (&fh, url, GNOME_VFS_OPEN_READ);
  if (r != GNOME_VFS_OK) {
    g_warning ("Failed to open '%s' - %d", url, r);
    return NULL;
  }

  /* Read URL contents */
  g_debug ("Reading data from '%s'", url);
  data = g_string_new ("");
  do {
    gnome_vfs_read (fh, buffer, 1024, &bytes_read);
    buffer[bytes_read] = '\0';
    g_string_append (data, buffer);
  } while (bytes_read > 0);
  g_debug ("  Done reading data from url");

  gnome_vfs_close (fh);

  return g_string_free (data, FALSE);
}

static gchar *
xml_get_image (const gchar *xmldata)
{
  xmlDocPtr doc;
  xmlXPathContextPtr xpath_ctx;
  xmlXPathObjectPtr xpath_res;
  gchar *image;

  doc = xmlRecoverDoc ((xmlChar *) xmldata);
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

  image = (gchar *) xmlNodeListGetString (doc,
                                          xpath_res->nodesetval->nodeTab[0]->xmlChildrenNode,
                                          1);
  xmlXPathFreeObject (xpath_res);
  xmlXPathFreeContext (xpath_ctx);
  xmlFreeDoc (doc);

  return image;
}

/* ================== API Implementation ================ */

static const GList *
ms_lastfm_albumart_source_supported_keys (MsMetadataSource *source)
{
  static GList *keys = NULL;

  if (!keys) {
    keys = ms_metadata_key_list_new (MS_METADATA_KEY_THUMBNAIL,
				     NULL);
  }

  return keys;
}

static const GList *
ms_lastfm_albumart_source_key_depends (MsMetadataSource *source, MsKeyID key_id)
{
  static GList *deps = NULL;

  if (!deps) {
    deps = ms_metadata_key_list_new (MS_METADATA_KEY_ARTIST, MS_METADATA_KEY_ALBUM, NULL);
  }

  switch (key_id) {
  case MS_METADATA_KEY_THUMBNAIL:
    return deps;
  default:
    break;
  }

  return  NULL;
}

static void
ms_lastfm_albumart_source_resolve (MsMetadataSource *source,
                                   MsMetadataSourceResolveSpec *rs)
{
  const gchar *artist = NULL;
  const gchar *album = NULL;
  gchar *esc_artist = NULL;
  gchar *esc_album = NULL;
  gchar *url = NULL;
  gchar *xmldata = NULL;
  GError *error = NULL;
  gchar *image;

  g_debug ("ms_lastfm_albumart_source_resolve");

  GList *iter;

  /* Check that albumart is requested */
  iter = rs->keys;
  while (iter) {
    if (POINTER_TO_MSKEYID (iter->data) == MS_METADATA_KEY_THUMBNAIL) {
      break;
    } else {
      iter = g_list_next (iter);
    }
  }

  if (iter == NULL) {
    g_debug ("No supported key was requested");
  } else {
    artist = ms_content_get_string (MS_CONTENT (rs->media),
                                    MS_METADATA_KEY_ARTIST);

    album = ms_content_get_string (MS_CONTENT (rs->media),
                                   MS_METADATA_KEY_ALBUM);

    if (!artist || !album) {
      g_debug ("Missing dependencies");
      rs->callback (source, rs->media, rs->user_data, NULL);
    } else {
      esc_artist = g_uri_escape_string (artist, NULL, TRUE);
      esc_album = g_uri_escape_string (album, NULL, TRUE);
      url = g_strdup_printf (LASTFM_GET_ALBUM, esc_artist, esc_album);
      xmldata = read_url (url);
      g_free (esc_artist);
      g_free (esc_album);
      g_free (url);

      if (!xmldata) {
        error = g_error_new (MS_ERROR,
                             MS_ERROR_RESOLVE_FAILED,
                             "Failed to connect to Last.FM");
        rs->callback (source, rs->media, rs->user_data, error);
        g_error_free (error);
        return;
      }

      image = xml_get_image (xmldata);
      g_free (xmldata);

      if (image) {
        ms_content_set_string (MS_CONTENT (rs->media),
                               MS_METADATA_KEY_THUMBNAIL,
                               image);
        g_free (image);
      }
    }
  }

  rs->callback (source, rs->media, rs->user_data, NULL);
}
