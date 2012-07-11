/*
 *  Database class for DMAP sharing
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

#ifndef __SIMPLE_DMAP_DB
#define __SIMPLE_DMAP_DB

#include <libdmapsharing/dmap.h>

G_BEGIN_DECLS

#define TYPE_SIMPLE_DMAP_DB                     \
  (simple_dmap_db_get_type ())

#define SIMPLE_DMAP_DB(o)                             \
  (G_TYPE_CHECK_INSTANCE_CAST ((o),                   \
                               TYPE_SIMPLE_DMAP_DB,   \
                               SimpleDMAPDb))

#define SIMPLE_DMAP_DB_CLASS(k)                 \
  (G_TYPE_CHECK_CLASS_CAST((k),                 \
                           TYPE_SIMPLE_DMAP_DB, \
                           SimpleDMAPDbClass))
#define IS_SIMPLE_DMAP_DB(o)                          \
  (G_TYPE_CHECK_INSTANCE_TYPE((o),                    \
                              TYPE_SIMPLE_DMAP_DB))
#define IS_SIMPLE_DMAP_DB_CLASS(k)                       \
  (G_TYPE_CHECK_CLASS_TYPE((k),                          \
                           TYPE_SIMPLE_DMAP_DB_CLASS))

#define SIMPLE_DMAP_DB_GET_CLASS(o)                \
  (G_TYPE_INSTANCE_GET_CLASS((o),                  \
                             TYPE_SIMPLE_DMAP_DB,  \
                             SimpleDMAPDbClass))

#define SIMPLE_DMAP_DB_GET_PRIVATE(o)                 \
  (G_TYPE_INSTANCE_GET_PRIVATE((o),                   \
                               TYPE_SIMPLE_DMAP_DB,   \
                               SimpleDMAPDbPrivate))

typedef struct SimpleDMAPDbPrivate SimpleDMAPDbPrivate;

typedef struct {
  GObject parent;
  SimpleDMAPDbPrivate *priv;
} SimpleDMAPDb;

typedef struct {
  GObjectClass parent;
} SimpleDMAPDbClass;

void simple_dmap_db_filtered_foreach (SimpleDMAPDb *db,
                                      guint skip,
                                      guint count,
                                      GHRFunc predicate,
                                      gpointer pred_user_data,
                                      GHFunc func,
                                      gpointer user_data);

SimpleDMAPDb *simple_dmap_db_new (void);

GType simple_dmap_db_get_type (void);

#endif /* __SIMPLE_DMAP_DB */

G_END_DECLS
