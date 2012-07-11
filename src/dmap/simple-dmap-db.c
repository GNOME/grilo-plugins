/*
 * Copyright (C) 2012 W. Michael Petullo.
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

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <glib.h>

#include "simple-dmap-db.h"

/* Media ID's start at max and go down. Container ID's start at 1 and go up. */
static guint nextid = G_MAXINT; /* NOTE: this should be G_MAXUINT, but iPhoto can't handle it. */

struct SimpleDMAPDbPrivate {
  GHashTable *db;
};

enum {
  PROP_0,
  PROP_RECORD_FACTORY,
};

SimpleDMAPDb *
simple_dmap_db_new (void)
{
  return SIMPLE_DMAP_DB (g_object_new (TYPE_SIMPLE_DMAP_DB, NULL));
}

static DMAPRecord *
simple_dmap_db_lookup_by_id (const DMAPDb *db, guint id)
{
  DMAPRecord *record;

  record = g_hash_table_lookup (SIMPLE_DMAP_DB (db)->priv->db, GUINT_TO_POINTER (id));
  g_object_ref (record);

  return record;
}

static void
simple_dmap_db_foreach (const DMAPDb *db,
                        GHFunc func,
                        gpointer data)
{
  g_hash_table_foreach (SIMPLE_DMAP_DB (db)->priv->db, func, data);
}

static gint64
simple_dmap_db_count (const DMAPDb *db)
{
  return g_hash_table_size (SIMPLE_DMAP_DB (db)->priv->db);
}

static guint
simple_dmap_db_add (DMAPDb *db, DMAPRecord *record)
{
  g_object_ref (record);
  g_hash_table_insert (SIMPLE_DMAP_DB (db)->priv->db, GUINT_TO_POINTER (nextid--), record);

  return nextid;
}

static void
simple_dmap_db_interface_init (gpointer iface, gpointer data)
{
  DMAPDbIface *dmap_db = iface;

  g_assert (G_TYPE_FROM_INTERFACE (dmap_db) == DMAP_TYPE_DB);

  dmap_db->add = simple_dmap_db_add;
  dmap_db->lookup_by_id = simple_dmap_db_lookup_by_id;
  dmap_db->foreach = simple_dmap_db_foreach;
  dmap_db->count = simple_dmap_db_count;
}

void
simple_dmap_db_filtered_foreach (SimpleDMAPDb *db,
                                 guint skip,
                                 guint count,
                                 GHRFunc predicate,
                                 gpointer pred_user_data,
                                 GHFunc func,
                                 gpointer user_data)
{
  GHashTableIter iter;
  gpointer key, val;
  guint i;

  g_hash_table_iter_init (&iter, db->priv->db);
  for (i = 0; g_hash_table_iter_next (&iter, &key, &val); i++) {
    if (i < skip) {
      continue;
    }
    if (i == skip + count) {
      break;
    }
    if (predicate (key, val, pred_user_data)) {
      func (key, val, user_data);
    }
  }
}

G_DEFINE_TYPE_WITH_CODE (SimpleDMAPDb, simple_dmap_db, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (DMAP_TYPE_DB, simple_dmap_db_interface_init))

static GObject*
simple_dmap_db_constructor (GType type, guint n_construct_params, GObjectConstructParam *construct_params)
{
  GObject *object;

  object = G_OBJECT_CLASS (simple_dmap_db_parent_class)->constructor (type, n_construct_params, construct_params);

  return object;
}

static void simple_dmap_db_init (SimpleDMAPDb *db)
{
  db->priv = SIMPLE_DMAP_DB_GET_PRIVATE (db);
  db->priv->db = g_hash_table_new_full (g_direct_hash,
                                        g_direct_equal,
                                        NULL,
                                        g_object_unref);
}

static void
simple_dmap_db_finalize (GObject *object)
{
  SimpleDMAPDb *db = SIMPLE_DMAP_DB (object);

  g_debug ("Finalizing SimpleDMAPDb (%d records)",
           g_hash_table_size (db->priv->db));

  g_hash_table_destroy (db->priv->db);
}

static void
simple_dmap_db_set_property (GObject *object,
                             guint prop_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
  switch (prop_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
simple_dmap_db_get_property (GObject *object,
                             guint prop_id,
                             GValue *value,
                             GParamSpec *pspec)
{
  switch (prop_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}


static void simple_dmap_db_class_init (SimpleDMAPDbClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SimpleDMAPDbPrivate));

  object_class->finalize = simple_dmap_db_finalize;
  object_class->constructor = simple_dmap_db_constructor;
  object_class->set_property = simple_dmap_db_set_property;
  object_class->get_property = simple_dmap_db_get_property;
}
