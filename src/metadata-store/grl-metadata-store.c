/*
 * Copyright (C) 2010, 2011 Igalia S.L.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <grilo.h>
#include <sqlite3.h>
#include <string.h>

#include "grl-metadata-store.h"

#define GRL_LOG_DOMAIN_DEFAULT metadata_store_log_domain
GRL_LOG_DOMAIN_STATIC(metadata_store_log_domain);

#define SOURCE_ID   "grl-metadata-store"
#define SOURCE_NAME _("Metadata Store")
#define SOURCE_DESC _("A plugin for storing extra metadata information")

#define GRL_SQL_DB  "grl-metadata-store.db"

#define GRL_SQL_CREATE_TABLE_STORE			 \
  "CREATE TABLE IF NOT EXISTS store ("			 \
  "source_id TEXT,"					 \
  "media_id TEXT,"					 \
  "play_count INTEGER,"					 \
  "rating REAL,"					 \
  "last_position INTEGER,"				 \
  "last_played DATE,"                                    \
  "favourite INTEGER DEFAULT 0,"                         \
  "type_id INTEGER)"

#define GRL_SQL_ALTER_TABLE_ADD_FAVOURITE			 \
  "ALTER TABLE store ADD COLUMN "                        \
  "favourite INTEGER"

#define GRL_SQL_ALTER_TABLE_ADD_TYPE_ID			 \
  "ALTER TABLE store ADD COLUMN "                        \
  "type_id INTEGER"

#define GRL_SQL_GET_METADATA				\
  "SELECT * FROM store "				\
  "WHERE source_id=? AND media_id=? "			\
  "LIMIT 1"

#define GRL_SQL_UPDATE_METADATA			\
  "UPDATE store SET %s "			\
  "WHERE source_id=? AND media_id=?"

#define GRL_SQL_INSERT_METADATA			\
  "INSERT INTO store "				\
  "(type_id, %s source_id, media_id) VALUES "		\
  "(?, %s ?, ?)"

#define GRL_SQL_SEARCH                          \
  "SELECT * FROM store "                        \
  "LIMIT %u OFFSET %u"

#define GRL_SQL_FAVOURITE_FILTER                \
  "favourite=?"

#define GRL_SQL_SOURCE_FILTER                   \
  "source_id=?"

#define GRL_SQL_TYPE_FILTER                     \
  "type_id IN ( ? , ? , ? )"

#define GRL_SQL_SEARCH_FILTER                   \
  "SELECT * FROM store "                        \
  "WHERE %s "                                   \
  "LIMIT %u OFFSET %u"

struct _GrlMetadataStorePrivate {
  sqlite3 *db;
};

enum {
  STORE_SOURCE_ID = 0,
  STORE_MEDIA_ID,
  STORE_PLAY_COUNT,
  STORE_RATING,
  STORE_LAST_POSITION,
  STORE_LAST_PLAYED,
  STORE_FAVOURITE,
  STORE_TYPE_ID
};

enum {
  MEDIA = 0,
  MEDIA_AUDIO,
  MEDIA_VIDEO,
  MEDIA_IMAGE,
  MEDIA_CONTAINER
};

static void grl_metadata_store_source_class_finalize (GObject *object);

static GrlMetadataStoreSource *grl_metadata_store_source_new (void);

static void grl_metadata_store_source_resolve (GrlSource *source,
                                               GrlSourceResolveSpec *rs);

static void grl_metadata_store_source_store_metadata (GrlSource *source,
                                                      GrlSourceStoreMetadataSpec *sms);

static gboolean grl_metadata_store_source_may_resolve (GrlSource *source,
                                                       GrlMedia *media,
                                                       GrlKeyID key_id,
                                                       GList **missing_keys);

static const GList *grl_metadata_store_source_supported_keys (GrlSource *source);

static const GList *grl_metadata_store_source_writable_keys (GrlSource *source);

static GrlCaps *grl_metadata_store_source_get_caps (GrlSource *source,
                                                    GrlSupportedOps operation);

static void grl_metadata_store_source_search (GrlSource *source,
                                              GrlSourceSearchSpec *ss);

gboolean grl_metadata_store_source_plugin_init (GrlRegistry *registry,
                                                GrlPlugin *plugin,
                                                GList *configs);

G_DEFINE_TYPE_WITH_PRIVATE (GrlMetadataStoreSource, grl_metadata_store_source, GRL_TYPE_SOURCE)

/* =================== GrlMetadataStore Plugin  =============== */

gboolean
grl_metadata_store_source_plugin_init (GrlRegistry *registry,
                                       GrlPlugin *plugin,
                                       GList *configs)
{
  GRL_LOG_DOMAIN_INIT (metadata_store_log_domain, "metadata-store");

  GRL_DEBUG ("grl_metadata_store_source_plugin_init");

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  GrlMetadataStoreSource *source = grl_metadata_store_source_new ();
  grl_registry_register_source (registry,
                                plugin,
                                GRL_SOURCE (source),
                                NULL);
  return TRUE;
}

GRL_PLUGIN_DEFINE (GRL_MAJOR,
                   GRL_MINOR,
                   METADATA_STORE_PLUGIN_ID,
                   "Metadata Store",
                   "A plugin for storing extra metadata information",
                   "Igalia S.L.",
                   VERSION,
                   "LGPL-2.1-or-later",
                   "http://www.igalia.com",
                   grl_metadata_store_source_plugin_init,
                   NULL,
                   NULL);

/* ================== GrlMetadataStore GObject ================ */

static GrlMetadataStoreSource *
grl_metadata_store_source_new (void)
{
  GRL_DEBUG ("grl_metadata_store_source_new");
  return g_object_new (GRL_METADATA_STORE_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       NULL);
}

static void
grl_metadata_store_source_class_init (GrlMetadataStoreSourceClass * klass)
{
  GObjectClass *g_class = G_OBJECT_CLASS (klass);
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

  g_class->finalize = grl_metadata_store_source_class_finalize;

  source_class->supported_keys = grl_metadata_store_source_supported_keys;
  source_class->writable_keys = grl_metadata_store_source_writable_keys;
  source_class->get_caps = grl_metadata_store_source_get_caps;
  source_class->search = grl_metadata_store_source_search;
  source_class->may_resolve = grl_metadata_store_source_may_resolve;
  source_class->resolve = grl_metadata_store_source_resolve;
  source_class->store_metadata = grl_metadata_store_source_store_metadata;
}

static void
grl_metadata_store_source_class_finalize (GObject *object)
{
  GrlMetadataStoreSource *source = GRL_METADATA_STORE_SOURCE (object);

  sqlite3_close (source->priv->db);

  G_OBJECT_CLASS (grl_metadata_store_source_parent_class)->finalize (object);
}

static void
grl_metadata_store_source_init (GrlMetadataStoreSource *source)
{
  gint r;
  gchar *path;
  gchar *db_path;
  gchar *sql_error = NULL;

  source->priv = grl_metadata_store_source_get_instance_private (source);

  path = g_strconcat (g_get_user_data_dir (),
                      G_DIR_SEPARATOR_S, "grilo-plugins",
                      NULL);

  if (!g_file_test (path, G_FILE_TEST_IS_DIR)) {
    g_mkdir_with_parents (path, 0775);
  }

  GRL_DEBUG ("Opening database connection...");
  db_path = g_strconcat (path, G_DIR_SEPARATOR_S, GRL_SQL_DB, NULL);
  r = sqlite3_open (db_path, &source->priv->db);
  g_free (path);

  if (r) {
    g_critical ("Failed to open database '%s': %s",
                db_path, sqlite3_errmsg (source->priv->db));
    sqlite3_close (source->priv->db);
    g_free (db_path);
    return;
  }
  g_free (db_path);

  GRL_DEBUG ("  OK");

  GRL_DEBUG ("Checking database tables...");
  r = sqlite3_exec (source->priv->db, GRL_SQL_CREATE_TABLE_STORE,
		    NULL, NULL, &sql_error);

  if (r) {
    if (sql_error) {
      GRL_WARNING ("Failed to create database tables: %s", sql_error);
      g_clear_pointer (&sql_error, sqlite3_free);
    } else {
      GRL_WARNING ("Failed to create database tables.");
    }
    sqlite3_close (source->priv->db);
    return;
  }

  // For backwards compatibility, add newer columns if they don't exist
  // in the old database.
  sqlite3_exec (source->priv->db, GRL_SQL_ALTER_TABLE_ADD_FAVOURITE,
                NULL, NULL, NULL);

  sqlite3_exec (source->priv->db, GRL_SQL_ALTER_TABLE_ADD_TYPE_ID,
                NULL, NULL, NULL);

  GRL_DEBUG ("  OK");
}

/* ======================= Utilities ==================== */

static sqlite3_stmt *
query_metadata_store (sqlite3 *db,
		      const gchar *source_id,
		      const gchar *media_id)
{
  gint r, idx;
  sqlite3_stmt *sql_stmt = NULL;

  GRL_DEBUG ("get_metadata");

  r = sqlite3_prepare_v2 (db, GRL_SQL_GET_METADATA, -1, &sql_stmt, NULL);

  if (r != SQLITE_OK) {
    GRL_WARNING ("Failed to get metadata: %s", sqlite3_errmsg (db));
    return NULL;
  }

  idx = 0;
  sqlite3_bind_text(sql_stmt, ++idx, source_id, -1, SQLITE_STATIC);
  sqlite3_bind_text(sql_stmt, ++idx, media_id, -1, SQLITE_STATIC);

  return sql_stmt;
}

static void
fill_metadata_from_stmt (GrlMedia *media, GList *keys, sqlite3_stmt *stmt)
{
  GList *iter;
  gint play_count, last_position, favourite;
  gdouble rating;
  gchar *last_played;

  iter = keys;
  while (iter) {
    GrlKeyID key = GRLPOINTER_TO_KEYID (iter->data);
    if (key == GRL_METADATA_KEY_PLAY_COUNT) {
      play_count = sqlite3_column_int (stmt, STORE_PLAY_COUNT);
      grl_media_set_play_count (media, play_count);
    } else if (key == GRL_METADATA_KEY_RATING) {
      rating = sqlite3_column_double (stmt, STORE_RATING);
      grl_media_set_rating (media, rating, 5.00);
    } else if (key == GRL_METADATA_KEY_LAST_PLAYED) {
      GDateTime *date;
      last_played = (gchar *) sqlite3_column_text (stmt, STORE_LAST_PLAYED);
      date = grl_date_time_from_iso8601 (last_played);
      if (date) {
        grl_media_set_last_played (media, date);
        g_date_time_unref (date);
      } else {
        GRL_WARNING ("Unable to set 'last-played', as '%s' date is invalid",
                     last_played);
      }
    } else if (key == GRL_METADATA_KEY_LAST_POSITION) {
      last_position = sqlite3_column_int (stmt, STORE_LAST_POSITION);
      grl_media_set_last_position (media, last_position);
    } else if (key == GRL_METADATA_KEY_FAVOURITE) {
      favourite = sqlite3_column_int (stmt, STORE_FAVOURITE);
      grl_media_set_favourite (media, (gboolean) favourite);
    }
    iter = g_list_next (iter);
  }
}

static void
fill_metadata (GrlMedia *media, GList *keys, sqlite3_stmt *stmt)
{
  gint r;

  while ((r = sqlite3_step (stmt)) == SQLITE_BUSY);

  if (r != SQLITE_ROW) {
    /* No info in DB for this item, bail out silently */
    sqlite3_finalize (stmt);
    return;
  }

  fill_metadata_from_stmt (media, keys, stmt);

  sqlite3_finalize (stmt);
}

static const gchar *
get_column_name_from_key_id (GrlKeyID key_id)
{
  static const gchar *col_names[] = {"rating", "last_played", "last_position",
				     "play_count", "favourite"};
  if (key_id == GRL_METADATA_KEY_RATING) {
    return col_names[0];
  } else if (key_id == GRL_METADATA_KEY_LAST_PLAYED) {
    return col_names[1];
  } else if (key_id == GRL_METADATA_KEY_LAST_POSITION) {
    return col_names[2];
  } else if (key_id == GRL_METADATA_KEY_PLAY_COUNT) {
    return col_names[3];
  } else if (key_id == GRL_METADATA_KEY_FAVOURITE) {
    return col_names[4];
  } else {
    return NULL;
  }
}

static int
get_media_type (GrlMedia *media)
{
  if (grl_media_is_audio (media)) {
    return MEDIA_AUDIO;
  }
  if (grl_media_is_video (media)) {
    return MEDIA_VIDEO;
  }
  if (grl_media_is_image (media)) {
    return MEDIA_IMAGE;
  }
  if (grl_media_is_container (media)) {
    return MEDIA_CONTAINER;
  }

  return MEDIA;
}

static gboolean
bind_and_exec (sqlite3 *db,
	       const gchar *sql,
	       const gchar *source_id,
	       const gchar *media_id,
	       GList *col_names,
	       GList *keys,
	       GrlMedia *media)
{
  gint r;
  gint int_value;
  double double_value;
  GList *iter_names, *iter_keys;
  guint count;
  sqlite3_stmt *stmt;

  /* Create statement from sql */
  GRL_DEBUG ("%s", sql);
  r = sqlite3_prepare_v2 (db, sql, strlen (sql), &stmt, NULL);

  if (r != SQLITE_OK) {
    GRL_WARNING ("Failed to update metadata for '%s - %s': %s",
                     source_id, media_id, sqlite3_errmsg (db));
    sqlite3_finalize (stmt);
    return FALSE;
  }

  /* Bind media type */
  sqlite3_bind_int (stmt, 1, get_media_type (media));

  /* Bind column values */
  count = 2;
  iter_names = col_names;
  iter_keys = keys;
  while (iter_names) {
    if (iter_names->data) {
      GrlKeyID key = GRLPOINTER_TO_KEYID (iter_keys->data);
      if (key == GRL_METADATA_KEY_RATING) {
	double_value = grl_media_get_rating (media);
	sqlite3_bind_double (stmt, count, double_value);
      } else if (key == GRL_METADATA_KEY_PLAY_COUNT) {
	int_value = grl_media_get_play_count (media);
	sqlite3_bind_int (stmt, count, int_value);
      } else if (key == GRL_METADATA_KEY_LAST_POSITION) {
	int_value = grl_media_get_last_position (media);
	sqlite3_bind_int (stmt, count, int_value);
      } else if (key == GRL_METADATA_KEY_LAST_PLAYED) {
	GDateTime *date;
	char *date_str;
	date = grl_media_get_last_played (media);
	if (date) {
	  date_str = g_date_time_format (date, "%F %T");
	  sqlite3_bind_text (stmt, count, date_str, -1, SQLITE_STATIC);
	  g_free (date_str);
	}
      } else if (key == GRL_METADATA_KEY_FAVOURITE) {
        int_value = (gint) grl_media_get_favourite (media);
        sqlite3_bind_int (stmt, count, int_value);
      }
      count++;
    }
    iter_keys = g_list_next (iter_keys);
    iter_names = g_list_next (iter_names);
  }

  sqlite3_bind_text (stmt, count++, source_id, -1, SQLITE_STATIC);
  sqlite3_bind_text (stmt, count++, media_id, -1, SQLITE_STATIC);

  /* execute query */
  while ((r = sqlite3_step (stmt)) == SQLITE_BUSY);

  sqlite3_finalize (stmt);

  return (r == SQLITE_DONE);
}

static gboolean
prepare_and_exec_update (sqlite3 *db,
			 const gchar *source_id,
			 const gchar *media_id,
			 GList *col_names,
			 GList *keys,
			 GrlMedia *media)
{
  gchar *sql;
  gint r;
  GList *iter_names;
  GString *sql_buf;
  gchar *sql_set;

  GRL_DEBUG ("prepare_and_exec_update");

  /* Prepare sql "set" for update query */
  sql_buf = g_string_new ("type_id=?");
  iter_names = col_names;
  while (iter_names) {
    gchar *col_name = (gchar *) iter_names->data;
    if (col_name) {
      g_string_append_printf (sql_buf, " , %s=?", col_name);
    }
    iter_names = g_list_next (iter_names);
  }
  sql_set = g_string_free (sql_buf, FALSE);

  /* Execute query */
  sql = g_strdup_printf (GRL_SQL_UPDATE_METADATA, sql_set);
  r = bind_and_exec (db, sql, source_id, media_id, col_names, keys, media);
  g_free (sql);
  g_free (sql_set);

  return r;
}

static gboolean
prepare_and_exec_insert (sqlite3 *db,
			 const gchar *source_id,
			 const gchar *media_id,
			 GList *col_names,
			 GList *keys,
			 GrlMedia *media)
{
  gchar *sql;
  gint r;
  GList *iter_names;
  GString *sql_buf_cols, *sql_buf_values;
  gchar *sql_cols, *sql_values;

  GRL_DEBUG ("prepare_and_exec_insert");

  /* Prepare sql for insert query */
  sql_buf_cols = g_string_new ("");
  sql_buf_values = g_string_new ("");
  iter_names = col_names;
  while (iter_names) {
    gchar *col_name = (gchar *) iter_names->data;
    if (col_name) {
      g_string_append_printf (sql_buf_cols, "%s, ", col_name);
      g_string_append (sql_buf_values, "?, ");
    }
    iter_names = g_list_next (iter_names);
  }
  sql_cols = g_string_free (sql_buf_cols, FALSE);
  sql_values = g_string_free (sql_buf_values, FALSE);

  /* Execute query */
  sql = g_strdup_printf (GRL_SQL_INSERT_METADATA, sql_cols, sql_values);
  r = bind_and_exec (db, sql, source_id, media_id, col_names, keys, media);
  g_free (sql);
  g_free (sql_cols);
  g_free (sql_values);

  return r;
}

static GList *
write_keys (sqlite3 *db,
            const gchar *source_id,
            const gchar *media_id,
            GrlSourceStoreMetadataSpec *sms,
            GError **error)
{
  GList *col_names = NULL;
  GList *iter;
  GList *failed_keys = NULL;
  guint supported_keys = 0;
  gint r;

  /* Get DB column names for each key to be updated */
  iter = sms->keys;
  while (iter) {
    const gchar *col_name =
        get_column_name_from_key_id (GRLPOINTER_TO_KEYID (iter->data));
    if (!col_name) {
      GRL_WARNING ("Key %" GRL_KEYID_FORMAT " is not supported for "
                       "writing, ignoring...",
                 GRLPOINTER_TO_KEYID (iter->data));
      failed_keys = g_list_prepend (failed_keys, iter->data);
    } else {
      supported_keys++;
    }
    col_names = g_list_prepend (col_names, (gchar *) col_name);
    iter = g_list_next (iter);
  }
  col_names = g_list_reverse (col_names);

  if (supported_keys == 0) {
    GRL_WARNING ("Failed to update metadata, none of the specified "
                     "keys is writable");
    *error = g_error_new (GRL_CORE_ERROR,
                          GRL_CORE_ERROR_STORE_METADATA_FAILED,
                          _("Failed to update metadata: %s"),
                          _("specified keys are not writable"));
    goto done;
  }

  r = prepare_and_exec_update (db,
			       source_id,
			       media_id,
			       col_names,
			       sms->keys,
			       sms->media);

  if (!r) {
    GRL_WARNING ("Failed to update metadata for '%s - %s': %s",
                     source_id, media_id, sqlite3_errmsg (db));
    g_list_free (failed_keys);
    failed_keys = g_list_copy (sms->keys);
    *error = g_error_new (GRL_CORE_ERROR,
                          GRL_CORE_ERROR_STORE_METADATA_FAILED,
                          _("Failed to update metadata: %s"),
                          sqlite3_errmsg (db));
    goto done;
  }

  if (sqlite3_changes (db) == 0) {
    /* We have to create the row */
    r = prepare_and_exec_insert (db,
				 source_id,
				 media_id,
				 col_names,
				 sms->keys,
				 sms->media);
  }

  if (!r) {
    GRL_WARNING ("Failed to update metadata for '%s - %s': %s",
                     source_id, media_id, sqlite3_errmsg (db));
    g_list_free (failed_keys);
    failed_keys = g_list_copy (sms->keys);
    *error = g_error_new_literal (GRL_CORE_ERROR,
                                  GRL_CORE_ERROR_STORE_METADATA_FAILED,
                                  _("Failed to update metadata"));
    goto done;
  }

 done:
  g_list_free (col_names);
  return failed_keys;
}

static GrlMedia *
create_media (sqlite3_stmt * stmt, GList *keys)
{
  GrlMedia *media;
  gint media_type;

  media_type = sqlite3_column_int (stmt, STORE_TYPE_ID);
  switch (media_type) {
  case MEDIA_AUDIO:
    media = grl_media_audio_new ();
    break;
  case MEDIA_VIDEO:
    media = grl_media_video_new ();
    break;
  case MEDIA_IMAGE:
    media = grl_media_image_new ();
    break;
  case MEDIA_CONTAINER:
    media = grl_media_container_new ();
    break;
  default:
    media = grl_media_new ();
  }

  grl_media_set_source (media,
                    (const gchar *) sqlite3_column_text (stmt, STORE_SOURCE_ID));
  grl_media_set_id (media,
                    (const gchar *) sqlite3_column_text (stmt, STORE_MEDIA_ID));
  fill_metadata_from_stmt (media, keys, stmt);

  return media;
}

/* ================== API Implementation ================ */

static const GList *
grl_metadata_store_source_supported_keys (GrlSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_RATING,
                                      GRL_METADATA_KEY_PLAY_COUNT,
                                      GRL_METADATA_KEY_LAST_PLAYED,
                                      GRL_METADATA_KEY_LAST_POSITION,
                                      GRL_METADATA_KEY_FAVOURITE,
                                      NULL);
  }
  return keys;
}

static const GList *
grl_metadata_store_source_writable_keys (GrlSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_RATING,
                                      GRL_METADATA_KEY_PLAY_COUNT,
                                      GRL_METADATA_KEY_LAST_PLAYED,
                                      GRL_METADATA_KEY_LAST_POSITION,
                                      GRL_METADATA_KEY_FAVOURITE,
                                      NULL);
  }
  return keys;
}

static GrlCaps *
grl_metadata_store_source_get_caps (GrlSource *source,
                                    GrlSupportedOps operation)
{
  static GrlCaps *caps = NULL;
  GList * keys;

  if (caps == NULL) {
      caps = grl_caps_new ();
      keys = grl_metadata_key_list_new (GRL_METADATA_KEY_FAVOURITE,
                                        GRL_METADATA_KEY_SOURCE,
                                        GRL_METADATA_KEY_INVALID);
      grl_caps_set_key_filter (caps, keys);
      g_list_free (keys);
      grl_caps_set_type_filter (caps, GRL_TYPE_FILTER_ALL);
  }

  return caps;
}

static gboolean
grl_metadata_store_source_may_resolve (GrlSource *source,
                                       GrlMedia *media,
                                       GrlKeyID key_id,
                                       GList **missing_keys)
{
  if (!(key_id == GRL_METADATA_KEY_RATING
        || key_id == GRL_METADATA_KEY_PLAY_COUNT
        || key_id == GRL_METADATA_KEY_LAST_PLAYED
        || key_id == GRL_METADATA_KEY_LAST_POSITION
        || key_id == GRL_METADATA_KEY_FAVOURITE))
    return FALSE;


  if (media) {
    if (!(grl_media_is_video (media) ||
          grl_media_is_audio (media) ||
          key_id == GRL_METADATA_KEY_FAVOURITE))
      /* the keys we handle for now only make sense for audio and video,
         with exception of the 'favourite' key, valid as well for pictures
         and containers */
      return FALSE;

    if (grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_ID))
      return TRUE;
  }

  if (missing_keys)
    *missing_keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID, NULL);

  return FALSE;
}

static void
grl_metadata_store_source_resolve (GrlSource *source,
                                   GrlSourceResolveSpec *rs)
{
  const gchar *source_id, *media_id;
  sqlite3_stmt *stmt;
  GError *error = NULL;

  GRL_DEBUG (__FUNCTION__);

  source_id = grl_media_get_source (rs->media);
  media_id = grl_media_get_id (rs->media);

  /* We need the source id */
  if (!source_id) {
    GRL_WARNING ("Failed to resolve metadata: source-id not available");
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_RESOLVE_FAILED,
                         _("Failed to resolve: %s"),
                         _("“source-id” not available"));
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);
    g_error_free (error);
    return;
  }

  /* Special case for root categories */
  if (!media_id) {
    media_id = "";
  }

  stmt = query_metadata_store (GRL_METADATA_STORE_SOURCE (source)->priv->db,
			       source_id, media_id);
  if (stmt) {
    fill_metadata (rs->media, rs->keys, stmt);
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
  } else {
    GRL_WARNING ("Failed to resolve metadata");
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_RESOLVE_FAILED,
                                 _("Failed to resolve"));
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);
    g_error_free (error);
  }
}

static void
grl_metadata_store_source_store_metadata (GrlSource *source,
                                          GrlSourceStoreMetadataSpec *sms)
{
  GRL_DEBUG ("grl_metadata_store_source_set_metadata");

  const gchar *media_id, *source_id;
  GError *error = NULL;
  GList *failed_keys = NULL;

  source_id = grl_media_get_source (sms->media);
  media_id = grl_media_get_id (sms->media);

  /* We need the source id */
  if (!source_id) {
    GRL_WARNING ("Failed to update metadata: source-id not available");
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_STORE_METADATA_FAILED,
                         _("Failed to update metadata: %s"),
                         _("“source-id” not available"));
    failed_keys = g_list_copy (sms->keys);
  } else {
    /* Special case for root categories */
    if (!media_id) {
      media_id = "";
    }

    failed_keys = write_keys (GRL_METADATA_STORE_SOURCE (source)->priv->db,
                              source_id, media_id, sms, &error);
  }

  sms->callback (sms->source, sms->media, failed_keys, sms->user_data, error);

  g_clear_error (&error);
  g_list_free (failed_keys);
}

static void
grl_metadata_store_source_search (GrlSource *source,
                                  GrlSourceSearchSpec *ss)
{
  sqlite3_stmt *sql_stmt = NULL;
  sqlite3 *db;
  gchar *sql;
  gint r;
  gint i;
  GError *error = NULL;
  GrlMedia *media;
  GList *iter, *medias = NULL;
  GValue *filter_favourite_val;
  GValue *filter_source_val;
  GrlTypeFilter filter_type_val;
  GString *filters;
  guint count;
  gint type_filter[3];

  GRL_DEBUG (__FUNCTION__);

  db = GRL_METADATA_STORE_SOURCE (source)->priv->db;
  if (!db) {
    GRL_WARNING ("Can't execute operation: no database connection.");
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_QUERY_FAILED,
                                 _("No database connection"));
    ss->callback (ss->source, ss->operation_id, NULL, 0, ss->user_data, error);
    g_error_free (error);
    return;
  }

  filters = g_string_new ("");

  filter_favourite_val = grl_operation_options_get_key_filter (ss->options,
                                                               GRL_METADATA_KEY_FAVOURITE);
  filter_source_val = grl_operation_options_get_key_filter (ss->options,
                                                            GRL_METADATA_KEY_SOURCE);
  filter_type_val = grl_operation_options_get_type_filter (ss->options);

  if (filter_favourite_val) {
    filters = g_string_append (filters, GRL_SQL_FAVOURITE_FILTER);
  }

  if (filter_source_val) {
    if (filters->len > 0) {
      filters = g_string_append (filters, " AND ");
    }
    filters = g_string_append (filters, GRL_SQL_SOURCE_FILTER);
  }

  if (filter_type_val != GRL_TYPE_FILTER_ALL) {
    /* Fill the type_filter array */
    if (filter_type_val & GRL_TYPE_FILTER_AUDIO) {
      type_filter[0] = MEDIA_AUDIO;
    } else {
      type_filter[0] = -1;
    }
    if (filter_type_val & GRL_TYPE_FILTER_VIDEO) {
      type_filter[1] = MEDIA_VIDEO;
    } else {
      type_filter[1] = -1;
    }
    if (filter_type_val & GRL_TYPE_FILTER_IMAGE) {
      type_filter[2] = MEDIA_IMAGE;
    } else {
      type_filter[2] = -1;
    }
    if (filters->len > 0) {
      filters = g_string_append (filters, " AND ");
    }
    filters = g_string_append (filters, GRL_SQL_TYPE_FILTER);
  }

  if (filters->len > 0) {
    sql = g_strdup_printf (GRL_SQL_SEARCH_FILTER,
                           filters->str,
                           grl_operation_options_get_count (ss->options),
                           grl_operation_options_get_skip (ss->options));
  } else {
    sql = g_strdup_printf (GRL_SQL_SEARCH,
                           grl_operation_options_get_count (ss->options),
                           grl_operation_options_get_skip (ss->options));
  }

  r = sqlite3_prepare_v2 (db, sql, -1, &sql_stmt, NULL);

  g_free (sql);
  g_string_free (filters, TRUE);

  if (r != SQLITE_OK) {
    GRL_WARNING ("Failed to search in the metadata store: %s", sqlite3_errmsg (db));
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_SEARCH_FAILED,
                         _("Failed to search: %s"),
                         sqlite3_errmsg (db));
    ss->callback (ss->source, ss->operation_id, NULL, 0, ss->user_data, error);
    g_error_free (error);
    return;
  }

  count = 1;

  if (filter_favourite_val) {
    sqlite3_bind_int (sql_stmt, count++, (gint) g_value_get_boolean (filter_favourite_val));
  }

  if (filter_source_val) {
    sqlite3_bind_text (sql_stmt, count++, g_value_get_string (filter_source_val), -1, SQLITE_STATIC);
  }

  if (filter_type_val != GRL_TYPE_FILTER_ALL) {
    for (i = 0; i < G_N_ELEMENTS (type_filter); i++) {
      sqlite3_bind_int (sql_stmt, count++, type_filter[i]);
    }
  }

  while ((r = sqlite3_step (sql_stmt)) == SQLITE_BUSY);

  count = 0;
  while (r == SQLITE_ROW) {
    media = create_media (sql_stmt, ss->keys);
    medias = g_list_prepend (medias, media);
    count++;
    r = sqlite3_step (sql_stmt);
  }

  if (r != SQLITE_DONE) {
    GRL_WARNING ("Failed to search in the metadata store: %s", sqlite3_errmsg (db));
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_SEARCH_FAILED,
                         _("Failed to search: %s"),
                         sqlite3_errmsg (db));
    ss->callback (ss->source, ss->operation_id, NULL, 0, ss->user_data, error);
    g_error_free (error);
    sqlite3_finalize (sql_stmt);
    return;
  }

  sqlite3_finalize (sql_stmt);

  if (count > 0) {
    iter = medias;
    while (iter) {
      media = GRL_MEDIA (iter->data);
      ss->callback (ss->source,
                    ss->operation_id,
                    media,
                    --count,
                    ss->user_data,
                    NULL);
      iter = g_list_next (iter);
    }
    g_list_free (medias);
  } else {
    ss->callback (ss->source, ss->operation_id, NULL, 0, ss->user_data, NULL);
  }
}
