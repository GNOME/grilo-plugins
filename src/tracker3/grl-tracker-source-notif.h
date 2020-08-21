/*
 * Copyright (C) 2011 Intel Corporation.
 * Copyright (C) 2011-2012 Igalia S.L.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
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

#define TRACKER_DATASOURCES_REQUEST                                     \
  "SELECT "                                                             \
  "(SELECT GROUP_CONCAT(rdf:type(?source), \":\") "                     \
  " WHERE { ?urn nie:dataSource ?source }) "                            \
  "nie:dataSource(?urn) "                                               \
  "(SELECT GROUP_CONCAT(nie:title(?source), \":\") "                    \
  " WHERE { ?urn nie:dataSource ?source }) "                            \
  "(SELECT GROUP_CONCAT(nie:url(tracker:mountPoint(?source)), \":\") "  \
  " WHERE { ?urn nie:dataSource ?source }) "                            \
  "tracker:available(?urn) "                                            \
  "WHERE "                                                              \
  "{ "                                                                  \
  "?urn a nfo:FileDataObject . FILTER (bound(nie:dataSource(?urn)))"    \
  "} "                                                                  \
  "GROUP BY (nie:dataSource(?urn))"

/**/

void grl_tracker_notify_init     (TrackerSparqlConnection *sparql_conn);
void grl_tracker_notify_shutdown (void);

#endif /* _GRL_TRACKER_SOURCE_NOTIF_H_ */
