/*
 * Copyright (C) 2011 Intel Corporation.
 * Copyright (C) 2011-2012 Igalia S.L.
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

#ifndef _GRL_TRACKER_SOURCE_NOTIF_H_
#define _GRL_TRACKER_SOURCE_NOTIF_H_

#include "grl-tracker-source.h"

/* ------- Definitions ------- */

#define GRL_TRACKER_TYPE_SOURCE_NOTIFY grl_tracker_source_notify_get_type ()
G_DECLARE_FINAL_TYPE (GrlTrackerSourceNotify, grl_tracker_source_notify, GRL_TRACKER, SOURCE_NOTIFY, GObject)

/**/

GrlTrackerSourceNotify * grl_tracker_source_notify_new (GrlSource               *source,
                                                        TrackerSparqlConnection *sparql_conn);

#endif /* _GRL_TRACKER_SOURCE_NOTIF_H_ */
