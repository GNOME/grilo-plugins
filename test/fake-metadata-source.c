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
#include "../src/ms-plugin-registry.h"
#include "../src/content/ms-content-media.h"

#include <glib.h>

gboolean fake_metadata_plugin_init (MsPluginRegistry *registry, 
				    const MsPluginInfo *plugin);

MS_PLUGIN_REGISTER (fake_metadata_plugin_init, 
                    NULL, 
                    "fake-metadata-plugin-id", 
                    "Fake Metadata Plugin", 
                    "A plugin for faking metadata", 
                    "0.0.1",
                    "Igalia S.L.", 
                    "LGPL", 
                    "http://www.igalia.com");

gboolean
fake_metadata_plugin_init (MsPluginRegistry *registry, const MsPluginInfo *plugin)
{
  g_print ("fake_metadata_plugin_init\n");
  FakeMetadataSource *source = fake_metadata_source_new ();
  ms_plugin_registry_register_source (registry, plugin, MS_MEDIA_PLUGIN (source));
  return TRUE;
}

static void
fake_metadata_source_metadata (MsMetadataSource *source,
			    const gchar *object_id,
			    const GList *keys,
			    MsMetadataSourceResultCb callback,
			    gpointer user_data)
{
  g_print ("fake_metadata_source_metadata\n");

  callback (source, NULL, NULL, NULL);
}

static const GList *
fake_metadata_source_supported_keys (MsMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = ms_metadata_key_list_new (MS_METADATA_KEY_TITLE, 
				  MS_METADATA_KEY_URL, 
				  MS_METADATA_KEY_ALBUM,
				  MS_METADATA_KEY_ARTIST,
				  MS_METADATA_KEY_GENRE,
				  MS_METADATA_KEY_THUMBNAIL,
				  MS_METADATA_KEY_LYRICS,
				  NULL);
  }
  return keys;
}

static const GList *
fake_metadata_source_key_depends (MsMetadataSource *source, MsKeyID key_id)
{
  static GList *lyrics_deps = NULL;
  if (!lyrics_deps) {
    lyrics_deps = ms_metadata_key_list_new (MS_METADATA_KEY_SITE, NULL);
  }
  static GList *deps_title = NULL;
  if (!deps_title) {
    deps_title = ms_metadata_key_list_new (MS_METADATA_KEY_TITLE, NULL);
  }

  switch (key_id) {
  case MS_METADATA_KEY_ALBUM:
  case MS_METADATA_KEY_ARTIST:
  case MS_METADATA_KEY_GENRE:
    return deps_title;
  case MS_METADATA_KEY_THUMBNAIL: 
    /* Depends on artist,album, which in the end depends on title */
    return deps_title;
  case MS_METADATA_KEY_LYRICS:
    /* Example of key that will not be resolved */
    return lyrics_deps;
  default:
    return NULL;
  }
}

static void
fake_metadata_source_resolve_metadata (MsMetadataSource *source,
				       const GList *keys,
                                       MsContent *media,
				       MsMetadataSourceResolveCb callback,
				       gpointer user_data)
{
  MsKeyID key;
  while (keys) {
    key = GPOINTER_TO_INT (keys->data);
    switch (key) {
    case MS_METADATA_KEY_ALBUM:
      ms_content_set_string (media, key, "fake-album");
      break;
    case MS_METADATA_KEY_ARTIST:
      ms_content_set_string (media, key, "fake-artist");
      break;
    case MS_METADATA_KEY_GENRE:
      ms_content_set_string (media, key, "fake-genre");
      break;
    case MS_METADATA_KEY_THUMBNAIL:
      ms_content_media_set_thumbnail (MS_CONTENT_MEDIA (media),
                                   "http://fake-thumbnail.com/fake-thumbnail.jpg");
      break;
    default:
      break;
    }
    keys = g_list_next (keys);
  }

  callback (source, media, user_data, NULL);
}

static void
fake_metadata_source_class_init (FakeMetadataSourceClass * klass)
{
  MsMetadataSourceClass *metadata_class = MS_METADATA_SOURCE_CLASS (klass);
  metadata_class->metadata = fake_metadata_source_metadata;
  metadata_class->supported_keys = fake_metadata_source_supported_keys;
  metadata_class->key_depends = fake_metadata_source_key_depends;
  metadata_class->resolve = fake_metadata_source_resolve_metadata;
}

static void
fake_metadata_source_init (FakeMetadataSource *source)
{
}

G_DEFINE_TYPE (FakeMetadataSource, fake_metadata_source, MS_TYPE_METADATA_SOURCE);

FakeMetadataSource *
fake_metadata_source_new (void)
{
  return g_object_new (FAKE_METADATA_SOURCE_TYPE,
		       "source-id", "FakeMetadataSourceId",
		       "source-name", "Fake Metadata Source",
		       "source-desc", "A fake metadata source",
		       NULL);
}
