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

#include "fake-metadata-source.h"
#include "../src/plugin-registry.h"

#include <glib.h>

gboolean fake_metadata_plugin_init (PluginRegistry *registry, 
				    const PluginInfo *plugin);

PLUGIN_REGISTER (fake_metadata_plugin_init, 
		 NULL, 
		 "fake-metadata-plugin-id", 
		 "Fake Metadata Plugin", 
		 "A plugin for faking metadata", 
		 "0.0.1",
		 "Igalia S.L.", 
		 "LGPL", 
		 "http://www.igalia.com");

gboolean
fake_metadata_plugin_init (PluginRegistry *registry, const PluginInfo *plugin)
{
  g_print ("fake_metadata_plugin_init\n");
  FakeMetadataSource *source = fake_metadata_source_new ();
  plugin_registry_register_source (registry, plugin, MEDIA_PLUGIN (source));
  return TRUE;
}

static void
fake_metadata_source_metadata (MetadataSource *source,
			    const gchar *object_id,
			    const KeyID *keys,
			    MetadataSourceResultCb callback,
			    gpointer user_data)
{
  g_print ("fake_metadata_source_metadata\n");

  callback (source, "metadata-id", NULL, NULL, NULL);
}

static const KeyID *
fake_metadata_source_supported_keys (MetadataSource *source)
{
  static const KeyID keys[] = { METADATA_KEY_TITLE, 
				METADATA_KEY_URL, 
				METADATA_KEY_GENRE,
				0 };
  return keys;
}

static KeyID *
fake_metadata_source_key_depends (MetadataSource *source, KeyID key_id)
{
  return NULL;
}

static void
fake_metadata_source_class_init (FakeMetadataSourceClass * klass)
{
  MetadataSourceClass *metadata_class = METADATA_SOURCE_CLASS (klass);
  metadata_class->metadata = fake_metadata_source_metadata;
  metadata_class->supported_keys = fake_metadata_source_supported_keys;
  metadata_class->key_depends = fake_metadata_source_key_depends;;
}

static void
fake_metadata_source_init (FakeMetadataSource *source)
{
}

G_DEFINE_TYPE (FakeMetadataSource, fake_metadata_source, METADATA_SOURCE_TYPE);

FakeMetadataSource *
fake_metadata_source_new (void)
{
  return g_object_new (FAKE_METADATA_SOURCE_TYPE,
		       "source-id", "FakeMetadataSourceId",
		       "source-name", "Fake Metadata Source",
		       "source-desc", "A fake metadata source",
		       NULL);
}

