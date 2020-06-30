/*
 *  Database class for DPAP sharing
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

#ifndef __GRL_DPAP_DB
#define __GRL_DPAP_DB

#include <libdmapsharing/dmap.h>
#include <grilo.h>

#include "grl-dpap-compat.h"

G_BEGIN_DECLS

#define TYPE_GRL_DPAP_DB (grl_dpap_db_get_type ())

#define GRL_DPAP_DB(o)                                                         \
  (G_TYPE_CHECK_INSTANCE_CAST ((o),                                            \
                                TYPE_GRL_DPAP_DB,                              \
                                GrlDpapDb))

#define GRL_DPAP_DB_CLASS(k)                                                   \
  (G_TYPE_CHECK_CLASS_CAST ((k),                                               \
                             TYPE_GRL_DPAP_DB,                                 \
                             GrlDpapDbClass))
#define GRL_IS_DPAP_DB(o)                                                      \
  (G_TYPE_CHECK_INSTANCE_TYPE ((o),                                            \
                                TYPE_GRL_DPAP_DB))
#define GRL_IS_DPAP_DB_CLASS(k)                                                \
  (G_TYPE_CHECK_CLASS_TYPE ((k),                                               \
                             TYPE_GRL_DPAP_DB_CLASS))

#define GRL_DPAP_DB_GET_CLASS(o)                                               \
  (G_TYPE_INSTANCE_GET_CLASS ((o),                                             \
                               TYPE_GRL_DPAP_DB,                               \
                               GrlDpapDbClass))

#define GRL_DPAP_DB_GET_PRIVATE(o)                                             \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o),                                           \
                                 TYPE_GRL_DPAP_DB,                             \
                                 GrlDpapDbPrivate))

typedef struct GrlDpapDbPrivate GrlDpapDbPrivate;

typedef struct {
  GObject parent;
  GrlDpapDbPrivate *priv;
} GrlDpapDb;

typedef struct {
  GObjectClass parent;
} GrlDpapDbClass;

GrlDpapDb *grl_dpap_db_new (void);
void grl_dpap_db_browse (GrlDpapDb *_db,
                         GrlMedia *container,
                         GrlSource *source,
                         guint op_id,
                         guint skip,
                         guint count,
                         GrlSourceResultCb func,
                         gpointer user_data);
void grl_dpap_db_search (GrlDpapDb *_db,
                         GrlSource *source,
                         guint op_id,
                         GHRFunc predicate,
                         gpointer pred_user_data,
                         GrlSourceResultCb func,
                         gpointer user_data);

GType grl_dpap_db_get_type (void);

#endif /* __GRL_DPAP_DB */

G_END_DECLS
