/*
 * Copyright (C) 2012 W. Michael Petullo.
 *
 * Contact: W. Michael Petullo <mike@flyn.org>
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

#include <grilo.h>
#include <libdmapsharing/dmap.h>

#include "grl-dpap-compat.h"
#include "grl-common.h"
#include "grl-dpap-record.h"

struct GrlDpapRecordPrivate {
  char *location;
  gint largefilesize;
  gint creationdate;
  gint rating;
  char *filename;
  void *thumbnail; /* GByteArray or GArray, depending on libdmapsharing ver. */
  char *aspectratio;
  gint height;
  gint width;
  char *format;
  char *comments;
};

enum {
  PROP_0,
  PROP_LOCATION,
  PROP_LARGE_FILESIZE,
  PROP_CREATION_DATE,
  PROP_RATING,
  PROP_FILENAME,
  PROP_ASPECT_RATIO,
  PROP_PIXEL_HEIGHT,
  PROP_PIXEL_WIDTH,
  PROP_FORMAT,
  PROP_COMMENTS,
  PROP_THUMBNAIL
};

static void grl_dpap_record_dmap_iface_init (gpointer iface, gpointer data);
static void grl_dpap_record_dpap_iface_init (gpointer iface, gpointer data);

G_DEFINE_TYPE_WITH_CODE (GrlDpapRecord, grl_dpap_record, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GrlDpapRecord)
                         G_IMPLEMENT_INTERFACE (DMAP_TYPE_IMAGE_RECORD, grl_dpap_record_dpap_iface_init)
                         G_IMPLEMENT_INTERFACE (DMAP_TYPE_RECORD, grl_dpap_record_dmap_iface_init))

static void
grl_dpap_record_set_property (GObject *object,
                              guint prop_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
  GrlDpapRecord *record = SIMPLE_DPAP_RECORD (object);

  switch (prop_id) {
  case PROP_LOCATION:
    g_free (record->priv->location);
    record->priv->location = g_value_dup_string (value);
    break;
  case PROP_LARGE_FILESIZE:
    record->priv->largefilesize = g_value_get_int (value);
    break;
  case PROP_CREATION_DATE:
    record->priv->creationdate = g_value_get_int (value);
    break;
  case PROP_RATING:
    record->priv->rating = g_value_get_int (value);
    break;
  case PROP_FILENAME:
    g_free (record->priv->filename);
    record->priv->filename = g_value_dup_string (value);
    break;
  case PROP_ASPECT_RATIO:
    g_free (record->priv->aspectratio);
    record->priv->aspectratio = g_value_dup_string (value);
    break;
  case PROP_PIXEL_HEIGHT:
    record->priv->height = g_value_get_int (value);
    break;
  case PROP_PIXEL_WIDTH:
    record->priv->width = g_value_get_int (value);
    break;
  case PROP_FORMAT:
    g_free (record->priv->format);
    record->priv->format = g_value_dup_string (value);
    break;
  case PROP_COMMENTS:
    g_free (record->priv->comments);
    record->priv->comments = g_value_dup_string (value);
    break;
  case PROP_THUMBNAIL:
    record->priv->thumbnail = get_thumbnail (record->priv->thumbnail, value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
grl_dpap_record_get_property (GObject *object,
                              guint prop_id,
                              GValue *value,
                              GParamSpec *pspec)
{
  GrlDpapRecord *record = SIMPLE_DPAP_RECORD (object);

  switch (prop_id) {
  case PROP_LOCATION:
    g_value_set_static_string (value, record->priv->location);
    break;
  case PROP_LARGE_FILESIZE:
    g_value_set_int (value, record->priv->largefilesize);
    break;
  case PROP_CREATION_DATE:
    g_value_set_int (value, record->priv->creationdate);
    break;
  case PROP_RATING:
    g_value_set_int (value, record->priv->rating);
    break;
  case PROP_FILENAME:
    g_value_set_static_string (value, record->priv->filename);
    break;
  case PROP_ASPECT_RATIO:
    g_value_set_static_string (value, record->priv->aspectratio);
    break;
  case PROP_PIXEL_HEIGHT:
    g_value_set_int (value, record->priv->height);
    break;
  case PROP_PIXEL_WIDTH:
    g_value_set_int (value, record->priv->width);
    break;
  case PROP_FORMAT:
    g_value_set_static_string (value, record->priv->format);
    break;
  case PROP_COMMENTS:
    g_value_set_static_string (value, record->priv->comments);
    break;
  case PROP_THUMBNAIL:
    set_thumbnail (value, record->priv->thumbnail);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

GrlDpapRecord *
grl_dpap_record_new (void)
{
  return SIMPLE_DPAP_RECORD (g_object_new (TYPE_SIMPLE_DPAP_RECORD, NULL));
}

GInputStream *
grl_dpap_record_read (DmapImageRecord *record, GError **error)
{
  GFile *file;
  GInputStream *stream;

  file = g_file_new_for_uri (SIMPLE_DPAP_RECORD (record)->priv->location);
  stream = G_INPUT_STREAM (g_file_read (file, NULL, error));

  g_object_unref (file);

  return stream;
}

static void
grl_dpap_record_init (GrlDpapRecord *record)
{
  record->priv = grl_dpap_record_get_instance_private (record);
}

static void grl_dpap_record_finalize (GObject *object);

static void
grl_dpap_record_class_init (GrlDpapRecordClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = grl_dpap_record_set_property;
  gobject_class->get_property = grl_dpap_record_get_property;
  gobject_class->finalize     = grl_dpap_record_finalize;

  g_object_class_override_property (gobject_class, PROP_LOCATION, "location");
  g_object_class_override_property (gobject_class, PROP_LARGE_FILESIZE, "large-filesize");
  g_object_class_override_property (gobject_class, PROP_CREATION_DATE, "creation-date");
  g_object_class_override_property (gobject_class, PROP_RATING, "rating");
  g_object_class_override_property (gobject_class, PROP_FILENAME, "filename");
  g_object_class_override_property (gobject_class, PROP_ASPECT_RATIO, "aspect-ratio");
  g_object_class_override_property (gobject_class, PROP_PIXEL_HEIGHT, "pixel-height");
  g_object_class_override_property (gobject_class, PROP_PIXEL_WIDTH, "pixel-width");
  g_object_class_override_property (gobject_class, PROP_FORMAT, "format");
  g_object_class_override_property (gobject_class, PROP_COMMENTS, "comments");
  g_object_class_override_property (gobject_class, PROP_THUMBNAIL, "thumbnail");
}

static void
grl_dpap_record_dpap_iface_init (gpointer iface, gpointer data)
{
  DmapImageRecordInterface *dpap_record = iface;

  g_assert (G_TYPE_FROM_INTERFACE (dpap_record) == DMAP_TYPE_IMAGE_RECORD);

  dpap_record->read = grl_dpap_record_read;
}

static void
grl_dpap_record_dmap_iface_init (gpointer iface, gpointer data)
{
  DmapRecordInterface *dmap_record = iface;

  g_assert (G_TYPE_FROM_INTERFACE (dmap_record) == DMAP_TYPE_RECORD);
}

static void
grl_dpap_record_finalize (GObject *object)
{
  GrlDpapRecord *record = SIMPLE_DPAP_RECORD (object);

  g_free (record->priv->location);
  g_free (record->priv->filename);
  g_free (record->priv->aspectratio);
  g_free (record->priv->format);
  g_free (record->priv->comments);

  if (record->priv->thumbnail)
    unref_thumbnail (record->priv->thumbnail);

  G_OBJECT_CLASS (grl_dpap_record_parent_class)->finalize (object);
}
