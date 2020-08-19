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

#ifndef _GRL_TRACKER_SOURCE_API_H_
#define _GRL_TRACKER_SOURCE_API_H_

#include "grl-tracker-source.h"

/**/

void grl_tracker_source_init_requests (void);

const GList *grl_tracker_source_writable_keys (GrlSource *source);

void grl_tracker_source_query (GrlSource *source,
                               GrlSourceQuerySpec *qs);

void grl_tracker_source_resolve (GrlSource *source,
                                 GrlSourceResolveSpec *rs);

gboolean grl_tracker_source_may_resolve (GrlSource *source,
                                         GrlMedia *media,
                                         GrlKeyID key_id,
                                         GList **missing_keys);

void grl_tracker_source_store_metadata (GrlSource *source,
                                        GrlSourceStoreMetadataSpec *sms);

void grl_tracker_source_cancel (GrlSource *source, guint operation_id);

void grl_tracker_source_search (GrlSource *source,
                                GrlSourceSearchSpec *ss);

void grl_tracker_source_browse (GrlSource *source,
                                GrlSourceBrowseSpec *bs);

gboolean grl_tracker_source_change_start (GrlSource *source,
                                          GError **error);

gboolean grl_tracker_source_change_stop (GrlSource *source,
                                         GError **error);

GrlCaps *grl_tracker_source_get_caps (GrlSource *source,
                                      GrlSupportedOps operation);

GrlSupportedOps grl_tracker_source_supported_operations (GrlSource *source);

gboolean grl_tracker_source_test_media_from_uri (GrlSource *source,
                                                 const gchar *uri);

void grl_tracker_source_get_media_from_uri (GrlSource *source,
                                            GrlSourceMediaFromUriSpec *mfus);

#endif /* _GRL_TRACKER_SOURCE_API_H_ */
