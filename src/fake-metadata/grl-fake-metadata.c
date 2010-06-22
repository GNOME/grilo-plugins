/*
 * Copyright (C) 2010 Igalia S.L.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
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

#include "grl-fake-metadata.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "grl-fake-metadata"

#define PLUGIN_ID   FAKEMETADATA_PLUGIN_ID

#define SOURCE_ID   "grl-fake-metadata"
#define SOURCE_NAME "Fake Metadata Provider"
#define SOURCE_DESC "A source for faking metadata resolution"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"


static GrlFakeMetadataSource *grl_fake_metadata_source_new (void);

static void grl_fake_metadata_source_resolve (GrlMetadataSource *source,
                                              GrlMetadataSourceResolveSpec *rs);

static void grl_fake_metadata_source_set_metadata (GrlMetadataSource *source,
						   GrlMetadataSourceSetMetadataSpec *sms);

static const GList *grl_fake_metadata_source_supported_keys (GrlMetadataSource *source);

static const GList *grl_fake_metadata_source_key_depends (GrlMetadataSource *source,
                                                          GrlKeyID key_id);

static const GList *grl_fake_metadata_source_writable_keys (GrlMetadataSource *source);

gboolean grl_fake_metadata_source_plugin_init (GrlPluginRegistry *registry,
                                               const GrlPluginInfo *plugin,
                                               GList *configs);


/* =================== GrlFakeMetadata Plugin  =============== */

gboolean
grl_fake_metadata_source_plugin_init (GrlPluginRegistry *registry,
                                      const GrlPluginInfo *plugin,
                                      GList *configs)
{
  g_debug ("grl_fake_metadata_source_plugin_init");
  GrlFakeMetadataSource *source = grl_fake_metadata_source_new ();
  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source));
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_fake_metadata_source_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== GrlFakeMetadata GObject ================ */

static GrlFakeMetadataSource *
grl_fake_metadata_source_new (void)
{
  g_debug ("grl_fake_metadata_source_new");
  return g_object_new (GRL_FAKE_METADATA_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       NULL);
}

static void
grl_fake_metadata_source_class_init (GrlFakeMetadataSourceClass * klass)
{
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);
  metadata_class->supported_keys = grl_fake_metadata_source_supported_keys;
  metadata_class->key_depends = grl_fake_metadata_source_key_depends;
  metadata_class->resolve = grl_fake_metadata_source_resolve;
  metadata_class->set_metadata = grl_fake_metadata_source_set_metadata;
  metadata_class->writable_keys = grl_fake_metadata_source_writable_keys;
}

static void
grl_fake_metadata_source_init (GrlFakeMetadataSource *source)
{
}

G_DEFINE_TYPE (GrlFakeMetadataSource,
               grl_fake_metadata_source,
               GRL_TYPE_METADATA_SOURCE);

/* ======================= Utilities ==================== */

static void
fill_metadata (GrlMedia *media, GrlKeyID key_id)
{
  if (key_id == GRL_METADATA_KEY_AUTHOR) {
    grl_media_set_author (media, "fake author");
  } else if (key_id == GRL_METADATA_KEY_ARTIST) {
    grl_data_set_string (GRL_DATA (media),
                         GRL_METADATA_KEY_ARTIST, "fake artist");
  } else if (key_id == GRL_METADATA_KEY_ALBUM) {
    grl_data_set_string (GRL_DATA (media),
                         GRL_METADATA_KEY_ALBUM, "fake album");
  } else if (key_id == GRL_METADATA_KEY_GENRE) {
    grl_data_set_string (GRL_DATA (media),
                         GRL_METADATA_KEY_GENRE, "fake genre");
  } else if (key_id == GRL_METADATA_KEY_DESCRIPTION) {
    grl_media_set_description (media, "fake description");
  } else if (key_id == GRL_METADATA_KEY_DURATION) {
    grl_media_set_duration (media, 99);
  } else if (key_id == GRL_METADATA_KEY_DATE) {
    grl_data_set_string (GRL_DATA (media),
                         GRL_METADATA_KEY_DATE, "01/01/1970");
  } else if (key_id == GRL_METADATA_KEY_THUMBNAIL) {
    grl_media_set_thumbnail (media,
                                  "http://fake.thumbnail.com/fake-image.jpg");
  }
}


/* ================== API Implementation ================ */

static const GList *
grl_fake_metadata_source_supported_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_AUTHOR,
                                      GRL_METADATA_KEY_ARTIST,
                                      GRL_METADATA_KEY_ALBUM,
                                      GRL_METADATA_KEY_GENRE,
                                      GRL_METADATA_KEY_DESCRIPTION,
                                      GRL_METADATA_KEY_DURATION,
                                      GRL_METADATA_KEY_DATE,
                                      GRL_METADATA_KEY_THUMBNAIL,
                                      NULL);
  }
  return keys;
}

static const GList *
grl_fake_metadata_source_key_depends (GrlMetadataSource *source,
                                      GrlKeyID key_id)
{
  static GList *deps = NULL;
  if (!deps) {
    deps = grl_metadata_key_list_new (GRL_METADATA_KEY_TITLE, NULL);
  }

  if (key_id == GRL_METADATA_KEY_AUTHOR ||
      key_id == GRL_METADATA_KEY_ARTIST ||
      key_id == GRL_METADATA_KEY_ALBUM ||
      key_id == GRL_METADATA_KEY_GENRE ||
      key_id == GRL_METADATA_KEY_DESCRIPTION ||
      key_id == GRL_METADATA_KEY_DURATION ||
      key_id == GRL_METADATA_KEY_DATE ||
      key_id == GRL_METADATA_KEY_THUMBNAIL) {
    return deps;
  }

  return  NULL;
}

static const GList *
grl_fake_metadata_source_writable_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ALBUM,
                                      GRL_METADATA_KEY_ARTIST,
                                      GRL_METADATA_KEY_GENRE,
                                      NULL);
  }
  return keys;
}

static void
grl_fake_metadata_source_resolve (GrlMetadataSource *source,
                                  GrlMetadataSourceResolveSpec *rs)
{
  g_debug ("grl_fake_metadata_source_resolve");

  GList *iter;

  iter = rs->keys;
  while (iter) {
    fill_metadata (GRL_MEDIA (rs->media), iter->data);
    iter = g_list_next (iter);
  }

  rs->callback (source, rs->media, rs->user_data, NULL);
}

static void
grl_fake_metadata_source_set_metadata (GrlMetadataSource *source,
				       GrlMetadataSourceSetMetadataSpec *sms)
{
  g_debug ("grl_fake_metadata_source_set_metadata");
  g_debug ("  Faking set metadata for %d keys!", g_list_length (sms->keys));
  sms->callback (sms->source, sms->media, NULL, sms->user_data, NULL);
}
