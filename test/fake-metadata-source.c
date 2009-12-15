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

#include <glib.h>

static void
fake_metadata_source_metadata (MetadataSource *source,
			    const gchar *object_id,
			    const gchar *const *keys,
			    MetadataSourceResultCb callback,
			    gpointer user_data)
{
  g_print ("fake_metadata_source_metadata\n");

  callback (source, "metadata-id", NULL, NULL, NULL);
}

static void
fake_metadata_source_class_init (FakeMetadataSourceClass * klass)
{
  MetadataSourceClass *metadata_class = METADATA_SOURCE_CLASS (klass);
  metadata_class->metadata = fake_metadata_source_metadata;
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
