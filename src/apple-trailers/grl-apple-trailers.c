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
  "http://trailers.apple.com/trailers/home/xml/current_720p.xml"

#define APPLE_TRAILERS_CURRENT_HD                               \
  "http://trailers.apple.com/trailers/home/xml/current_720p.xml"

/* --- Plugin information --- */

#define PLUGIN_ID   "grl-apple-trailers"
#define PLUGIN_NAME "Apple Movie Trailers"
#define PLUGIN_DESC "A plugin for browsing Apple Movie Trailers"

#define SOURCE_ID   "grl-apple-trailers"
#define SOURCE_NAME "Appe Movie Trailers"
#define SOURCE_DESC "A plugin for browsing Apple Movie Trailers"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

static GrlAppleTrailersSource *grl_apple_trailers_source_new (void);

gboolean grl_apple_trailers_plugin_init (GrlPluginRegistry *registry,
                                         const GrlPluginInfo *plugin);

static const GList *grl_apple_trailers_source_supported_keys (GrlMetadataSource *source);


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
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);
  metadata_class->supported_keys = grl_apple_trailers_source_supported_keys;
}

static void
grl_apple_trailers_source_init (GrlAppleTrailersSource *source)
{
}

G_DEFINE_TYPE (GrlAppleTrailersSource, grl_apple_trailers_source, GRL_TYPE_MEDIA_SOURCE);

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
