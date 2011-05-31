/*
 * Copyright (C) 2011 Igalia S.L.
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

#ifndef _GRL_TRACKER_MEDIA_API_H_
#define _GRL_TRACKER_MEDIA_API_H_

#include "grl-tracker-media.h"

/**/

void grl_tracker_media_init_requests (void);

const GList *grl_tracker_media_writable_keys (GrlSource *source);

void grl_tracker_media_query (GrlSource *source,
                              GrlSourceQuerySpec *qs);

void grl_tracker_media_resolve (GrlSource *source,
                                GrlSourceResolveSpec *rs);

void grl_tracker_media_store_metadata (GrlSource *source,
                                       GrlSourceStoreMetadataSpec *sms);

void grl_tracker_media_cancel (GrlSource *source, guint operation_id);

void grl_tracker_media_search (GrlSource *source,
                               GrlSourceSearchSpec *ss);

void grl_tracker_media_browse (GrlSource *source,
                               GrlSourceBrowseSpec *bs);

gboolean grl_tracker_media_change_start (GrlSource *source,
                                         GError **error);

gboolean grl_tracker_media_change_stop (GrlSource *source,
                                        GError **error);

#endif /* _GRL_TRACKER_MEDIA_API_H_ */
