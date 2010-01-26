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

#include <media-store.h>
#include <flickcurl.h>

#include "ms-flickr.h"

#define MS_FLICKR_SOURCE_GET_PRIVATE(object)    \
  (G_TYPE_INSTANCE_GET_PRIVATE((object), MS_TYPE_MEDIA_SOURCE, MsFlickrSourcePrivate))

/* --------- Logging  -------- */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "ms-flickr"

/* ----- Security tokens ---- */

#define FLICKR_KEY    "fa037bee8120a921b34f8209d715a2fa"
#define FLICKR_SECRET "9f6523b9c52e3317"
#define FLICKR_FROB   "416-357-743"
#define FLICKR_TOKEN  "72157623286932154-c90318d470e96a29"

/* --- Plugin information --- */

#define PLUGIN_ID   "ms-flickr"
#define PLUGIN_NAME "Flickr"
#define PLUGIN_DESC "A plugin for browsing and searching Flickr photos"

#define SOURCE_ID   "ms-flickr"
#define SOURCE_NAME "Flickr"
#define SOURCE_DESC "A source for browsing and searching Flickr photos"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

struct _MsFlickrSourcePrivate {
  flickcurl *fc;
};

static MsFlickrSource *ms_flickr_source_new (void);

gboolean ms_flickr_plugin_init (MsPluginRegistry *registry,
				 const MsPluginInfo *plugin);

static const GList *ms_flickr_source_supported_keys (MsMetadataSource *source);

/* =================== Flickr Plugin  =============== */

gboolean
ms_flickr_plugin_init (MsPluginRegistry *registry, const MsPluginInfo *plugin)
{
  g_debug ("flickr_plugin_init\n");

  MsFlickrSource *source = ms_flickr_source_new ();
  if (source) {
    ms_plugin_registry_register_source (registry, plugin, MS_MEDIA_PLUGIN (source));
    return TRUE;
  } else {
    return FALSE;
  }
}

MS_PLUGIN_REGISTER (ms_flickr_plugin_init,
                    NULL,
                    PLUGIN_ID,
                    PLUGIN_NAME,
                    PLUGIN_DESC,
                    PACKAGE_VERSION,
                    AUTHOR,
                    LICENSE,
                    SITE);

/* ================== Flickr GObject ================ */

static MsFlickrSource *
ms_flickr_source_new (void)
{
  MsFlickrSource *fs;
  static flickcurl *fc = NULL;

  g_debug ("ms_flickr_source_new");

  if (!fc) {
    if (flickcurl_init ()) {
      g_warning ("Unable to initialize Flickcurl");
      return NULL;
    }

    fc = flickcurl_new ();
    if (!fc) {
      g_warning ("Unable to get a Flickcurl session");
      return NULL;
    }

    flickcurl_set_api_key (fc, FLICKR_KEY);
    flickcurl_set_auth_token (fc, FLICKR_TOKEN);
    flickcurl_set_shared_secret (fc, FLICKR_SECRET);
  }

  fs = g_object_new (MS_FLICKR_SOURCE_TYPE,
                     "source-id", SOURCE_ID,
                     "source-name", SOURCE_NAME,
                     "source-desc", SOURCE_DESC,
                     NULL);
  fs->priv->fc = fc;

  return fs;
}

static void
ms_flickr_source_class_init (MsFlickrSourceClass * klass)
{
  MsMetadataSourceClass *metadata_class = MS_METADATA_SOURCE_CLASS (klass);
  metadata_class->supported_keys = ms_flickr_source_supported_keys;

  g_type_class_add_private (metadata_class, sizeof (MsFlickrSourcePrivate));
}

static void
ms_flickr_source_init (MsFlickrSource *source)
{
  source->priv = MS_FLICKR_SOURCE_GET_PRIVATE (source);
}

G_DEFINE_TYPE (MsFlickrSource, ms_flickr_source, MS_TYPE_MEDIA_SOURCE);

/* ================== API Implementation ================ */

static const GList *
ms_flickr_source_supported_keys (MsMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = ms_metadata_key_list_new (MS_METADATA_KEY_ID,
				     MS_METADATA_KEY_TITLE,
                                     MS_METADATA_KEY_URL,
                                     MS_METADATA_KEY_AUTHOR,
                                     NULL);
  }
  return keys;
}

