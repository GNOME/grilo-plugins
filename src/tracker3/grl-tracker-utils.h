/*
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
 * Copyright (C) 2020 Red Hat Inc.
 *
 * Contact: Carlos Garnacho <carlosg@gnome.org>
 *
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 *          Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
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

#ifndef _GRL_TRACKER_UTILS_H_
#define _GRL_TRACKER_UTILS_H_

#include "grl-tracker-source-priv.h"

/**/

typedef void (*tracker_grl_sparql_setter_cb_t) (TrackerSparqlCursor *cursor,
                                                gint                 column,
                                                GrlMedia            *media,
                                                GrlKeyID             key);

typedef struct {
  GrlKeyID     grl_key;
  const gchar *sparql_var_name;
  const gchar *sparql_key_attr_call;
  GrlTypeFilter filter;

  tracker_grl_sparql_setter_cb_t set_value;
} tracker_grl_sparql_t;

extern GrlKeyID grl_metadata_key_tracker_urn;

const GList *grl_tracker_supported_keys (GrlSource *source);

gboolean grl_tracker_key_is_supported (const GrlKeyID key);

const gchar * grl_tracker_key_get_variable_name (const GrlKeyID key);
const gchar * grl_tracker_key_get_sparql_statement (const GrlKeyID key,
                                                    GrlTypeFilter  filter);

void grl_tracker_setup_key_mappings (void);

TrackerResource * grl_tracker_build_resource_from_media (GrlMedia *media, GList *keys);

tracker_grl_sparql_t *grl_tracker_get_mapping_from_sparql (const gchar *key);

GrlMedia *grl_tracker_build_grilo_media (GrlMediaType type);

#endif /* _GRL_TRACKER_UTILS_H_ */
