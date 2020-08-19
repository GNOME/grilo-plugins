/*
 * Copyright (C) 2011-2012 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
 * Copyright (C) 2020 Red Hat Inc.
 *
 * Contact: Carlos Garnacho <carlosg@gnome.org>
 *
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 *          Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
 *          Juan A. Suarez Romero <jasuarez@igalia.com>
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

#ifndef _GRL_TRACKER_SOURCE_PRIV_H_
#define _GRL_TRACKER_SOURCE_PRIV_H_

#include "grl-tracker-source.h"
#include "grl-tracker-source-cache.h"
#include "grl-tracker-source-notif.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <tracker-sparql.h>

/* ---- Source information ---- */

#define GRL_TRACKER_SOURCE_ID   "grl-tracker3-source"
#define GRL_TRACKER_SOURCE_NAME "Tracker3"
#define GRL_TRACKER_SOURCE_DESC _("A plugin for searching multimedia content using Tracker3")

#define GRL_TRACKER_AUTHOR  "Igalia S.L."
#define GRL_TRACKER_LICENSE "LGPL"
#define GRL_TRACKER_SITE    "http://www.igalia.com"

/**/

#define GRL_TRACKER_SOURCE_GET_PRIVATE(object)           \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                 \
                               GRL_TRACKER_SOURCE_TYPE,	\
                               GrlTrackerSourcePriv))

typedef enum {
  GRL_TRACKER_SOURCE_STATE_INSERTING,
  GRL_TRACKER_SOURCE_STATE_RUNNING,
  GRL_TRACKER_SOURCE_STATE_DELETING,
  GRL_TRACKER_SOURCE_STATE_DELETED,
} GrlTrackerSourceState;

struct _GrlTrackerSourcePrivate {
  TrackerSparqlConnection *tracker_connection;
  GDBusProxy *writeback;

  GHashTable *operations;
  GrlTrackerSourceNotify *notifier;
  GList *cached_statements;

  gboolean notify_changes;

  GrlTrackerSourceState state;
};

/**/

extern GrlPlugin *grl_tracker_plugin;

/* shared data across  */
extern GrlTrackerCache *grl_tracker_item_cache;

/* tracker plugin config */
extern gchar *grl_tracker_store_path;
extern gchar *grl_tracker_miner_service;

#endif /* _GRL_TRACKER_SOURCE_PRIV_H_ */
