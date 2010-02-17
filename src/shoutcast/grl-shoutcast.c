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

#define SHOUTCAST_BASE_ENTRY "http://api.shoutcast.com/get2"

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
}
