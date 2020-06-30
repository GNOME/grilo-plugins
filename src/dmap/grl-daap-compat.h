/*
 * Copyright (C) 2019 W. Michael Petullo
 * Copyright (C) 2019 Igalia S.L.
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

#ifndef _GRL_DAAP_COMPAT_H_
#define _GRL_DAAP_COMPAT_H_

#include "grl-dmap-compat.h"

#ifdef LIBDMAPSHARING_COMPAT

DMAPRecord *grl_daap_record_factory_create (DMAPRecordFactory *factory, gpointer user_data, GError **error);
guint grl_daap_db_add (DMAPDb *_db, DMAPRecord *_record, GError **error);

/* Building against libdmapsharing 3 API. */

#define dmap_av_connection_new daap_connection_new
#define DmapAvRecord DAAPRecord
#define DmapAvRecordInterface DAAPRecordIface
#define DMAP_AV_RECORD DAAP_RECORD
#define DMAP_TYPE_AV_RECORD DAAP_TYPE_RECORD
#define DMAP_IS_AV_RECORD IS_DAAP_RECORD

static inline DmapRecord *
grl_daap_record_factory_create_compat (DmapRecordFactory *factory, gpointer user_data)
{
  return grl_daap_record_factory_create (factory, user_data, NULL);
}

static inline guint
grl_daap_db_add_compat (DmapDb *_db, DmapRecord *_record)
{
  return grl_daap_db_add (_db, _record, NULL);
}

#else

/* Building against libdmapsharing 4 API. */

DmapRecord *grl_daap_record_factory_create (DmapRecordFactory *factory, gpointer user_data, GError **error);
guint grl_daap_db_add (DmapDb *_db, DmapRecord *_record, GError **error);

static inline DmapRecord *
grl_daap_record_factory_create_compat (DmapRecordFactory *factory, gpointer user_data, GError **error)
{
  return grl_daap_record_factory_create (factory, user_data, error);
}

static inline guint
grl_daap_db_add_compat (DmapDb *_db, DmapRecord *_record, GError **error)
{
  return grl_daap_db_add (_db, _record, error);
}

#endif

#endif /* _GRL_DAAP_COMPAT_H_ */
