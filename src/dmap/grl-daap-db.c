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

/* This DAAP database implementation maintains a series of hash tables that
 * represent sets of media. The tables include: root, albums, and artists.
 * Root contains albums and artists, and albums and artists each contain
 * the set of albums and artists in the database, respectively. Thus this
 * database implementation imposes a hierarchical structure, whereas DAAP
 * normally provides a flat structure.
 *
 * Each hash table/set is a mapping between a GrlMedia container and a series of
 * GrlMedia objects (either more GrlMedia container objects, or, in the case of a
 * leaf, GrlMediaAudio objects). The constant GrlMedia container objects (e.g.,
 * albums_container and artists_container) facilitate this, along with additional
 * GrlMediaAudio objects that the grl_daap_db_add function creates.
 *
 * An application will normally first browse using the NULL container,
 * and thus will first receive albums_container and artists_container. Browsing
 * albums_container will provide the application the GrlMedia container objects in
 * albums. Further browsing one of these objects will provide the
 * application with the songs contained therein.
 *
 * Grilo IDs must be unique. Here the convention is:
 *
 *	1. Top-level IDs resemble their name (e.g., Album's ID is "albums").
 *	2. The ID for albums, artists, etc. is the item name prefixed by
 *	   the category name (e.g., albums-NAME-OF-ALBUM).
 *	3. The ID for songs is the string form of the integer identifier used
 *	   to identify the song to libdmapsharing.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <glib.h>
#include <grilo.h>
#include <string.h>
#include <libdmapsharing/dmap.h>

#include "grl-daap-compat.h"
#include "grl-common.h"
#include "grl-daap-db.h"

#define ALBUMS_ID    "albums"
#define ALBUMS_NAME  _("Albums")
#define ARTISTS_ID   "artists"
#define ARTISTS_NAME _("Artists")

/* Media ID's start at max and go down. Container ID's start at 1 and go up. */
static guint nextid = G_MAXINT; /* NOTE: this should be G_MAXUINT, but iPhoto can't handle it. */

struct GrlDaapDbPrivate {
  /* Contains each album container (tracked with albums hash table) */
  GrlMedia *albums_container;

  /* Contains each artist container (tracked with artist hash table) */
  GrlMedia *artists_container;

  GHashTable  *root;
  GHashTable  *albums;
  GHashTable  *artists;
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

GrlDaapDb *
grl_daap_db_new (void)
{
  GrlDaapDb *db = g_object_new (TYPE_GRL_DAAP_DB, NULL);

  return db;
}

static DmapRecord *
grl_daap_db_lookup_by_id (const DmapDb *db, guint id)
{
  g_error ("Not implemented");
  return NULL;
}

static void
grl_daap_db_foreach (const DmapDb *db,
                     DmapIdRecordFunc func,
                     gpointer data)
{
  g_error ("Not implemented");
}

static gint64
grl_daap_db_count (const DmapDb *db)
{
  g_error ("Not implemented");
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
  if (NULL == set) {
    set = g_hash_table_new_full (container_hash, container_equal, g_object_unref, NULL);
    g_hash_table_insert (category, g_object_ref (container), set);
  }

  g_hash_table_insert (set, g_object_ref (media), NULL);

  g_free (id);
  g_object_unref (container);
}

guint
grl_daap_db_add (DmapDb *_db, DmapRecord *_record, GError **error)
{
  g_assert (GRL_IS_DAAP_DB (_db));
  g_assert (DMAP_IS_AV_RECORD (_record));

  GrlDaapDb *db = GRL_DAAP_DB (_db);
  DmapAvRecord *record = DMAP_AV_RECORD (_record);

  gint   duration = 0;
  gint32  bitrate = 0,
             disc = 0,
            track = 0;
  gchar  *id_s    = NULL,
         *title   = NULL,
         *album   = NULL,
         *artist  = NULL,
         *genre   = NULL,
         *url     = NULL;
  gboolean has_video;
  GrlMedia *media;

  g_object_get (record,
               "songalbum", &album,
               "songartist", &artist,
               "bitrate", &bitrate,
               "duration", &duration,
               "songgenre", &genre,
               "title", &title,
               "track", &track,
               "disc", &disc,
               "location", &url,
               "has-video", &has_video,
                NULL);

  id_s = g_strdup_printf ("%u", nextid);

  if (has_video == TRUE) {
    media = grl_media_video_new ();
  } else {
    media = grl_media_audio_new ();
  }

  grl_media_set_id (media, id_s);
  grl_media_set_duration (media, duration);

  if (title) {
    grl_media_set_title (media, title);
  }

  if (url) {
    /* Replace URL's daap:// with http:// */
    url[0] = 'h'; url[1] = 't'; url[2] = 't'; url[3] = 'p';
    grl_media_set_url (media, url);
  }

  if (has_video == FALSE) {
    grl_media_set_bitrate (media, bitrate);
    grl_media_set_track_number (media, track);

    if (disc != 0) {
      grl_media_set_album_disc_number (media, disc);
    }

    if (album) {
      grl_media_set_album (media, album);
    }

    if (artist) {
      grl_media_set_artist (media, artist);
    }

    if (genre) {
      grl_media_set_genre (media, genre);
    }
  }

  set_insert (db->priv->artists, ARTISTS_ID, artist, media);
  set_insert (db->priv->albums,  ALBUMS_ID,  album,  media);

  g_free (id_s);
  g_object_unref (media);
  g_free (album);
  g_free (artist);
  g_free (genre);
  g_free (title);
  g_free (url);

  return --nextid;
}

static gboolean
same_media (GrlMedia *a, GrlMedia *b)
{
  return strcmp (grl_media_get_id (a), grl_media_get_id (b)) == 0;
}

void
grl_daap_db_browse (GrlDaapDb *db,
                    GrlMedia *container,
                    GrlSource *source,
                    guint op_id,
                    guint skip,
                    guint count,
                    GrlSourceResultCb func,
                    gpointer user_data)
{
  g_assert (GRL_IS_DAAP_DB (db));

  int i;
  guint remaining;
  GHashTable *hash_table;
  GHashTableIter iter;
  gpointer key, val;

  const gchar *container_id = grl_media_get_id (container);
  if (NULL == container_id) {
    hash_table = db->priv->root;
  } else if (same_media (container, GRL_MEDIA (db->priv->albums_container))) {
    hash_table = db->priv->albums;
  } else if (same_media (container, GRL_MEDIA (db->priv->artists_container))) {
    hash_table = db->priv->artists;
  } else {
    hash_table = g_hash_table_lookup (db->priv->artists, container);
    if (NULL == hash_table) {
      hash_table = g_hash_table_lookup (db->priv->albums, container);
    }
  }

  /* Should not be NULL; this means the container requested
     does not exist in the database. */
  if (NULL == hash_table) {
    GError *error = g_error_new (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_BROWSE_FAILED,
                                 _("Invalid container identifier %s"),
                                 container_id);
    func (source, op_id, NULL, 0, user_data, error);
    goto done;
  }

  remaining = g_hash_table_size (hash_table) - skip;
  remaining = remaining < count ? remaining : count;
  g_hash_table_iter_init (&iter, hash_table);
  for (i = 0; g_hash_table_iter_next (&iter, &key, &val) && i < skip + count; i++) {
    if (i < skip) {
      continue;
    }
    if (grl_media_is_container (key)) {
      grl_media_set_childcount (key, g_hash_table_size (val));
    }
    func (source, op_id, GRL_MEDIA (g_object_ref (key)), --remaining, user_data, NULL);
  }
done:
  return;
}

void
grl_daap_db_search (GrlDaapDb *db,
                    GrlSource *source,
                    guint op_id,
                    GHRFunc predicate,
                    gpointer pred_user_data,
                    GrlSourceResultCb func,
                    gpointer user_data)
{
  g_assert (GRL_IS_DAAP_DB (db));

  gint i, j, k;
  guint remaining = 0;
  gpointer key1, val1, key2, val2;
  GHashTable *hash_tables[] = { db->priv->albums, db->priv->artists };

  /* Use hash table to avoid duplicates */
  GHashTable *results = NULL;
  GHashTableIter iter1, iter2;

  results = g_hash_table_new (g_str_hash, g_str_equal);

  /* For albums and artists... */
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
  DmapDbInterface *daap_db = iface;

  g_assert (G_TYPE_FROM_INTERFACE (daap_db) == DMAP_TYPE_DB);

  daap_db->add = grl_daap_db_add_compat;
  daap_db->lookup_by_id = grl_daap_db_lookup_by_id;
  daap_db->foreach = grl_daap_db_foreach;
  daap_db->count = grl_daap_db_count;
}

G_DEFINE_TYPE_WITH_CODE (GrlDaapDb, grl_daap_db, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GrlDaapDb)
                         G_IMPLEMENT_INTERFACE (DMAP_TYPE_DB, dmap_db_interface_init))

static GObject*
grl_daap_db_constructor (GType type, guint n_construct_params, GObjectConstructParam *construct_params)
{
  GObject *object;

  object = G_OBJECT_CLASS (grl_daap_db_parent_class)->constructor (type, n_construct_params, construct_params);

  return object;
}

static void
grl_daap_db_init (GrlDaapDb *db)
{
  db->priv = grl_daap_db_get_instance_private (db);

  db->priv->albums_container  = grl_media_container_new ();
  db->priv->artists_container = grl_media_container_new ();

  grl_media_set_id (GRL_MEDIA (db->priv->albums_container), ALBUMS_ID);
  grl_media_set_title (GRL_MEDIA (db->priv->albums_container), ALBUMS_NAME);

  grl_media_set_id (GRL_MEDIA (db->priv->artists_container), ARTISTS_ID);
  grl_media_set_title (GRL_MEDIA (db->priv->artists_container), ARTISTS_NAME);

  db->priv->root    = g_hash_table_new_full (container_hash, container_equal, g_object_unref, (GDestroyNotify) g_hash_table_destroy);
  db->priv->albums  = g_hash_table_new_full (container_hash, container_equal, g_object_unref, (GDestroyNotify) g_hash_table_destroy);
  db->priv->artists = g_hash_table_new_full (container_hash, container_equal, g_object_unref, (GDestroyNotify) g_hash_table_destroy);

  g_hash_table_insert (db->priv->root, g_object_ref (db->priv->albums_container),  db->priv->albums);
  g_hash_table_insert (db->priv->root, g_object_ref (db->priv->artists_container), db->priv->artists);
}

static void
grl_daap_db_finalize (GObject *object)
{
  GrlDaapDb *db = GRL_DAAP_DB (object);

  GRL_DEBUG ("Finalizing GrlDaapDb");

  g_object_unref (db->priv->albums_container);
  g_object_unref (db->priv->artists_container);

  g_hash_table_destroy (db->priv->albums);
  g_hash_table_destroy (db->priv->artists);
}

static void
grl_daap_db_set_property (GObject *object,
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
grl_daap_db_get_property (GObject *object,
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


static void
grl_daap_db_class_init (GrlDaapDbClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = grl_daap_db_finalize;
  object_class->constructor = grl_daap_db_constructor;
  object_class->set_property = grl_daap_db_set_property;
  object_class->get_property = grl_daap_db_get_property;
}
