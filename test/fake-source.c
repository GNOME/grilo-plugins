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

#include <glib.h>

static guint
fake_media_source_browse (MediaSource *source, 
			  const gchar *container_id,
			  const gchar *const *keys,
			  guint skip,
			  guint count,
			  MediaSourceResultCb callback,
			  gpointer user_data)
{
  g_print ("fake_media_source_browse\n");

  callback (source, 0, "media-id", NULL, 0, user_data, NULL);

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

  callback (source, 0, "media-id", NULL, 0, user_data, NULL);

  return 0;
}

static void
fake_media_source_metadata (MetadataSource *source,
			    const gchar *object_id,
			    const gchar *const *keys,
			    MetadataSourceResultCb callback,
			    gpointer user_data)
{
  g_print ("fake_media_source_metadata\n");

  callback (source, "media-id", NULL, NULL, NULL);
}

static void
fake_media_source_class_init (FakeMediaSourceClass * klass)
{
  MediaSourceClass *source_class = MEDIA_SOURCE_CLASS (klass);
  MetadataSourceClass *metadata_class = METADATA_SOURCE_CLASS (klass);
  source_class->browse = fake_media_source_browse;
  source_class->search = fake_media_source_search;
  metadata_class->metadata = fake_media_source_metadata;
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
