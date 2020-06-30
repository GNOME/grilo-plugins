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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* See grl-daap-db.c for a description of this database. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <grilo.h>
#include <glib/gi18n-lib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <glib.h>
#include <string.h>
#include <libdmapsharing/dmap.h>

#include "grl-dpap-compat.h"
#include "grl-common.h"
#include "grl-dpap-db.h"

#define PHOTOS_ID     "photos"
#define PHOTOS_NAME _("Photos")

/* Media IDs start at max and go down. Container IDs start at 1 and go up. */
static guint nextid = G_MAXINT; /* NOTE: this should be G_MAXUINT, but iPhoto can't handle it. */

struct GrlDpapDbPrivate {
  /* Contains each picture container (tracked with photos hash table) */
  GrlMedia *photos_container;

  GHashTable  *root;
  GHashTable  *photos;
};

enum {
  PROP_0,
  PROP_RECORD_FACTORY,
};

static guint
container_hash (gconstpointer a)
{
  return g_str_hash (grl_media_get_id (GRL_MEDIA (a)));
}

static gboolean
container_equal (gconstpointer a, gconstpointer b)
{
  return g_str_equal (grl_media_get_id (GRL_MEDIA (a)), grl_media_get_id (GRL_MEDIA (b)));
}

GrlDpapDb *
grl_dpap_db_new (void)
{
  GrlDpapDb *db = g_object_new (TYPE_GRL_DPAP_DB, NULL);

  return db;
}

static DmapRecord *
grl_dpap_db_lookup_by_id (const DmapDb *db, guint id)
{
  g_warning ("Not implemented");
  return NULL;
}

static void
grl_dpap_db_foreach (const DmapDb *db,
                     DmapIdRecordFunc func,
                     gpointer data)
{
  g_warning ("Not implemented");
}

static gint64
grl_dpap_db_count (const DmapDb *db)
{
  g_warning ("Not implemented");
  return 0;
}

static void
set_insert (GHashTable *category, const char *category_name, char *set_name, GrlMedia *media)
{
  gchar      *id = NULL;
  GrlMedia   *container;
  GHashTable *set;

  id = g_strdup_printf ("%s-%s", category_name, set_name);

  container = grl_media_container_new ();
  grl_media_set_id (container, id);
  grl_media_set_title (container, set_name);

  set = g_hash_table_lookup (category, container);
  if (set == NULL) {
    set = g_hash_table_new_full (container_hash, container_equal, g_object_unref, NULL);
    g_hash_table_insert (category, g_object_ref (container), set);
  }

  g_hash_table_insert (set, g_object_ref (media), NULL);

  g_free (id);
  g_object_unref (container);
}

guint
grl_dpap_db_add (DmapDb *_db, DmapRecord *_record, GError **error)
{
  g_assert (GRL_IS_DPAP_DB (_db));
  g_assert (DMAP_IS_IMAGE_RECORD (_record));

  GrlDpapDb *db = GRL_DPAP_DB (_db);
  DmapImageRecord *record = DMAP_IMAGE_RECORD (_record);

  gint        height        = 0,
              width         = 0,
              largefilesize = 0,
              creationdate  = 0,
              rating        = 0;
  GArray     *thumbnail     = NULL;
  gchar      *id_s          = NULL,
             *filename      = NULL,
             *aspectratio   = NULL,
             *format        = NULL,
             *comments      = NULL,
             *url           = NULL;
  GrlMedia   *media;

  g_object_get (record,
               "large-filesize", &largefilesize,
               "creation-date", &creationdate,
               "rating", &rating,
               "filename", &filename,
               "aspect-ratio", &aspectratio,
               "pixel-height", &height,
               "pixel-width", &width,
               "format", &format,
               "comments", &comments,
               "thumbnail", &thumbnail,
               "location", &url,
                NULL);

  id_s = g_strdup_printf ("%u", nextid);

  media = grl_media_image_new ();

  grl_media_set_id (media, id_s);

  if (filename)
    grl_media_set_title (media, filename);

  if (url) {
    /* Replace URL's dpap:// with http:// */
    memcpy (url, "http", 4);
    grl_media_set_url (media, url);
  }

  grl_media_set_width (media, width);
  grl_media_set_height (media, height);

  set_insert (db->priv->photos,  PHOTOS_ID, "Unknown",  media);

  g_free (id_s);
  g_object_unref (media);
  g_free (filename);
  g_free (aspectratio);
  g_free (format);
  g_free (comments);
  g_free (url);
  g_array_unref (thumbnail);

  return --nextid;
}

static gboolean
same_media (GrlMedia *a, GrlMedia *b)
{
  return strcmp (grl_media_get_id (a), grl_media_get_id (b)) == 0;
}

void
grl_dpap_db_browse (GrlDpapDb *db,
                    GrlMedia *container,
                    GrlSource *source,
                    guint op_id,
                    guint skip,
                    guint count,
                    GrlSourceResultCb func,
                    gpointer user_data)
{
  g_assert (GRL_IS_DPAP_DB (db));

  int i;
  guint remaining;
  GHashTable *hash_table;
  GHashTableIter iter;
  gpointer key, val;

  const gchar *container_id = grl_media_get_id (container);
  if (container_id == NULL) {
    hash_table = db->priv->root;
  } else if (same_media (container, GRL_MEDIA (db->priv->photos_container))) {
    hash_table = db->priv->photos;
  } else {
    hash_table = g_hash_table_lookup (db->priv->photos, container);
  }

  /* Should not be NULL; this means the container requested
     does not exist in the database. */
  if (hash_table == NULL) {
    GError *error = g_error_new (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_BROWSE_FAILED,
                                "Invalid container identifier %s",
                                 container_id);
    func (source, op_id, NULL, 0, user_data, error);
    goto done;
  }

  remaining = g_hash_table_size (hash_table) - skip;
  remaining = remaining < count ? remaining : count;
  g_hash_table_iter_init (&iter, hash_table);
  for (i = 0; g_hash_table_iter_next (&iter, &key, &val) && i < skip + count; i++) {
    if (i < skip)
      continue;
    if (grl_media_is_container (key))
      grl_media_set_childcount (key, g_hash_table_size (val));
    func (source, op_id, GRL_MEDIA (g_object_ref (key)), --remaining, user_data, NULL);
  }
done:
  return;
}

void
grl_dpap_db_search (GrlDpapDb *db,
                    GrlSource *source,
                    guint op_id,
                    GHRFunc predicate,
                    gpointer pred_user_data,
                    GrlSourceResultCb func,
                    gpointer user_data)
{
  g_assert (GRL_IS_DPAP_DB (db));

  gint i, j, k;
  guint remaining = 0;
  gpointer key1, val1, key2, val2;
  GHashTable *hash_tables[] = { db->priv->photos };

  /* Use hash table to avoid duplicates */
  GHashTable *results = NULL;
  GHashTableIter iter1, iter2;

  results = g_hash_table_new (g_str_hash, g_str_equal);

  /* For photos ... */
  for (i = 0; i < G_N_ELEMENTS (hash_tables); i++) {
    g_hash_table_iter_init (&iter1, hash_tables[i]);
    /* For each album or artist in above... */
    for (j = 0; g_hash_table_iter_next (&iter1, &key1, &val1); j++) {
      if (grl_media_is_container (key1)) {
        g_hash_table_iter_init (&iter2, val1);
        /* For each media item in above... */
        for (k = 0; g_hash_table_iter_next (&iter2, &key2, &val2); k++) {
          const char *id = grl_media_get_id (GRL_MEDIA (key2));
          /* If the predicate returns true, add to results set. */
          if (predicate (key2, val2, pred_user_data)
           && ! g_hash_table_contains (results, id)) {
            remaining++;
            g_hash_table_insert (results, (gpointer) id, key2);
          }
        }
      }
    }
  }

  /* Process results set. */
  g_hash_table_iter_init (&iter1, results);
  for (i = 0; g_hash_table_iter_next (&iter1, &key1, &val1); i++) {
    func (source, op_id, GRL_MEDIA (g_object_ref (val1)), --remaining, user_data, NULL);
  }
}

static void
dmap_db_interface_init (gpointer iface, gpointer data)
{
  DmapDbInterface *dpap_db = iface;

  g_assert (G_TYPE_FROM_INTERFACE (dpap_db) == DMAP_TYPE_DB);

  dpap_db->add = grl_dpap_db_add_compat;
  dpap_db->lookup_by_id = grl_dpap_db_lookup_by_id;
  dpap_db->foreach = grl_dpap_db_foreach;
  dpap_db->count = grl_dpap_db_count;
}

G_DEFINE_TYPE_WITH_CODE (GrlDpapDb, grl_dpap_db, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GrlDpapDb)
                         G_IMPLEMENT_INTERFACE (DMAP_TYPE_DB, dmap_db_interface_init))

static GObject*
grl_dpap_db_constructor (GType type, guint n_construct_params, GObjectConstructParam *construct_params)
{
  GObject *object;

  object = G_OBJECT_CLASS (grl_dpap_db_parent_class)->constructor (type, n_construct_params, construct_params);

  return object;
}

static void
grl_dpap_db_init (GrlDpapDb *db)
{
  db->priv = grl_dpap_db_get_instance_private (db);

  db->priv->photos_container  = grl_media_container_new ();

  grl_media_set_id (GRL_MEDIA (db->priv->photos_container), PHOTOS_ID);
  grl_media_set_title (GRL_MEDIA (db->priv->photos_container), PHOTOS_NAME);

  db->priv->root   = g_hash_table_new_full (container_hash, container_equal, g_object_unref, (GDestroyNotify) g_hash_table_destroy);
  db->priv->photos = g_hash_table_new_full (container_hash, container_equal, g_object_unref, (GDestroyNotify) g_hash_table_destroy);

  g_hash_table_insert (db->priv->root, g_object_ref (db->priv->photos_container), db->priv->photos);
}

static void
grl_dpap_db_finalize (GObject *object)
{
  GrlDpapDb *db = GRL_DPAP_DB (object);

  GRL_DEBUG ("Finalizing GrlDpapDb");

  g_object_unref (db->priv->photos_container);

  g_hash_table_destroy (db->priv->photos);
}

static void
grl_dpap_db_class_init (GrlDpapDbClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = grl_dpap_db_finalize;
  object_class->constructor = grl_dpap_db_constructor;
}
