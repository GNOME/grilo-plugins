/*
 * Copyright (C) 202011-2012 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
 * Copyright (C) 2020 Red Hat Inc
 *
 * Contact: Carlos Garnacho <carlosg@gnome.org>
 *
 * Authors: Carlos Garnacho <carlosg@gnome.org>
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

#ifndef _GRL_TRACKER_SOURCE_STATEMENTS_H_
#define _GRL_TRACKER_SOURCE_STATEMENTS_H_

#include "grl-tracker-source.h"

/**/

typedef enum {
  GRL_TRACKER_QUERY_MEDIA_FROM_URI, /* Arguments: ~uri */
  GRL_TRACKER_QUERY_RESOLVE,        /* Arguments: ~resource */
  GRL_TRACKER_QUERY_RESOLVE_URI,    /* Arguments: ~uri */
  GRL_TRACKER_QUERY_ALL,            /* No arguments */
  GRL_TRACKER_QUERY_FTS_SEARCH,     /* Arguments: ~match */
  GRL_TRACKER_QUERY_N_QUERIES,
} GrlTrackerQueryType;

TrackerSparqlStatement *grl_tracker_source_create_statement (GrlTrackerSource     *source,
                                                             GrlTrackerQueryType   type,
                                                             GrlOperationOptions  *options,
                                                             GList                *keys,
                                                             const gchar          *extra_sparql,
                                                             GError              **error);

#endif /* _GRL_TRACKER_SOURCE_STATEMENTS_H_ */
