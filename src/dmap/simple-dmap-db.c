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

/* This DMAP database implementation maintains a series of hash tables that
 * represent sets of media. The tables include: root, albums, and artists.
 * Root contains albums and artists, and albums and artists each contain
 * the set of albums and artists in the database, respectively. Thus this
 * database implementation imposes a hierarchical structure, whereas DMAP
 * normally provides a flat structure.
 *
 * Each hash table/set is a mapping between a GrlMediaBox and a series of
 * GrlMedia objects (either more GrlMediaBox objects, or, in the case of a
 * leaf, GrlMediaAudio objects). The constant GrlMediaBox objects (e.g.,
 * albums_box and artists_box) facilitate this, along with additional
 * GrlMediaAudio objects that the simple_dmap_db_add function creates.
 *
 * An application will normally first browse using the NULL container,
 * and thus will first receive albums_box and artists_box. Browsing
 * albums_box will provide the application the GrlMediaBox objects in
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
#include <string.h>

#include "simple-dmap-db.h"

/* Media ID's start at max and go down. Container ID's start at 1 and go up. */
static guint nextid = G_MAXINT; /* NOTE: this should be G_MAXUINT, but iPhoto can't handle it. */

static const gchar *ALBUMS_ID    =  "albums";
static const gchar *ALBUMS_NAME  =  "Albums";
static const gchar *ARTISTS_ID   = "artists";
static const gchar *ARTISTS_NAME = "Artists";

struct SimpleDMAPDbPrivate {
  GrlMediaBox *albums_box;  // Contains each album box (tracked with albums hash table).
  GrlMediaBox *artists_box; // Contains each artist box (tracked with artist hash table).

  GHashTable  *root;
  GHashTable  *albums;
  GHashTable  *artists;
};

enum {
  PROP_0,
  PROP_RECORD_FACTORY,
};

static guint
box_hash (gconstpointer a)
{
  return g_str_hash (grl_media_get_id (GRL_MEDIA (a)));
}

static gboolean
box_equal (gconstpointer a, gconstpointer b)
{
  return g_str_equal (grl_media_get_id (GRL_MEDIA (a)), grl_media_get_id (GRL_MEDIA (b)));
}

SimpleDMAPDb *
simple_dmap_db_new (void)
{
  SimpleDMAPDb *db = g_object_new (TYPE_SIMPLE_DMAP_DB, NULL);

  return db;
}

static DMAPRecord *
simple_dmap_db_lookup_by_id (const DMAPDb *db, guint id)
{
  g_error ("Not implemented");
  return NULL;
}

static void
simple_dmap_db_foreach (const DMAPDb *db,
                        GHFunc func,
                        gpointer data)
{
  g_error ("Not implemented");
}

static gint64
simple_dmap_db_count (const DMAPDb *db)
{
  g_error ("Not implemented");
  return 0;
}

static void
set_insert (GHashTable *category, const char *category_name, char *set_name, GrlMedia *media)
{
  gchar      *id = NULL;
  GrlMedia   *box;
  GHashTable *set;

  id = g_strdup_printf ("%s-%s", category_name, set_name);

  box = g_object_new (GRL_TYPE_MEDIA_BOX, NULL);
  grl_media_set_id    (box, id);
  grl_media_set_title (box, set_name);

  set = g_hash_table_lookup (category, box);
  if (NULL == set) {
    set = g_hash_table_new_full (box_hash, box_equal, g_object_unref, NULL);
    g_hash_table_insert (category, g_object_ref (box), set);
  }

  g_hash_table_insert (set, g_object_ref (media), NULL);

  g_free (id);
  g_object_unref (box);
}

static guint
simple_dmap_db_add (DMAPDb *_db, DMAPRecord *record)
{
  SimpleDMAPDb *db = SIMPLE_DMAP_DB (_db);
  gint   duration = 0;
  gint32  bitrate = 0,
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
               "songalbum",
               &album,
               "songartist",
               &artist,
               "bitrate",
               &bitrate,
               "duration",
               &duration,
               "songgenre",
               &genre,
               "title",
               &title,
               "track",
               &track,
               "location",
               &url,
               "has-video",
               &has_video,
                NULL);

  id_s = g_strdup_printf ("%u", nextid);

  if (has_video == TRUE) {
    media = grl_media_video_new ();
  } else {
    media = grl_media_audio_new ();
  }

  grl_media_set_id           (media, id_s);
  grl_media_set_duration     (media, duration);

  if (title) {
    grl_media_set_title (media, title);
  }

  if (url) {
    // Replace URL's daap:// with http://.
    url[0] = 'h'; url[1] = 't'; url[2] = 't'; url[3] = 'p';
    grl_media_set_url (media, url);
  }

  if (has_video == FALSE) {
    GrlMediaAudio *media_audio = GRL_MEDIA_AUDIO (media);

    grl_media_audio_set_bitrate      (media_audio, bitrate);
    grl_media_audio_set_track_number (media_audio, track);

    if (album) {
      grl_media_audio_set_album (media_audio, album);
    }

    if (artist) {
      grl_media_audio_set_artist (media_audio, artist);
    }

    if (genre) {
      grl_media_audio_set_genre (media_audio, genre);
    }
  }

  set_insert (db->priv->artists, ARTISTS_ID, artist, media);
  set_insert (db->priv->albums,  ALBUMS_ID,  album,  media);

  g_free (id_s);
  g_object_unref (media);

  return --nextid;
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

static gboolean
same_media (GrlMedia *a, GrlMedia *b)
{
  return ! strcmp (grl_media_get_id (a), grl_media_get_id (b));
}

void
simple_dmap_db_browse (SimpleDMAPDb *db,
                       GrlMedia *container,
                       GrlSource *source,
                       guint op_id,
                       guint skip,
                       guint count,
                       GrlSourceResultCb func,
                       gpointer user_data)
{
  int i;
  guint remaining;
  GHashTable *hash_table;
  GHashTableIter iter;
  gpointer key, val;

  const gchar *box_id = grl_media_get_id (container);
  if (NULL == box_id) {
    hash_table = db->priv->root;
  } else if (same_media (container, GRL_MEDIA (db->priv->albums_box))) {
    hash_table = db->priv->albums;
  } else if (same_media (container, GRL_MEDIA (db->priv->artists_box))) {
    hash_table = db->priv->artists;
  } else {
    hash_table = g_hash_table_lookup (db->priv->artists, container);
    if (NULL == hash_table) {
      hash_table = g_hash_table_lookup (db->priv->albums, container);
    }
  }

  // Should not be NULL; this means the container requested
  // does not exist in the database.
  if (NULL == hash_table) {
    GError *error = g_error_new (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_BROWSE_FAILED,
                                 _("Invalid container identifier %s"),
                                 box_id);
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
    if (GRL_IS_MEDIA_BOX (key)) {
      grl_media_box_set_childcount (GRL_MEDIA_BOX (key), g_hash_table_size (val));
    }
    func (source, op_id, GRL_MEDIA (g_object_ref (key)), --remaining, user_data, NULL);
  }
done:
  return;
}

void
simple_dmap_db_search (SimpleDMAPDb *db,
                       GrlSource *source,
                       guint op_id,
                       GHRFunc predicate,
                       gpointer pred_user_data,
                       GrlSourceResultCb func,
                       gpointer user_data)
{
  gint i, j, k;
  guint remaining = 0;
  gpointer key1, val1, key2, val2;
  GHashTable *hash_table[] = { db->priv->albums, db->priv->artists };
  GHashTable *results = NULL; // Use hash table to avoid duplicates.
  GHashTableIter iter1, iter2;

  results = g_hash_table_new (g_str_hash, g_str_equal);

  // For albums and artists...
  for (i = 0; i < 2; i++) {
    g_hash_table_iter_init (&iter1, hash_table[i]);
    // For each album or artist in above...
    for (j = 0; g_hash_table_iter_next (&iter1, &key1, &val1); j++) {
      if (GRL_IS_MEDIA_BOX (key1)) {
        g_hash_table_iter_init (&iter2, val1);
        // For each media item in above...
        for (k = 0; g_hash_table_iter_next (&iter2, &key2, &val2); k++) {
          const char *id = grl_media_get_id (GRL_MEDIA (key2));
          // If the predicate returns true, add to results set.
          if (predicate (key2, val2, pred_user_data)
           && ! g_hash_table_contains (results, id)) {
            remaining++;
            g_hash_table_insert (results, (gpointer) id, key2);
          }
        }
      }
    }
  }

  // Process results set.
  g_hash_table_iter_init (&iter1, results);
  for (i = 0; g_hash_table_iter_next (&iter1, &key1, &val1); i++) {
    func (source, op_id, GRL_MEDIA (g_object_ref (val1)), --remaining, user_data, NULL);
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

static void
simple_dmap_db_init (SimpleDMAPDb *db)
{
  db->priv = SIMPLE_DMAP_DB_GET_PRIVATE (db);

  db->priv->albums_box  = g_object_new (GRL_TYPE_MEDIA_BOX, NULL);
  db->priv->artists_box = g_object_new (GRL_TYPE_MEDIA_BOX, NULL);

  grl_media_set_id    (GRL_MEDIA (db->priv->albums_box), ALBUMS_ID);
  grl_media_set_title (GRL_MEDIA (db->priv->albums_box), _(ALBUMS_NAME));

  grl_media_set_id    (GRL_MEDIA (db->priv->artists_box), ARTISTS_ID);
  grl_media_set_title (GRL_MEDIA (db->priv->artists_box), _(ARTISTS_NAME));

  db->priv->root    = g_hash_table_new_full (box_hash, box_equal, g_object_unref, (GDestroyNotify) g_hash_table_destroy);
  db->priv->albums  = g_hash_table_new_full (box_hash, box_equal, g_object_unref, (GDestroyNotify) g_hash_table_destroy);
  db->priv->artists = g_hash_table_new_full (box_hash, box_equal, g_object_unref, (GDestroyNotify) g_hash_table_destroy);

  g_hash_table_insert (db->priv->root, g_object_ref (db->priv->albums_box),  db->priv->albums);
  g_hash_table_insert (db->priv->root, g_object_ref (db->priv->artists_box), db->priv->artists);
}

static void
simple_dmap_db_finalize (GObject *object)
{
  SimpleDMAPDb *db = SIMPLE_DMAP_DB (object);

  g_debug ("Finalizing SimpleDMAPDb");

  g_object_unref (db->priv->albums_box);
  g_object_unref (db->priv->artists_box);

  g_hash_table_destroy (db->priv->albums);
  g_hash_table_destroy (db->priv->artists);
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


static void
simple_dmap_db_class_init (SimpleDMAPDbClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (SimpleDMAPDbPrivate));

  object_class->finalize = simple_dmap_db_finalize;
  object_class->constructor = simple_dmap_db_constructor;
  object_class->set_property = simple_dmap_db_set_property;
  object_class->get_property = simple_dmap_db_get_property;
}
