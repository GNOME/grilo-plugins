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

#include <media-store.h>
#include <gio/gio.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <sqlite3.h>
#include <string.h>
#include <stdlib.h>

#include "ms-podcasts.h"

#define MS_PODCASTS_GET_PRIVATE(object)				\
  (G_TYPE_INSTANCE_GET_PRIVATE((object), MS_PODCASTS_SOURCE_TYPE, MsPodcastsPrivate))

/* --------- Logging  -------- */ 

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "ms-podcasts"

/* --- Database --- */

#define MS_SQL_DB        ".ms-podcasts"

#define MS_SQL_CREATE_TABLE					\
  "CREATE TABLE IF NOT EXISTS podcasts ("			\
  "id    INTEGER  PRIMARY KEY AUTOINCREMENT,"			\
  "title TEXT,"							\
  "url   TEXT,"							\
  "desc  TEXT)"

#define MS_SQL_GET_PODCASTS			\
  "SELECT * FROM podcasts LIMIT %u OFFSET %u"

#define MS_SQL_GET_PODCASTS_BY_TEXT			\
  "SELECT * FROM podcasts "				\
  "WHERE title LIKE '%%%s%%' OR desc LIKE '%%%s%%' "	\
  "LIMIT %u OFFSET %u"

#define MS_SQL_GET_PODCASTS_BY_QUERY			\
  "SELECT * FROM podcasts "				\
  "WHERE %s "						\
  "LIMIT %u OFFSET %u"

#define MS_SQL_GET_PODCAST_BY_ID			\
  "SELECT * FROM podcasts "				\
  "WHERE id='%s' "						\
  "LIMIT 1"

enum {
  ID = 0,
  TITLE,
  URL,
  DESC,
};

/* --- Plugin information --- */

#define PLUGIN_ID   "ms-podcasts"
#define PLUGIN_NAME "Podcasts"
#define PLUGIN_DESC "A plugin for browsing podcasts"

#define SOURCE_ID   "ms-podcasts"
#define SOURCE_NAME "Podcasts"
#define SOURCE_DESC "A source for browsing podcasts"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

struct _MsPodcastsPrivate {
  sqlite3 *db;
};


struct OperationSpec {
  MsMediaSource *source;
  guint operation_id;
  guint skip;
  guint count;
  const gchar *text;
  MsMediaSourceResultCb callback;
  guint error_code;
  gboolean is_query;
  gpointer user_data;
};

static MsPodcastsSource *ms_podcasts_source_new (void);

static void ms_podcasts_source_finalize (GObject *plugin);

static const GList *ms_podcasts_source_supported_keys (MsMetadataSource *source);

static void ms_podcasts_source_browse (MsMediaSource *source,
				      MsMediaSourceBrowseSpec *bs);
static void ms_podcasts_source_search (MsMediaSource *source,
				       MsMediaSourceSearchSpec *ss);
static void ms_podcasts_source_query (MsMediaSource *source,
				      MsMediaSourceQuerySpec *qs);
static void ms_podcasts_source_metadata (MsMediaSource *source,
					 MsMediaSourceMetadataSpec *ms);

/* =================== Podcasts Plugin  =============== */

static gboolean
ms_podcasts_plugin_init (MsPluginRegistry *registry, const MsPluginInfo *plugin)
{
  g_debug ("podcasts_plugin_init\n");

  MsPodcastsSource *source = ms_podcasts_source_new ();
  ms_plugin_registry_register_source (registry, plugin, MS_MEDIA_PLUGIN (source));
  return TRUE;
}

MS_PLUGIN_REGISTER (ms_podcasts_plugin_init, 
                    NULL, 
                    PLUGIN_ID,
                    PLUGIN_NAME, 
                    PLUGIN_DESC, 
                    PACKAGE_VERSION,
                    AUTHOR, 
                    LICENSE, 
                    SITE);

/* ================== Podcasts GObject ================ */

static MsPodcastsSource *
ms_podcasts_source_new (void)
{
  g_debug ("ms_podcasts_source_new");
  return g_object_new (MS_PODCASTS_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       NULL);
}

static void
ms_podcasts_source_class_init (MsPodcastsSourceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  MsMediaSourceClass *source_class = MS_MEDIA_SOURCE_CLASS (klass);
  MsMetadataSourceClass *metadata_class = MS_METADATA_SOURCE_CLASS (klass);

  gobject_class->finalize = ms_podcasts_source_finalize;

  source_class->browse = ms_podcasts_source_browse;
  source_class->search = ms_podcasts_source_search;
  source_class->query = ms_podcasts_source_query;
  source_class->metadata = ms_podcasts_source_metadata;

  metadata_class->supported_keys = ms_podcasts_source_supported_keys;

  g_type_class_add_private (klass, sizeof (MsPodcastsPrivate));
}

static void
ms_podcasts_source_init (MsPodcastsSource *source)
{
  gint r;
  const gchar *home;
  gchar *db_path;
  gchar *sql_error = NULL;

  source->priv = MS_PODCASTS_GET_PRIVATE (source);
  memset (source->priv, 0, sizeof (MsPodcastsPrivate));

  home = g_getenv ("HOME");
  if (!home) {
    g_warning ("$HOME not set, cannot open database");
    return;
  }

  g_debug ("Opening database connection...");
  db_path = g_strconcat (home, G_DIR_SEPARATOR_S, MS_SQL_DB, NULL);
  r = sqlite3_open (db_path, &source->priv->db);
  if (r) {
    g_critical ("Failed to open database '%s': %s",
		db_path, sqlite3_errmsg (source->priv->db));
    sqlite3_close (source->priv->db);
    return;
  }
  g_debug ("  OK");

  g_debug ("Checking database tables...");
  r = sqlite3_exec (source->priv->db, MS_SQL_CREATE_TABLE,
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

G_DEFINE_TYPE (MsPodcastsSource, ms_podcasts_source, MS_TYPE_MEDIA_SOURCE);

static void
ms_podcasts_source_finalize (GObject *object)
{
  MsPodcastsSource *source;
  
  g_debug ("ms_podcasts_source_finalize");

  source = MS_PODCASTS_SOURCE (object);

  sqlite3_close (source->priv->db);

  G_OBJECT_CLASS (ms_podcasts_source_parent_class)->finalize (object);
}

/* ======================= Utilities ==================== */

static MsContentMedia *
build_media_from_stmt (MsContentMedia *content, sqlite3_stmt *sql_stmt)
{
  MsContentMedia *media;

  if (!content) {
    media = ms_content_audio_new ();
  } else {
    media = content;
  }
  
  ms_content_media_set_id (media, 
			   (gchar *) sqlite3_column_text (sql_stmt, ID));
  ms_content_media_set_title (media,
			      (gchar *) sqlite3_column_text (sql_stmt, TITLE));
  ms_content_media_set_url (media,
			    (gchar *) sqlite3_column_text (sql_stmt, URL));

  return media;
}

static void
produce_podcasts (struct OperationSpec *os)
{
  gint r;
  sqlite3_stmt *sql_stmt = NULL;
  sqlite3 *db;
  MsContentMedia *media;
  GError *error = NULL;
  GList *medias = NULL;
  guint count = 0;
  GList *iter;
  gchar *sql;

  db = MS_PODCASTS_SOURCE (os->source)->priv->db;

  if (!os->text) {
    /* Browse */
    sql = g_strdup_printf (MS_SQL_GET_PODCASTS, os->count, os->skip);
  } else if (os->is_query) {
    /* Query */
    sql = g_strdup_printf (MS_SQL_GET_PODCASTS_BY_QUERY,
			   os->text, os->count, os->skip);
  } else {
    /* Search */
    sql = g_strdup_printf (MS_SQL_GET_PODCASTS_BY_TEXT,
			   os->text, os->text, os->count, os->skip);
  }
  g_debug ("%s", sql);
  r = sqlite3_prepare_v2 (db, sql, strlen (sql), &sql_stmt, NULL); 
  g_free (sql);

  if (r != SQLITE_OK) {
    g_warning ("Failed to retrieve podcasts: %s", sqlite3_errmsg (db));
    error = g_error_new (MS_ERROR,
			 os->error_code,
			 "Failed to retrieve podcasts list");
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
    return;
  }
  
  while ((r = sqlite3_step (sql_stmt)) == SQLITE_BUSY);

  while (r == SQLITE_ROW) {
    media = build_media_from_stmt (NULL, sql_stmt);
    medias = g_list_prepend (medias, media);
    count++;
    r = sqlite3_step (sql_stmt);
  }

  sqlite3_finalize (sql_stmt);

  if (count > 0) {
    medias = g_list_reverse (medias);
    iter = medias;
    while (iter) {
      media = MS_CONTENT_MEDIA (iter->data);
      os->callback (os->source,
		    os->operation_id,
		    media,
		    --count,
		    os->user_data,
		    NULL);
      iter = g_list_next (iter);
    }
    g_list_free (medias);
  } else {
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, NULL);
  }
}

static void
podcast_metadata (MsMediaSourceMetadataSpec *ms)
{
  gint r;
  sqlite3_stmt *sql_stmt = NULL;
  sqlite3 *db;
  GError *error = NULL;
  gchar *sql;
  const gchar *id;

  db = MS_PODCASTS_SOURCE (ms->source)->priv->db;

  id = ms_content_media_get_id (ms->media);
  if (!id) {
    /* Root category: special case */
    ms_content_media_set_title (ms->media, "");
    ms->callback (ms->source, ms->media, ms->user_data, NULL);
    return;
  }

  sql = g_strdup_printf (MS_SQL_GET_PODCAST_BY_ID, id);
  g_debug ("%s", sql);
  r = sqlite3_prepare_v2 (db, sql, strlen (sql), &sql_stmt, NULL); 
  g_free (sql);

  if (r != SQLITE_OK) {
    g_warning ("Failed to get podcast: %s", sqlite3_errmsg (db));
    error = g_error_new (MS_ERROR,
			 MS_ERROR_METADATA_FAILED,
			 "Failed to get podcast metadata");
    ms->callback (ms->source, ms->media, ms->user_data, error);
    g_error_free (error);
    return;
  }
  
  while ((r = sqlite3_step (sql_stmt)) == SQLITE_BUSY);

  if (r == SQLITE_ROW) {
    build_media_from_stmt (ms->media, sql_stmt);
    ms->callback (ms->source, ms->media, ms->user_data, NULL);
  } else {
    g_warning ("Failed to get podcast: %s", sqlite3_errmsg (db));
    error = g_error_new (MS_ERROR,
			 MS_ERROR_METADATA_FAILED,
			 "Failed to get podcast metadata");
    ms->callback (ms->source, ms->media, ms->user_data, error);
    g_error_free (error);
  }

  sqlite3_finalize (sql_stmt);
}

/* ================== API Implementation ================ */

static const GList *
ms_podcasts_source_supported_keys (MsMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = ms_metadata_key_list_new (MS_METADATA_KEY_ID,
				     MS_METADATA_KEY_TITLE, 
				     MS_METADATA_KEY_URL,
				     MS_METADATA_KEY_CHILDCOUNT,
				     MS_METADATA_KEY_SITE,
				     NULL);
  }
  return keys;
}

static void
ms_podcasts_source_browse (MsMediaSource *source, MsMediaSourceBrowseSpec *bs)
{
  g_debug ("ms_podcasts_source_browse");
  struct OperationSpec *os;
  MsPodcastsSource *podcasts_source;
  GError *error = NULL;

  podcasts_source = MS_PODCASTS_SOURCE (source);
  if (!podcasts_source->priv->db) {
    g_warning ("Can't execute operation: no database connection.");
    error = g_error_new (MS_ERROR,
			 MS_ERROR_BROWSE_FAILED,
			 "No database connection");
    bs->callback (bs->source, bs->browse_id, NULL, 0, bs->user_data, error);
    g_error_free (error);
  }

  os = g_new0 (struct OperationSpec, 1);
  os->source = bs->source;
  os->operation_id = bs->browse_id;
  os->count = bs->count;
  os->skip = bs->skip;
  os->callback = bs->callback;
  os->user_data = bs->user_data;
  os->error_code = MS_ERROR_BROWSE_FAILED;
  produce_podcasts (os);
  g_free (os);
}

static void
ms_podcasts_source_search (MsMediaSource *source, MsMediaSourceSearchSpec *ss)
{
  g_debug ("ms_podcasts_source_search");

  MsPodcastsSource *podcasts_source;
  struct OperationSpec *os;
  GError *error = NULL;

  podcasts_source = MS_PODCASTS_SOURCE (source);
  if (!podcasts_source->priv->db) {
    g_warning ("Can't execute operation: no database connection.");
    error = g_error_new (MS_ERROR,
			 MS_ERROR_QUERY_FAILED,
			 "No database connection");
    ss->callback (ss->source, ss->search_id, NULL, 0, ss->user_data, error);
    g_error_free (error);
  }

  os = g_new0 (struct OperationSpec, 1);
  os->source = ss->source;
  os->operation_id = ss->search_id;
  os->text = ss->text;
  os->count = ss->count;
  os->skip = ss->skip;
  os->callback = ss->callback;
  os->user_data = ss->user_data;
  os->error_code = MS_ERROR_SEARCH_FAILED;
  produce_podcasts (os);
  g_free (os);
}

static void
ms_podcasts_source_query (MsMediaSource *source, MsMediaSourceQuerySpec *qs)
{
  g_debug ("ms_podcasts_source_query");

  MsPodcastsSource *podcasts_source;
  struct OperationSpec *os;
  GError *error = NULL;

  podcasts_source = MS_PODCASTS_SOURCE (source);
  if (!podcasts_source->priv->db) {
    g_warning ("Can't execute operation: no database connection.");
    error = g_error_new (MS_ERROR,
			 MS_ERROR_QUERY_FAILED,
			 "No database connection");
    qs->callback (qs->source, qs->query_id, NULL, 0, qs->user_data, error);
    g_error_free (error);
  }

  os = g_new0 (struct OperationSpec, 1);
  os->source = qs->source;
  os->operation_id = qs->query_id;
  os->text = qs->query;
  os->count = qs->count;
  os->skip = qs->skip;
  os->callback = qs->callback;
  os->user_data = qs->user_data;
  os->is_query = TRUE;
  os->error_code = MS_ERROR_SEARCH_FAILED;
  produce_podcasts (os);
  g_free (os);
}

static void
ms_podcasts_source_metadata (MsMediaSource *source, MsMediaSourceMetadataSpec *ms)
{
  g_debug ("ms_podcasts_source_metadata");

  MsPodcastsSource *podcasts_source;
  GError *error = NULL;

  podcasts_source = MS_PODCASTS_SOURCE (source);
  if (!podcasts_source->priv->db) {
    g_warning ("Can't execute operation: no database connection.");
    error = g_error_new (MS_ERROR,
			 MS_ERROR_METADATA_FAILED,
			 "No database connection");
    ms->callback (ms->source, ms->media, ms->user_data, error);
    g_error_free (error);
  }

  podcast_metadata (ms);
}
