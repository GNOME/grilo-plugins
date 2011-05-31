/*
 * Copyright (C) 2011 Intel Corporation.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
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

#ifndef _GRL_TRACKER_METADATA_H_
#define _GRL_TRACKER_METADATA_H_

#include <grilo.h>

#define GRL_TRACKER_METADATA_TYPE               \
  (grl_tracker_metadata_get_type ())

#define GRL_TRACKER_METADATA(obj)                               \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                           \
                               GRL_TRACKER_METADATA_TYPE,       \
                               GrlTrackerMetadata))

#define GRL_IS_TRACKER_METADATA(obj)                       \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                      \
                               GRL_TRACKER_METADATA_TYPE))

#define GRL_TRACKER_METADATA_CLASS(klass)               \
  (G_TYPE_CHECK_CLASS_CAST((klass),                     \
                           GRL_TRACKER_METADATA_TYPE,   \
                           GrlTrackerMetadataClass))

#define GRL_IS_TRACKER_METADATA_CLASS(klass)            \
  (G_TYPE_CHECK_CLASS_TYPE((klass),                     \
                           GRL_TRACKER_METADATA_TYPE))

#define GRL_TRACKER_METADATA_GET_CLASS(obj)                     \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                            \
                              GRL_TRACKER_METADATA_TYPE,        \
                              GrlTrackerMetadataClass))

/* ---- MetadataSource information ---- */

#define GRL_TRACKER_METADATA_ID   "grl-tracker-metadata"
#define GRL_TRACKER_METADATA_NAME "TrackerMetadata"
#define GRL_TRACKER_METADATA_DESC               \
  "A plugin for searching metadata"             \
  "using Tracker"

#define GRL_TRACKER_AUTHOR  "Igalia S.L."
#define GRL_TRACKER_LICENSE "LGPL"
#define GRL_TRACKER_SITE    "http://www.igalia.com"

/**/

typedef struct _GrlTrackerMetadata GrlTrackerMetadata;
typedef struct _GrlTrackerMetadataPriv GrlTrackerMetadataPriv;

struct _GrlTrackerMetadata {

  GrlSource parent;

  /*< private >*/
  GrlTrackerMetadataPriv *priv;

};

typedef struct _GrlTrackerMetadataClass GrlTrackerMetadataClass;

struct _GrlTrackerMetadataClass {

  GrlSourceClass parent_class;

};

GType grl_tracker_metadata_get_type (void);

/**/

void grl_tracker_metadata_init_requests (void);

void grl_tracker_metadata_source_init (void);

#endif /* _GRL_TRACKER_METADATA_H_ */
