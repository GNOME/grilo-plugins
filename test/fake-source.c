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
#include "../src/ms-plugin-registry.h"

#include <glib.h>

gboolean fake_media_plugin_init (MsPluginRegistry *registry, 
				 const MsPluginInfo *plugin);

MS_PLUGIN_REGISTER (fake_media_plugin_init, 
                    NULL, 
                    "fake-media-plugin-id", 
                    "Fake Media Source Plugin", 
                    "A plugin for faking media", 
                    "0.0.1",
                    "Igalia S.L.", 
                    "LGPL", 
                    "http://www.igalia.com");

gboolean
fake_media_plugin_init (MsPluginRegistry *registry, const MsPluginInfo *plugin)
{
  g_print ("fake_media_plugin_init\n");
  FakeMediaSource *source = fake_media_source_new ();
  ms_plugin_registry_register_source (registry, plugin, MS_MEDIA_PLUGIN (source));
  return TRUE;
}

static guint
fake_media_source_browse (MsMediaSource *source, 
			  const gchar *container_id,
			  const GList *keys,
			  guint skip,
			  guint count,
			  MsMediaSourceResultCb callback,
			  gpointer user_data)
{
  g_print ("fake_media_source_browse\n");

  callback (source, 0, NULL, 0, user_data, NULL);

  return 0;
}

static guint
fake_media_source_search (MsMediaSource *source,
			  const gchar *text,
			  const GList *keys,
			  const gchar *filter,
			  guint skip,
			  guint count,
			  MsMediaSourceResultCb callback,
			  gpointer user_data)
{
  g_print ("fake_media_source_search\n");

  callback (source, 0, NULL, 0, user_data, NULL);

  return 0;
}

static void
fake_media_source_metadata (MsMetadataSource *source,
			    const gchar *object_id,
			    const GList *keys,
			    MsMetadataSourceResultCb callback,
			    gpointer user_data)
{
  g_print ("fake_media_source_metadata\n");

  callback (source, "media-id", NULL, NULL, NULL);
}


static const GList *
fake_media_source_key_depends (MsMetadataSource *source, MsKeyID key_id)
{
  static GList *artist_deps = NULL;
  if (!artist_deps) {
    artist_deps = ms_metadata_key_list_new (MS_METADATA_KEY_TITLE, NULL);
  }

  switch (key_id) {
  case MS_METADATA_KEY_ARTIST:
    return artist_deps;
  default:
    return NULL;
  }
}

static const GList *
fake_media_source_supported_keys (MsMetadataSource *source)
{

  static GList *keys = NULL;
  if (!keys) {
    keys = ms_metadata_key_list_new (MS_METADATA_KEY_TITLE,
                                     MS_METADATA_KEY_URL,
                                     MS_METADATA_KEY_ARTIST,
                                     NULL);
  }
  return keys;
}

static void
fake_media_source_resolve_metadata (MsMetadataSource *source,
				    const GList *keys,
				    MsContent *media,
				    MsMetadataSourceResolveCb callback,
				    gpointer user_data)
{
  MsKeyID key;
  while (keys) {
    key = GPOINTER_TO_INT (keys->data);
    switch (key) {
    case MS_METADATA_KEY_ARTIST:
      ms_content_set_string (media, key, "fake-source-artist");
      break;
    default:
      break;
    }
    keys = g_list_next (keys);
  }

  callback (source, media, user_data, NULL);
}

static void
fake_media_source_class_init (FakeMediaSourceClass * klass)
{
  MsMediaSourceClass *source_class = MS_MEDIA_SOURCE_CLASS (klass);
  MsMetadataSourceClass *metadata_class = MS_METADATA_SOURCE_CLASS (klass);
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

G_DEFINE_TYPE (FakeMediaSource, fake_media_source, MS_TYPE_MEDIA_SOURCE);

FakeMediaSource *
fake_media_source_new (void)
{
  return g_object_new (FAKE_MEDIA_SOURCE_TYPE,
		       "source-id", "FakeMediaSourceId",
		       "source-name", "Fake Media Source",
		       "source-desc", "A fake media source",
		       NULL);
}
