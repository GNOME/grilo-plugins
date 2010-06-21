/*
 * Copyright (C) 2010 Igalia S.L.
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

#include <grilo.h>
#include <sqlite3.h>
#include <string.h>

#include "grl-metadata-store.h"

#define GRL_METADATA_STORE_GET_PRIVATE(object)			 \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),			 \
                               GRL_METADATA_STORE_SOURCE_TYPE,	 \
                               GrlMetadataStorePrivate))

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "grl-metadata-store"

#define PLUGIN_ID   METADATA_STORE_PLUGIN_ID

#define SOURCE_ID   "grl-metadata-store"
#define SOURCE_NAME "Metadata Store"
#define SOURCE_DESC "A plugin for storing extra metadata information"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

#define GRL_SQL_DB        ".grl-metadata-store"

#define GRL_SQL_CREATE_TABLE_STORE			 \
  "CREATE TABLE IF NOT EXISTS store ("			 \
  "source_id TEXT,"					 \
  "media_id TEXT,"					 \
  "play_count INTEGER,"					 \
  "rating REAL,"					 \
  "last_position INTEGER,"				 \
  "last_played DATE)"

#define GRL_SQL_GET_METADATA				\
  "SELECT * FROM store "				\
  "WHERE source_id='%s' AND media_id='%s' "		\
  "LIMIT 1"

#define GRL_SQL_UPDATE_METADATA			\
  "UPDATE store SET %s "			\
  "WHERE source_id=? AND media_id=?"

#define GRL_SQL_INSERT_METADATA			\
  "INSERT INTO store "				\
  "(%s source_id, media_id) VALUES "		\
  "(%s ?, ?)"

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
};

static GrlMetadataStoreSource *grl_metadata_store_source_new (void);

static void grl_metadata_store_source_resolve (GrlMetadataSource *source,
					       GrlMetadataSourceResolveSpec *rs);

static void grl_metadata_store_source_set_metadata (GrlMetadataSource *source,
						    GrlMetadataSourceSetMetadataSpec *sms);

static const GList *grl_metadata_store_source_supported_keys (GrlMetadataSource *source);

static const GList *grl_metadata_store_source_key_depends (GrlMetadataSource *source,
							   GrlKeyID key_id);
static const GList *grl_metadata_store_source_writable_keys (GrlMetadataSource *source);

gboolean grl_metadata_store_source_plugin_init (GrlPluginRegistry *registry,
						const GrlPluginInfo *plugin,
						GList *configs);


/* =================== GrlMetadataStore Plugin  =============== */

gboolean
grl_metadata_store_source_plugin_init (GrlPluginRegistry *registry,
                                      const GrlPluginInfo *plugin,
                                      GList *configs)
{
  g_debug ("grl_metadata_store_source_plugin_init");
  GrlMetadataStoreSource *source = grl_metadata_store_source_new ();
  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source));
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_metadata_store_source_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== GrlMetadataStore GObject ================ */

static GrlMetadataStoreSource *
grl_metadata_store_source_new (void)
{
  g_debug ("grl_metadata_store_source_new");
  return g_object_new (GRL_METADATA_STORE_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       NULL);
}

static void
grl_metadata_store_source_class_init (GrlMetadataStoreSourceClass * klass)
{
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);
  metadata_class->supported_keys = grl_metadata_store_source_supported_keys;
  metadata_class->key_depends = grl_metadata_store_source_key_depends;
  metadata_class->writable_keys = grl_metadata_store_source_writable_keys;
  metadata_class->resolve = grl_metadata_store_source_resolve;
  metadata_class->set_metadata = grl_metadata_store_source_set_metadata;

  g_type_class_add_private (klass, sizeof (GrlMetadataStorePrivate));
}

static void
grl_metadata_store_source_init (GrlMetadataStoreSource *source)
{
  gint r;
  const gchar *home;
  gchar *db_path;
  gchar *sql_error = NULL;

  source->priv = GRL_METADATA_STORE_GET_PRIVATE (source);

  home = g_getenv ("HOME");
  if (!home) {
    g_warning ("$HOME not set, cannot open database");
    return;
  }

  g_debug ("Opening database connection...");
  db_path = g_strconcat (home, G_DIR_SEPARATOR_S, GRL_SQL_DB, NULL);
  r = sqlite3_open (db_path, &source->priv->db);
  if (r) {
    g_critical ("Failed to open database '%s': %s",
		db_path, sqlite3_errmsg (source->priv->db));
    sqlite3_close (source->priv->db);
    return;
  }
  g_debug ("  OK");

  g_debug ("Checking database tables...");
  r = sqlite3_exec (source->priv->db, GRL_SQL_CREATE_TABLE_STORE,
		    NULL, NULL, &sql_error);

  if (r) {
    if (sql_error) {
      g_warning ("Failed to create database tables: %s", sql_error);
      sqlite3_free (sql_error);
      sql_error = NULL;
    } else {
      g_warning ("Failed to create database tables.");
    }
    sqlite3_close (source->priv->db);
    return;
  }
  g_debug ("  OK");

  g_free (db_path);
}

G_DEFINE_TYPE (GrlMetadataStoreSource, grl_metadata_store_source,
               GRL_TYPE_METADATA_SOURCE);

/* ======================= Utilities ==================== */

static sqlite3_stmt *
query_metadata_store (sqlite3 *db,
		      const gchar *source_id,
		      const gchar *media_id)
{
  gint r;
  sqlite3_stmt *sql_stmt = NULL;
  gchar *sql;

  g_debug ("get_metadata");

  sql = g_strdup_printf (GRL_SQL_GET_METADATA, source_id, media_id);
  g_debug ("%s", sql);
  r = sqlite3_prepare_v2 (db, sql, strlen (sql), &sql_stmt, NULL);
  g_free (sql);

  if (r != SQLITE_OK) {
    g_warning ("Failed to get metadata: %s", sqlite3_errmsg (db));
    return NULL;
  }

  return sql_stmt;
}

static void
fill_metadata (GrlMedia *media, GList *keys, sqlite3_stmt *stmt)
{
  GList *iter;
  gint play_count, last_position;
  gdouble rating;
  gchar *last_played;
  gint r;

  while ((r = sqlite3_step (stmt)) == SQLITE_BUSY);

  if (r != SQLITE_ROW) {
    /* No info in DB for this item, bail out silently */
    sqlite3_finalize (stmt);
    return;
  }

  iter = keys;
  while (iter) {
    if (iter->data == GRL_METADATA_KEY_PLAY_COUNT) {
      play_count = sqlite3_column_int (stmt, STORE_PLAY_COUNT);
      grl_media_set_play_count (media, play_count);
    } else if (iter->data == GRL_METADATA_KEY_RATING) {
      rating = sqlite3_column_double (stmt, STORE_RATING);
      grl_media_set_rating (media, rating, 5.00);
    } else if (iter->data == GRL_METADATA_KEY_LAST_PLAYED) {
      last_played = (gchar *) sqlite3_column_text (stmt, STORE_LAST_PLAYED);
      grl_media_set_last_played (media, last_played);
    } else if (iter->data == GRL_METADATA_KEY_LAST_POSITION) {
      last_position = sqlite3_column_int (stmt, STORE_LAST_POSITION);
      grl_media_set_last_position (media, last_position);
    }
    iter = g_list_next (iter);
  }

  sqlite3_finalize (stmt);
}

static const gchar *
get_column_name_from_key_id (GrlKeyID key_id)
{
  static const gchar *col_names[] = {"rating", "last_played", "last_position",
				     "play_count"};
  if (key_id == GRL_METADATA_KEY_RATING) {
    return col_names[0];
  } else if (key_id == GRL_METADATA_KEY_LAST_PLAYED) {
    return col_names[1];
  } else if (key_id == GRL_METADATA_KEY_LAST_POSITION) {
    return col_names[2];
  } else if (key_id == GRL_METADATA_KEY_PLAY_COUNT) {
    return col_names[3];
  } else {
    return NULL;
  }
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
  const gchar *char_value;
  gint int_value;
  double double_value;
  GList *iter_names, *iter_keys;
  guint count;
  sqlite3_stmt *stmt;

  /* Create statement from sql */
  g_debug ("%s", sql);
  r = sqlite3_prepare_v2 (db, sql, strlen (sql), &stmt, NULL);

  if (r != SQLITE_OK) {
    g_warning ("Failed to update metadata for '%s - %s': %s",
	       source_id, media_id, sqlite3_errmsg (db));
    sqlite3_finalize (stmt);
    return FALSE;
  }

  /* Bind column values */
  count = 1;
  iter_names = col_names;
  iter_keys = keys;
  while (iter_names) {
    if (iter_names->data) {
      if (iter_keys->data == GRL_METADATA_KEY_RATING) {
	double_value = grl_media_get_rating (media);
	sqlite3_bind_double (stmt, count, double_value);
      } else if (iter_keys->data == GRL_METADATA_KEY_PLAY_COUNT) {
	int_value = grl_media_get_play_count (media);
	sqlite3_bind_int (stmt, count, int_value);
      } else if (iter_keys->data == GRL_METADATA_KEY_LAST_POSITION) {
	int_value = grl_media_get_last_position (media);
	sqlite3_bind_int (stmt, count, int_value);
      } else if (iter_keys->data == GRL_METADATA_KEY_LAST_PLAYED) {
	char_value = grl_media_get_last_played (media);
	sqlite3_bind_text (stmt, count, char_value, -1, SQLITE_STATIC);
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
  guint count;

  g_debug ("prepare_and_exec_update");

  /* Prepare sql "set" for update query */
  count = 0;
  sql_buf = g_string_new ("");
  iter_names = col_names;
  while (iter_names) {
    gchar *col_name = (gchar *) iter_names->data;
    if (col_name) {
      if (count > 0) {
	g_string_append (sql_buf, " AND ");
      }
      g_string_append_printf (sql_buf, "%s=?", col_name);
      count++;
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

  g_debug ("prepare_and_exec_insert");

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
	    GrlMetadataSourceSetMetadataSpec *sms,
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
    const gchar *col_name = get_column_name_from_key_id (iter->data);
    if (!col_name) {
      g_warning ("Key %" GRL_KEYID_FORMAT " is not supported for writing, ignoring...",
                 iter->data);
      failed_keys = g_list_prepend (failed_keys, iter->data);
    } else {
      supported_keys++;
    }
    col_names = g_list_prepend (col_names, (gchar *) col_name);
    iter = g_list_next (iter);
  }
  col_names = g_list_reverse (col_names);

  if (supported_keys == 0) {
    g_warning ("Failed to update metadata, none of the specified "
	       "keys is writable");
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_SET_METADATA_FAILED,
			  "Failed to update metadata, "
			  "specified keys are not writable");
    goto done;
  }

  r = prepare_and_exec_update (db,
			       source_id,
			       media_id,
			       col_names,
			       sms->keys,
			       sms->media);
    
  if (!r) {
    g_warning ("Failed to update metadata for '%s - %s': %s",
	       source_id, media_id, sqlite3_errmsg (db));
    g_list_free (failed_keys);
    failed_keys = g_list_copy (sms->keys);
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_SET_METADATA_FAILED,
			  "Failed to update metadata");
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
    g_warning ("Failed to update metadata for '%s - %s': %s",
	       source_id, media_id, sqlite3_errmsg (db));
    g_list_free (failed_keys);
    failed_keys = g_list_copy (sms->keys);
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_SET_METADATA_FAILED,
			  "Failed to update metadata");
    goto done;
  } 

 done:
  g_list_free (col_names);
  return failed_keys;
}

/* ================== API Implementation ================ */

static const GList *
grl_metadata_store_source_supported_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_RATING,
                                      GRL_METADATA_KEY_PLAY_COUNT,
                                      GRL_METADATA_KEY_LAST_PLAYED,
                                      GRL_METADATA_KEY_LAST_POSITION,
                                      NULL);
  }
  return keys;
}

static const GList *
grl_metadata_store_source_writable_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_RATING,
                                      GRL_METADATA_KEY_PLAY_COUNT,
                                      GRL_METADATA_KEY_LAST_PLAYED,
                                      GRL_METADATA_KEY_LAST_POSITION,
                                      NULL);
  }
  return keys;
}

static const GList *
grl_metadata_store_source_key_depends (GrlMetadataSource *source,
				       GrlKeyID key_id)
{
  static GList *deps = NULL;
  if (!deps) {
    deps = grl_metadata_key_list_new (GRL_METADATA_KEY_ID, NULL);
  }

  if (key_id == GRL_METADATA_KEY_RATING ||
      key_id == GRL_METADATA_KEY_PLAY_COUNT ||
      key_id == GRL_METADATA_KEY_LAST_PLAYED ||
      key_id == GRL_METADATA_KEY_LAST_POSITION) {
    return deps;
  }

  return NULL;
}

static void
grl_metadata_store_source_resolve (GrlMetadataSource *source,
				   GrlMetadataSourceResolveSpec *rs)
{
  g_debug ("grl_metadata_store_source_resolve");

  const gchar *source_id, *media_id;
  sqlite3_stmt *stmt;
  GError *error = NULL;

  source_id = grl_media_get_source (rs->media);
  media_id = grl_media_get_id (rs->media);

  /* We need the source id */
  if (!source_id) {
    g_warning ("Failed to resolve metadata: source-id not available");
    error = g_error_new (GRL_ERROR,
			 GRL_ERROR_RESOLVE_FAILED,
			 "source-id not available, cannot resolve metadata.");
    rs->callback (rs->source, rs->media, rs->user_data, error);
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
    rs->callback (rs->source, rs->media, rs->user_data, NULL);
  } else {
    g_warning ("Failed to resolve metadata");
    error = g_error_new (GRL_ERROR,
			 GRL_ERROR_RESOLVE_FAILED,
			 "Failed to resolve metadata.");
    rs->callback (rs->source, rs->media, rs->user_data, error);
    g_error_free (error);
  }
}

static void
grl_metadata_store_source_set_metadata (GrlMetadataSource *source,
					GrlMetadataSourceSetMetadataSpec *sms)
{
  g_debug ("grl_metadata_store_source_set_metadata");

  const gchar *media_id, *source_id;
  GError *error = NULL;
  GList *failed_keys = NULL;

  source_id = grl_media_get_source (sms->media);
  media_id = grl_media_get_id (sms->media);

  /* We need the source id */
  if (!source_id) {
    g_warning ("Failed to update metadata: source-id not available");
    error = g_error_new (GRL_ERROR,
			 GRL_ERROR_SET_METADATA_FAILED,
			 "source-id not available, cannot update metadata.");
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

  if (error) {
    g_error_free (error);
  }
  g_list_free (failed_keys);
}
