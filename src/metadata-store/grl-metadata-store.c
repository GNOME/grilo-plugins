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

#define PLUGIN_ID   "grl-metadata-store"
#define PLUGIN_NAME "Metadata Store"
#define PLUGIN_DESC "A plugin for storing extra metadata information"

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
  "rating INTEGER,"					 \
  "last_position INTEGER,"				 \
  "last_played DATE)"

#define GRL_SQL_GET_METADATA				\
  "SELECT * FROM store "				\
  "WHERE source_id='%s' AND media_id='%s' "		\
  "LIMIT 1"

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

static const GList *grl_metadata_store_source_supported_keys (GrlMetadataSource *source);

static const GList *grl_metadata_store_source_key_depends (GrlMetadataSource *source,
							   GrlKeyID key_id);
static const GList *grl_metadata_store_source_writable_keys (GrlMetadataSource *source);

gboolean grl_metadata_store_source_plugin_init (GrlPluginRegistry *registry,
						const GrlPluginInfo *plugin,
						const GrlConfig *config);


/* =================== GrlMetadataStore Plugin  =============== */

gboolean
grl_metadata_store_source_plugin_init (GrlPluginRegistry *registry,
                                      const GrlPluginInfo *plugin,
                                      const GrlConfig *config)
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
                     PLUGIN_ID,
                     PLUGIN_NAME,
                     PLUGIN_DESC,
                     PACKAGE_VERSION,
                     AUTHOR,
                     LICENSE,
                     SITE);

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
  memset (source->priv, 0, sizeof (GrlMetadataStorePrivate));

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
  gint rating, play_count, last_position;
  gchar *last_played, *rating_str;
  gint r;

  while ((r = sqlite3_step (stmt)) == SQLITE_BUSY);

  if (r != SQLITE_ROW) {
    /* No info in DB for this item, bail out silently */
    sqlite3_finalize (stmt);
    return;
  }

  iter = keys;
  while (iter) {
    GrlKeyID key_id = POINTER_TO_GRLKEYID (iter->data);
    switch (key_id) {
    case GRL_METADATA_KEY_PLAY_COUNT:
      play_count = sqlite3_column_int (stmt, STORE_PLAY_COUNT);
      grl_media_set_play_count (media, play_count);
      break;
    case GRL_METADATA_KEY_RATING:
      rating = sqlite3_column_int (stmt, STORE_RATING);
      rating_str = g_strdup_printf ("%d", rating);
      grl_media_set_rating (media, rating_str, "5");
      g_free (rating_str);
      break;
    case GRL_METADATA_KEY_LAST_PLAYED:
      last_played = (gchar *) sqlite3_column_text (stmt, STORE_LAST_PLAYED);
      grl_media_set_last_played (media, last_played);
      break;
    case GRL_METADATA_KEY_LAST_POSITION:
      last_position = sqlite3_column_int (stmt, STORE_LAST_POSITION);
      grl_media_set_last_position (media, last_position);
      break;
    default:
      break;
    }
    iter = g_list_next (iter);
  }

  sqlite3_finalize (stmt);
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

  switch (key_id) {
  case GRL_METADATA_KEY_RATING:
  case GRL_METADATA_KEY_PLAY_COUNT:
  case GRL_METADATA_KEY_LAST_PLAYED:
  case GRL_METADATA_KEY_LAST_POSITION:
    return deps;
  default:
    break;
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
