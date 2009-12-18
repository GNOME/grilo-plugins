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

#include "fake-source.h"
#include "../src/plugin-registry.h"

#include <glib.h>

gboolean fake_media_plugin_init (PluginRegistry *registry, 
				 const PluginInfo *plugin);

PLUGIN_REGISTER (fake_media_plugin_init, 
		 NULL, 
		 "fake-media-plugin-id", 
		 "Fake Media Source Plugin", 
		 "A plugin for faking media", 
		 "0.0.1",
		 "Igalia S.L.", 
		 "LGPL", 
		 "http://www.igalia.com");

gboolean
fake_media_plugin_init (PluginRegistry *registry, const PluginInfo *plugin)
{
  g_print ("fake_media_plugin_init\n");
  FakeMediaSource *source = fake_media_source_new ();
  plugin_registry_register_source (registry, plugin, MEDIA_PLUGIN (source));
  return TRUE;
}

static guint
fake_media_source_browse (MediaSource *source, 
			  const gchar *container_id,
			  const KeyID *keys,
			  guint skip,
			  guint count,
			  MediaSourceResultCb callback,
			  gpointer user_data)
{
  g_print ("fake_media_source_browse\n");

  callback (source, 0, NULL, 0, user_data, NULL);

  return 0;
}

static guint
fake_media_source_search (MediaSource *source,
			  const gchar *text,
			  const gchar *filter,
			  guint skip,
			  guint count,
			  MediaSourceResultCb callback,
			  gpointer user_data)
{
  g_print ("fake_media_source_search\n");

  callback (source, 0, NULL, 0, user_data, NULL);

  return 0;
}

static void
fake_media_source_metadata (MetadataSource *source,
			    const gchar *object_id,
			    const KeyID *keys,
			    MetadataSourceResultCb callback,
			    gpointer user_data)
{
  g_print ("fake_media_source_metadata\n");

  callback (source, "media-id", NULL, NULL, NULL);
}


static KeyID *
fake_media_source_key_depends (MetadataSource *source, KeyID key_id)
{
  static KeyID artist_deps[] = { METADATA_KEY_TITLE, 0 };
  switch (key_id) {
  case METADATA_KEY_ARTIST:
    return artist_deps;
  default:
    return NULL;
  }
}

static const KeyID *
fake_media_source_supported_keys (MetadataSource *source)
{
  static const KeyID keys[] = { METADATA_KEY_TITLE, 
				METADATA_KEY_URL, 
				METADATA_KEY_ARTIST,
				0 };
  return keys;
}

static void
fake_media_source_resolve_metadata (MetadataSource *source,
				    KeyID *keys,
				    GHashTable *metadata)
{
  while (*keys) {
    switch (*keys) {
    case METADATA_KEY_ARTIST:
      g_hash_table_insert (metadata, GINT_TO_POINTER (*keys), "fake-source-artist");
      break;
    default:
      break;
    }
    keys++;
  }
}

static void
fake_media_source_class_init (FakeMediaSourceClass * klass)
{
  MediaSourceClass *source_class = MEDIA_SOURCE_CLASS (klass);
  MetadataSourceClass *metadata_class = METADATA_SOURCE_CLASS (klass);
  source_class->browse = fake_media_source_browse;
  source_class->search = fake_media_source_search;
  metadata_class->metadata = fake_media_source_metadata;
  metadata_class->supported_keys = fake_media_source_supported_keys;
  metadata_class->key_depends = fake_media_source_key_depends;
  metadata_class->resolve = fake_media_source_resolve_metadata;
}

static void
fake_media_source_init (FakeMediaSource *source)
{
}

G_DEFINE_TYPE (FakeMediaSource, fake_media_source, MEDIA_SOURCE_TYPE);

FakeMediaSource *
fake_media_source_new (void)
{
  return g_object_new (FAKE_MEDIA_SOURCE_TYPE,
		       "source-id", "FakeMediaSourceId",
		       "source-name", "Fake Media Source",
		       "source-desc", "A fake media source",
		       NULL);
}
