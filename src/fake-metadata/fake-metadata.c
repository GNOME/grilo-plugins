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

#include <media-store.h>

#include "fake-metadata.h"

#define PLUGIN_ID   "ms-fake-metadata"
#define PLUGIN_NAME "Fake Metadata Provider"
#define PLUGIN_DESC "A plugin for faking metadata resolution"

#define SOURCE_ID   "ms-fake-metadata"
#define SOURCE_NAME "Fake Metadata Provider"
#define SOURCE_DESC "A source for faking metadata resolution"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"


static FakeMetadataSource *fake_metadata_source_new (void);

static void fake_metadata_source_metadata (MsMetadataSource *source,
					   MsMetadataSourceMetadataSpec *ms);

static void fake_metadata_source_resolve (MsMetadataSource *source,
					  MsMetadataSourceResolveSpec *rs);

static const GList *fake_metadata_source_supported_keys (MsMetadataSource *source);

static const GList *fake_metadata_source_key_depends (MsMetadataSource *source,
						      MsKeyID key_id);

gboolean fake_metadata_source_plugin_init (MsPluginRegistry *registry,
					   const MsPluginInfo *plugin);


/* =================== Youtube Plugin  =============== */

gboolean
fake_metadata_source_plugin_init (MsPluginRegistry *registry,
				  const MsPluginInfo *plugin)
{
  g_debug ("fake_metadata_source_plugin_init\n");
  FakeMetadataSource *source = fake_metadata_source_new ();
  ms_plugin_registry_register_source (registry, plugin, MS_MEDIA_PLUGIN (source));
  return TRUE;
}

MS_PLUGIN_REGISTER (fake_metadata_source_plugin_init, 
                    NULL, 
                    PLUGIN_ID,
                    PLUGIN_NAME, 
                    PLUGIN_DESC, 
                    PACKAGE_VERSION,
                    AUTHOR, 
                    LICENSE, 
                    SITE);

/* ================== Youtube GObject ================ */

static FakeMetadataSource *
fake_metadata_source_new (void)
{
  g_debug ("fake_metadata_source_new");
  return g_object_new (FAKE_METADATA_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       NULL);
}

static void
fake_metadata_source_class_init (FakeMetadataSourceClass * klass)
{
  MsMetadataSourceClass *metadata_class = MS_METADATA_SOURCE_CLASS (klass);
  metadata_class->supported_keys = fake_metadata_source_supported_keys;
  metadata_class->key_depends = fake_metadata_source_key_depends;
  metadata_class->metadata = fake_metadata_source_metadata;
  metadata_class->resolve = fake_metadata_source_resolve;
}

static void
fake_metadata_source_init (FakeMetadataSource *source)
{
}

G_DEFINE_TYPE (FakeMetadataSource, fake_metadata_source, MS_TYPE_METADATA_SOURCE);

/* ======================= Utilities ==================== */

static void
fill_metadata (MsContentMedia *media, MsKeyID key_id)
{
  switch (key_id) {
  case MS_METADATA_KEY_AUTHOR:
    ms_content_media_set_author (media, "fake author");
    break;
  case MS_METADATA_KEY_ARTIST:
    ms_content_set_string (MS_CONTENT (media),
			   MS_METADATA_KEY_ARTIST, "fake artist");
    break;
  case MS_METADATA_KEY_ALBUM:
    ms_content_set_string (MS_CONTENT (media),
			   MS_METADATA_KEY_ALBUM, "fake album");
    break;
  case MS_METADATA_KEY_GENRE:
    ms_content_set_string (MS_CONTENT (media),
			   MS_METADATA_KEY_GENRE, "fake genre");
    break;
  case MS_METADATA_KEY_DESCRIPTION:
    ms_content_media_set_description (media, "fake description");
    break;
  case MS_METADATA_KEY_DURATION:
    ms_content_media_set_description (media, "99");
    break;
  case MS_METADATA_KEY_DATE:
    ms_content_set_string (MS_CONTENT (media),
			   MS_METADATA_KEY_DATE, "01/01/1970");
    break;
  case MS_METADATA_KEY_THUMBNAIL:
    ms_content_media_set_thumbnail (media, 
				    "http://fake.thumbnail.com/fake-image.jpg");
    break;
  default:
    break;
  }
}


/* ================== API Implementation ================ */

static const GList *
fake_metadata_source_supported_keys (MsMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = ms_metadata_key_list_new (MS_METADATA_KEY_AUTHOR,
				     MS_METADATA_KEY_ARTIST,
				     MS_METADATA_KEY_ALBUM,
				     MS_METADATA_KEY_GENRE,
				     MS_METADATA_KEY_DESCRIPTION,
				     MS_METADATA_KEY_DURATION,
				     MS_METADATA_KEY_DATE,
				     MS_METADATA_KEY_THUMBNAIL,
				     NULL);
  }
  return keys;
}

static const GList *
fake_metadata_source_key_depends (MsMetadataSource *source, MsKeyID key_id)
{
  static GList *deps = NULL;
  if (!deps) {
    deps = ms_metadata_key_list_new (MS_METADATA_KEY_TITLE, NULL);
  }

  switch (key_id) {
  case MS_METADATA_KEY_AUTHOR:
  case MS_METADATA_KEY_ARTIST:
  case MS_METADATA_KEY_ALBUM:
  case MS_METADATA_KEY_GENRE:
  case MS_METADATA_KEY_DESCRIPTION:
  case MS_METADATA_KEY_DURATION:
  case MS_METADATA_KEY_DATE:
  case MS_METADATA_KEY_THUMBNAIL:
    return deps;
  default:
    break;
  }

  return  NULL;
}

static void
fake_metadata_source_metadata (MsMetadataSource *source,
			       MsMetadataSourceMetadataSpec *ms)
{
  g_debug ("fake_metadata_source_metadata");

  GList *iter;
  MsContentMedia *media;

  media = ms_content_media_new ();

  iter = ms->keys;
  while (iter) {
    MsKeyID key_id = GPOINTER_TO_INT (iter->data);
    fill_metadata (media, key_id);
    iter = g_list_next (iter);
  }

  ms->callback (source, MS_CONTENT (media), ms->user_data, NULL);
}

static void
fake_metadata_source_resolve (MsMetadataSource *source,
			      MsMetadataSourceResolveSpec *rs)
{
  g_debug ("fake_metadata_source_resolve");

  GList *iter;

  iter = rs->keys;
  while (iter) {
    MsKeyID key_id = GPOINTER_TO_INT (iter->data);
    fill_metadata (MS_CONTENT_MEDIA (rs->media), key_id);
    iter = g_list_next (iter);
  }

  rs->callback (source, rs->media, rs->user_data, NULL);
}
