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

#ifndef _GRL_TRACKER_MEDIA_PRIV_H_
#define _GRL_TRACKER_MEDIA_PRIV_H_

#include "grl-tracker-media.h"
#include "grl-tracker-media-cache.h"

#include <tracker-sparql.h>

/* ---- MediaSource information ---- */

#define GRL_TRACKER_MEDIA_ID   "grl-tracker-media"
#define GRL_TRACKER_MEDIA_NAME "TrackerMedia"
#define GRL_TRACKER_MEDIA_DESC                  \
  "A plugin for searching multimedia "          \
  "content using Tracker"

#define GRL_TRACKER_AUTHOR  "Igalia S.L."
#define GRL_TRACKER_LICENSE "LGPL"
#define GRL_TRACKER_SITE    "http://www.igalia.com"

/**/

#define GRL_TRACKER_MEDIA_GET_PRIVATE(object)		\
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                \
                               GRL_TRACKER_MEDIA_TYPE,	\
                               GrlTrackerMediaPriv))

typedef enum {
  GRL_TRACKER_MEDIA_STATE_INSERTING,
  GRL_TRACKER_MEDIA_STATE_RUNNING,
  GRL_TRACKER_MEDIA_STATE_DELETING,
  GRL_TRACKER_MEDIA_STATE_DELETED,
} GrlTrackerMediaState;

struct _GrlTrackerMediaPriv {
  TrackerSparqlConnection *tracker_connection;

  GHashTable *operations;

  gchar *tracker_datasource;
  gboolean notify_changes;

  GrlTrackerMediaState state;
  guint notification_ref;
};

/**/

extern TrackerSparqlConnection *grl_tracker_connection;
extern const GrlPluginInfo *grl_tracker_plugin;

/* shared data across  */
extern GrlTrackerCache *grl_tracker_item_cache;
extern GHashTable *grl_tracker_modified_sources;

/* tracker plugin config */
extern gboolean grl_tracker_per_device_source;
extern gboolean grl_tracker_browse_filesystem;

#endif /* _GRL_TRACKER_MEDIA_PRIV_H_ */
