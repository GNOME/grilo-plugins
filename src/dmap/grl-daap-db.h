/*
 *  Database class for DAAP sharing
 *
 *  Copyright (C) 2008 W. Michael Petullo <mike@flyn.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __GRL_DAAP_DB
#define __GRL_DAAP_DB

#include <libdmapsharing/dmap.h>
#include <grilo.h>

#include "grl-daap-compat.h"

G_BEGIN_DECLS

#define TYPE_GRL_DAAP_DB (grl_daap_db_get_type ())

#define GRL_DAAP_DB(o)                                                         \
  (G_TYPE_CHECK_INSTANCE_CAST ((o),                                            \
                                TYPE_GRL_DAAP_DB,                              \
                                GrlDaapDb))

#define GRL_DAAP_DB_CLASS(k)                                                   \
  (G_TYPE_CHECK_CLASS_CAST ((k),                                               \
                             TYPE_GRL_DAAP_DB,                                 \
                             GrlDaapDbClass))
#define GRL_IS_DAAP_DB(o)                                                      \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o),                                            \
                                TYPE_GRL_DAAP_DB))

#define GRL_IS_DAAP_DB_CLASS(k)                                                \
  (G_TYPE_CHECK_CLASS_TYPE ((k),                                               \
                             TYPE_GRL_DAAP_DB_CLASS))

#define GRL_DAAP_DB_GET_CLASS(o)                                               \
  (G_TYPE_INSTANCE_GET_CLASS ((o),                                             \
                               TYPE_GRL_DAAP_DB,                               \
                               GrlDaapDbClass))

#define GRL_DAAP_DB_GET_PRIVATE(o)                                             \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o),                                           \
                                 TYPE_GRL_DAAP_DB,                             \
                                 GrlDaapDbPrivate))

typedef struct GrlDaapDbPrivate GrlDaapDbPrivate;

typedef struct {
  GObject parent;
  GrlDaapDbPrivate *priv;
} GrlDaapDb;

typedef struct {
  GObjectClass parent;
} GrlDaapDbClass;

void grl_daap_db_browse (GrlDaapDb *db,
                         GrlMedia *container,
                         GrlSource *source,
                         guint op_id,
                         guint skip,
                         guint count,
                         GrlSourceResultCb func,
                         gpointer user_data);

void grl_daap_db_search (GrlDaapDb *db,
                         GrlSource *source,
                         guint op_id,
                         GHRFunc predicate,
                         gpointer pred_user_data,
                         GrlSourceResultCb func,
                         gpointer user_data);

GrlDaapDb *grl_daap_db_new (void);

GType grl_daap_db_get_type (void);

#endif /* __GRL_DAAP_DB */

G_END_DECLS
