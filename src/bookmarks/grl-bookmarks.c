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

/**
 * SECTION:grl-bookmarks
 * @short_description: A Grilo Plugin for bookmarks
 *
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <grilo.h>
#include <sqlite3.h>
#include <string.h>
#include <stdlib.h>

#include "grl-bookmarks.h"

#define GRL_BOOKMARKS_GET_PRIVATE(object)			 \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),			 \
                               GRL_BOOKMARKS_SOURCE_TYPE,        \
                               GrlBookmarksPrivate))

#define GRL_ROOT_TITLE "Bookmarks"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT bookmarks_log_domain
GRL_LOG_DOMAIN_STATIC(bookmarks_log_domain);

/* --- Database --- */

#define GRL_SQL_DB "grl-bookmarks.db"

#define GRL_SQL_CREATE_TABLE_BOOKMARKS			 \
  "CREATE TABLE IF NOT EXISTS bookmarks ("		 \
  "id     INTEGER PRIMARY KEY AUTOINCREMENT,"		 \
  "parent INTEGER REFERENCES bookmarks (id),"		 \
  "type   INTEGER,"					 \
  "url    TEXT,"					 \
  "title  TEXT,"					 \
  "date   TEXT,"					 \
  "mime   TEXT,"					 \
  "desc   TEXT)"

#define GRL_SQL_GET_BOOKMARKS_BY_PARENT			\
  "SELECT b1.*, count(b2.parent <> '') "		\
  "FROM bookmarks b1 LEFT OUTER JOIN bookmarks b2 "	\
  "  ON b1.id = b2.parent "				\
  "WHERE b1.parent='%s' "				\
  "GROUP BY b1.id "					\
  "LIMIT %u OFFSET %u"

#define GRL_SQL_GET_BOOKMARK_BY_ID			\
  "SELECT b1.*, count(b2.parent <> '') "		\
  "FROM bookmarks b1 LEFT OUTER JOIN bookmarks b2 "	\
  "  ON b1.id = b2.parent "				\
  "WHERE b1.id='%s' "					\
  "GROUP BY b1.id "					\
  "LIMIT 1"

#define GRL_SQL_STORE_BOOKMARK				  \
  "INSERT INTO bookmarks "				  \
  "(parent, type, url, title, date, mime, desc) "	  \
  "VALUES (?, ?, ?, ?, ?, ?, ?)"

#define GRL_SQL_REMOVE_BOOKMARK			\
  "DELETE FROM bookmarks "			\
  "WHERE id='%s' or parent='%s'"

#define GRL_SQL_REMOVE_ORPHAN			\
  "DELETE FROM bookmarks "			\
  "WHERE id in ( "				\
  "  SELECT DISTINCT id FROM bookmarks "	\
  "  WHERE parent NOT IN ( "			\
  "    SELECT DISTINCT id FROM bookmarks) "	\
  "  and parent <> 0)"

#define GRL_SQL_GET_BOOKMARKS_BY_TEXT				\
  "SELECT b1.*, count(b2.parent <> '') "			\
  "FROM bookmarks b1 LEFT OUTER JOIN bookmarks b2 "		\
  "  ON b1.id = b2.parent "					\
  "WHERE (b1.title LIKE '%%%s%%' OR b1.desc LIKE '%%%s%%') "	\
  "  AND b1.type = 1 "                                          \
  "GROUP BY b1.id "						\
  "LIMIT %u OFFSET %u"

#define GRL_SQL_GET_BOOKMARKS_BY_QUERY				\
  "SELECT b1.*, count(b2.parent <> '') "			\
  "FROM bookmarks b1 LEFT OUTER JOIN bookmarks b2 "		\
  "  ON b1.id = b2.parent "					\
  "WHERE %s "							\
  "GROUP BY b1.id "						\
  "LIMIT %u OFFSET %u"

/* --- Plugin information --- */

#define PLUGIN_ID   BOOKMARKS_PLUGIN_ID

#define SOURCE_ID   "grl-bookmarks"
#define SOURCE_NAME "Bookmarks"
#define SOURCE_DESC "A source for organizing media bookmarks"

GrlKeyID GRL_BOOKMARKS_KEY_BOOKMARK_TIME = 0;

enum {
  BOOKMARK_TYPE_CATEGORY = 0,
  BOOKMARK_TYPE_STREAM,
};

enum {
  BOOKMARK_ID = 0,
  BOOKMARK_PARENT,
  BOOKMARK_TYPE,
  BOOKMARK_URL,
  BOOKMARK_TITLE,
  BOOKMARK_DATE,
  BOOKMARK_MIME,
  BOOKMARK_DESC,
  BOOKMARK_CHILDCOUNT
};

struct _GrlBookmarksPrivate {
  sqlite3 *db;
  gboolean notify_changes;
};

typedef struct {
  GrlSource *source;
  guint operation_id;
  const gchar *media_id;
  guint skip;
  guint count;
  GrlSourceResultCb callback;
  guint error_code;
  gboolean is_query;
  gpointer user_data;
} OperationSpec;

static GrlBookmarksSource *grl_bookmarks_source_new (void);

static void grl_bookmarks_source_finalize (GObject *plugin);

static const GList *grl_bookmarks_source_supported_keys (GrlSource *source);
static GrlSupportedOps grl_bookmarks_source_supported_operations (GrlSource *source);

static void grl_bookmarks_source_search (GrlSource *source,
                                         GrlSourceSearchSpec *ss);
static void grl_bookmarks_source_query (GrlSource *source,
                                        GrlSourceQuerySpec *qs);
static void grl_bookmarks_source_browse (GrlSource *source,
                                         GrlSourceBrowseSpec *bs);
static void grl_bookmarks_source_resolve (GrlSource *source,
                                          GrlSourceResolveSpec *rs);
static void grl_bookmarks_source_store (GrlSource *source,
                                        GrlSourceStoreSpec *ss);
static void grl_bookmarks_source_remove (GrlSource *source,
                                         GrlSourceRemoveSpec *rs);

static gboolean grl_bookmarks_source_notify_change_start (GrlSource *source,
                                                          GError **error);

static gboolean grl_bookmarks_source_notify_change_stop (GrlSource *source,
                                                         GError **error);

 /* =================== Bookmarks Plugin  =============== */

 static gboolean
 grl_bookmarks_plugin_init (GrlRegistry *registry,
                            GrlPlugin *plugin,
                            GList *configs)
 {
   GParamSpec *spec;
   GRL_LOG_DOMAIN_INIT (bookmarks_log_domain, "bookmarks");

   GRL_DEBUG ("grl_bookmarks_plugin_init");

   spec = g_param_spec_boxed ("bookmark-date",
                              "Bookmark date",
                              "When the media was bookmarked",
                              G_TYPE_DATE_TIME,
                              G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE),
   GRL_BOOKMARKS_KEY_BOOKMARK_TIME =
       grl_registry_register_metadata_key (registry, spec, NULL);
   /* If key was not registered, could be that it is already registered. If so,
      check if type is the expected one, and reuse it */
   if (GRL_BOOKMARKS_KEY_BOOKMARK_TIME == GRL_METADATA_KEY_INVALID) {
     g_param_spec_unref (spec);
     GRL_BOOKMARKS_KEY_BOOKMARK_TIME =
         grl_registry_lookup_metadata_key (registry, "bookmark-date");
     if (grl_metadata_key_get_type (GRL_BOOKMARKS_KEY_BOOKMARK_TIME)
         != G_TYPE_DATE_TIME) {
       GRL_BOOKMARKS_KEY_BOOKMARK_TIME = GRL_METADATA_KEY_INVALID;
     }
   }

   GrlBookmarksSource *source = grl_bookmarks_source_new ();
   grl_registry_register_source (registry,
                                 plugin,
                                 GRL_SOURCE (source),
                                 NULL);
   return TRUE;
 }

 GRL_PLUGIN_REGISTER (grl_bookmarks_plugin_init,
                      NULL,
                      PLUGIN_ID);

 /* ================== Bookmarks GObject ================ */

 static GrlBookmarksSource *
 grl_bookmarks_source_new (void)
 {
   GRL_DEBUG ("grl_bookmarks_source_new");
   return g_object_new (GRL_BOOKMARKS_SOURCE_TYPE,
                        "source-id", SOURCE_ID,
                        "source-name", SOURCE_NAME,
                        "source-desc", SOURCE_DESC,
                        NULL);
 }

 static void
 grl_bookmarks_source_class_init (GrlBookmarksSourceClass * klass)
 {
   GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
   GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

   gobject_class->finalize = grl_bookmarks_source_finalize;

   source_class->supported_operations = grl_bookmarks_source_supported_operations;
   source_class->supported_keys = grl_bookmarks_source_supported_keys;
   source_class->browse = grl_bookmarks_source_browse;
   source_class->search = grl_bookmarks_source_search;
   source_class->query = grl_bookmarks_source_query;
   source_class->store = grl_bookmarks_source_store;
   source_class->remove = grl_bookmarks_source_remove;
   source_class->resolve = grl_bookmarks_source_resolve;
   source_class->notify_change_start = grl_bookmarks_source_notify_change_start;
   source_class->notify_change_stop = grl_bookmarks_source_notify_change_stop;

   g_type_class_add_private (klass, sizeof (GrlBookmarksPrivate));
}

static void
grl_bookmarks_source_init (GrlBookmarksSource *source)
{
  gint r;
  gchar *path;
  gchar *db_path;
  gchar *sql_error = NULL;

  source->priv = GRL_BOOKMARKS_GET_PRIVATE (source);

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
  g_free (db_path);

  if (r) {
    g_critical ("Failed to open database '%s': %s",
		db_path, sqlite3_errmsg (source->priv->db));
    sqlite3_close (source->priv->db);
    return;
  }
  GRL_DEBUG ("  OK");

  GRL_DEBUG ("Checking database tables...");
  r = sqlite3_exec (source->priv->db, GRL_SQL_CREATE_TABLE_BOOKMARKS,
		    NULL, NULL, &sql_error);

  if (r) {
    if (sql_error) {
      GRL_WARNING ("Failed to create database tables: %s", sql_error);
      sqlite3_free (sql_error);
      sql_error = NULL;
    } else {
      GRL_WARNING ("Failed to create database tables.");
    }
    sqlite3_close (source->priv->db);
    return;
  }
  GRL_DEBUG ("  OK");
}

G_DEFINE_TYPE (GrlBookmarksSource, grl_bookmarks_source, GRL_TYPE_SOURCE);

static void
grl_bookmarks_source_finalize (GObject *object)
{
  GrlBookmarksSource *source;

  GRL_DEBUG ("grl_bookmarks_source_finalize");

  source = GRL_BOOKMARKS_SOURCE (object);

  sqlite3_close (source->priv->db);

  G_OBJECT_CLASS (grl_bookmarks_source_parent_class)->finalize (object);
}

/* ======================= Utilities ==================== */

static gboolean
mime_is_video (const gchar *mime)
{
  return mime && g_str_has_prefix (mime, "video/");
}

static gboolean
mime_is_audio (const gchar *mime)
{
  return mime && g_str_has_prefix (mime, "audio/");
}

static GrlMedia *
build_media_from_stmt (GrlMedia *content, sqlite3_stmt *sql_stmt)
{
  GrlMedia *media = NULL;
  gchar *id;
  gchar *title;
  gchar *url;
  gchar *desc;
  gchar *date;
  gchar *mime;
  guint type;
  guint childcount;

  if (content) {
    media = content;
  }

  id = (gchar *) sqlite3_column_text (sql_stmt, BOOKMARK_ID);
  title = (gchar *) sqlite3_column_text (sql_stmt, BOOKMARK_TITLE);
  url = (gchar *) sqlite3_column_text (sql_stmt, BOOKMARK_URL);
  desc = (gchar *) sqlite3_column_text (sql_stmt, BOOKMARK_DESC);
  date = (gchar *) sqlite3_column_text (sql_stmt, BOOKMARK_DATE);
  mime = (gchar *) sqlite3_column_text (sql_stmt, BOOKMARK_MIME);
  type = (guint) sqlite3_column_int (sql_stmt, BOOKMARK_TYPE);
  childcount = (guint) sqlite3_column_int (sql_stmt, BOOKMARK_CHILDCOUNT);

  if (!media) {
    if (type == BOOKMARK_TYPE_CATEGORY) {
      media = GRL_MEDIA (grl_media_box_new ());
    } else if (mime_is_audio (mime)) {
      media = GRL_MEDIA (grl_media_new ());
    } else if (mime_is_video (mime)) {
      media = GRL_MEDIA (grl_media_new ());
    } else {
      media = GRL_MEDIA (grl_media_new ());
    }
  }

  grl_media_set_id (media, id);
  grl_media_set_title (media, title);
  if (url) {
    grl_media_set_url (media, url);
  }
  if (desc) {
    grl_media_set_description (media, desc);
  }

  if (date) {
    GDateTime *date_time = grl_date_time_from_iso8601 (date);
    if (date_time) {
      grl_data_set_boxed (GRL_DATA (media),
                          GRL_BOOKMARKS_KEY_BOOKMARK_TIME,
                          date_time);
      g_date_time_unref (date_time);
    }
  }

  if (type == BOOKMARK_TYPE_CATEGORY) {
    grl_media_box_set_childcount (GRL_MEDIA_BOX (media), childcount);
  }

  return media;
}

static void
bookmark_resolve (GrlSourceResolveSpec *rs)
{
  gint r;
  sqlite3_stmt *sql_stmt = NULL;
  sqlite3 *db;
  GError *error = NULL;
  gchar *sql;
  const gchar *id;

  GRL_DEBUG (__FUNCTION__);

  db = GRL_BOOKMARKS_SOURCE (rs->source)->priv->db;

  id = grl_media_get_id (rs->media);
  if (!id) {
    /* Root category: special case */
    grl_media_set_title (rs->media, GRL_ROOT_TITLE);
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
    return;
  }

  sql = g_strdup_printf (GRL_SQL_GET_BOOKMARK_BY_ID, id);
  GRL_DEBUG ("%s", sql);
  r = sqlite3_prepare_v2 (db, sql, strlen (sql), &sql_stmt, NULL);
  g_free (sql);

  if (r != SQLITE_OK) {
    GRL_WARNING ("Failed to get bookmark: %s", sqlite3_errmsg (db));
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_RESOLVE_FAILED,
                         "Failed to get bookmark metadata");
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);
    g_error_free (error);
    return;
  }

  while ((r = sqlite3_step (sql_stmt)) == SQLITE_BUSY);

  if (r == SQLITE_ROW) {
    build_media_from_stmt (rs->media, sql_stmt);
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
  } else {
    GRL_WARNING ("Failed to get bookmark: %s", sqlite3_errmsg (db));
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_RESOLVE_FAILED,
                         "Failed to get bookmark metadata");
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);
    g_error_free (error);
  }

  sqlite3_finalize (sql_stmt);
}

static void
produce_bookmarks_from_sql (OperationSpec *os, const gchar *sql)
{
  gint r;
  sqlite3_stmt *sql_stmt = NULL;
  sqlite3 *db;
  GrlMedia *media;
  GError *error = NULL;
  GList *medias = NULL;
  guint count = 0;
  GList *iter;

  GRL_DEBUG ("produce_bookmarks_from_sql");

  GRL_DEBUG ("%s", sql);
  db = GRL_BOOKMARKS_SOURCE (os->source)->priv->db;
  r = sqlite3_prepare_v2 (db, sql, strlen (sql), &sql_stmt, NULL);

  if (r != SQLITE_OK) {
    GRL_WARNING ("Failed to retrieve bookmarks: %s", sqlite3_errmsg (db));
    error = g_error_new (GRL_CORE_ERROR,
			 os->error_code,
			 "Failed to retrieve bookmarks list");
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
    goto free_resources;
  }

  while ((r = sqlite3_step (sql_stmt)) == SQLITE_BUSY);

  while (r == SQLITE_ROW) {
    media = build_media_from_stmt (NULL, sql_stmt);
    medias = g_list_prepend (medias, media);
    count++;
    r = sqlite3_step (sql_stmt);
  }

  if (r != SQLITE_DONE) {
    GRL_WARNING ("Failed to retrieve bookmarks: %s", sqlite3_errmsg (db));
    error = g_error_new (GRL_CORE_ERROR,
			 os->error_code,
			 "Failed to retrieve bookmarks list");
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
    goto free_resources;
  }

  if (count > 0) {
    medias = g_list_reverse (medias);
    iter = medias;
    while (iter) {
      media = GRL_MEDIA (iter->data);
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

 free_resources:
  if (sql_stmt)
    sqlite3_finalize (sql_stmt);
}

static void
produce_bookmarks_by_query (OperationSpec *os, const gchar *query)
{
  gchar *sql;
  GRL_DEBUG ("produce_bookmarks_by_query");
  sql = g_strdup_printf (GRL_SQL_GET_BOOKMARKS_BY_QUERY,
			 query, os->count, os->skip);
  produce_bookmarks_from_sql (os, sql);
  g_free (sql);
}

static void
produce_bookmarks_by_text (OperationSpec *os, const gchar *text)
{
  gchar *sql;
  GRL_DEBUG ("produce_bookmarks_by_text");
  sql = g_strdup_printf (GRL_SQL_GET_BOOKMARKS_BY_TEXT,
			 text? text: "",
                         text? text: "",
                         os->count,
                         os->skip);
  produce_bookmarks_from_sql (os, sql);
  g_free (sql);
}

static void
produce_bookmarks_from_category (OperationSpec *os, const gchar *category_id)
{
  gchar *sql;
  GRL_DEBUG ("produce_bookmarks_from_category");
  sql = g_strdup_printf (GRL_SQL_GET_BOOKMARKS_BY_PARENT,
			 category_id, os->count, os->skip);
  produce_bookmarks_from_sql (os, sql);
  g_free (sql);
}

static void
remove_bookmark (GrlBookmarksSource *bookmarks_source,
                 const gchar *bookmark_id,
                 GError **error)
{
  gint r;
  gchar *sql_error;
  gchar *sql;

  GRL_DEBUG ("remove_bookmark");

  sql = g_strdup_printf (GRL_SQL_REMOVE_BOOKMARK, bookmark_id, bookmark_id);
  GRL_DEBUG ("%s", sql);
  r = sqlite3_exec (bookmarks_source->priv->db, sql, NULL, NULL, &sql_error);
  g_free (sql);

  if (r != SQLITE_OK) {
    GRL_WARNING ("Failed to remove bookmark '%s': %s", bookmark_id, sql_error);
    *error = g_error_new (GRL_CORE_ERROR,
			  GRL_CORE_ERROR_REMOVE_FAILED,
			  "Failed to remove bookmark");
    sqlite3_free (sql_error);
  }

  /* Remove orphan nodes from database */
  GRL_DEBUG ("%s", GRL_SQL_REMOVE_ORPHAN);
  r = sqlite3_exec (bookmarks_source->priv->db,
                    GRL_SQL_REMOVE_ORPHAN,
                    NULL, NULL, NULL);

  if (bookmarks_source->priv->notify_changes) {
    /* We can improve accuracy computing the parent container of removed
       element */
    grl_source_notify_change (GRL_SOURCE (bookmarks_source),
                              NULL,
                              GRL_CONTENT_REMOVED,
                              TRUE);
  }
}

static void
store_bookmark (GrlBookmarksSource *bookmarks_source,
                GList **keylist,
                GrlMediaBox *parent,
                GrlMedia *bookmark,
                GError **error)
{
  gint r;
  sqlite3_stmt *sql_stmt = NULL;
  const gchar *title;
  const gchar *url;
  const gchar *desc;
  GTimeVal now;
  const gchar *parent_id;
  const gchar *mime;
  gchar *date;
  guint type;
  gchar *id;

  GRL_DEBUG ("store_bookmark");

  title = grl_media_get_title (bookmark);
  url = grl_media_get_url (bookmark);
  desc = grl_media_get_description (bookmark);
  mime = grl_media_get_mime (bookmark);
  g_get_current_time (&now);
  date = g_time_val_to_iso8601 (&now);

  if (!parent) {
    parent_id = "0";
  } else {
    parent_id = grl_media_get_id (GRL_MEDIA (parent));
  }
  if (!parent_id) {
    parent_id = "0";
  }

  GRL_DEBUG ("%s", GRL_SQL_STORE_BOOKMARK);
  r = sqlite3_prepare_v2 (bookmarks_source->priv->db,
			  GRL_SQL_STORE_BOOKMARK,
			  strlen (GRL_SQL_STORE_BOOKMARK),
			  &sql_stmt, NULL);
  if (r != SQLITE_OK) {
    GRL_WARNING ("Failed to store bookmark '%s': %s", title,
                 sqlite3_errmsg (bookmarks_source->priv->db));
    *error = g_error_new (GRL_CORE_ERROR,
			  GRL_CORE_ERROR_STORE_FAILED,
			  "Failed to store bookmark '%s'", title);
    return;
  }

  GRL_DEBUG ("URL: '%s'", url);

  if (GRL_IS_MEDIA_BOX (bookmark)) {
    type = BOOKMARK_TYPE_CATEGORY;
  } else {
    type = BOOKMARK_TYPE_STREAM;
  }

  sqlite3_bind_text (sql_stmt, BOOKMARK_PARENT, parent_id, -1, SQLITE_STATIC);
  sqlite3_bind_int (sql_stmt, BOOKMARK_TYPE, type);
  if (type == BOOKMARK_TYPE_STREAM) {
    sqlite3_bind_text (sql_stmt, BOOKMARK_URL, url, -1, SQLITE_STATIC);
    *keylist = g_list_remove (*keylist,
                              GRLKEYID_TO_POINTER (GRL_METADATA_KEY_URL));
  } else {
    sqlite3_bind_null (sql_stmt, BOOKMARK_URL);
  }
  if (title) {
    sqlite3_bind_text (sql_stmt, BOOKMARK_TITLE, title, -1, SQLITE_STATIC);
    *keylist = g_list_remove (*keylist,
                              GRLKEYID_TO_POINTER (GRL_METADATA_KEY_TITLE));
  } else if (url) {
    sqlite3_bind_text (sql_stmt, BOOKMARK_TITLE, url, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_text (sql_stmt, BOOKMARK_TITLE, "(unknown)", -1, SQLITE_STATIC);
  }
  if (date) {
    sqlite3_bind_text (sql_stmt, BOOKMARK_DATE, date, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_null (sql_stmt, BOOKMARK_DATE);
  }
  if (mime) {
    sqlite3_bind_text (sql_stmt, BOOKMARK_MIME, mime, -1, SQLITE_STATIC);
    *keylist = g_list_remove (*keylist,
                              GRLKEYID_TO_POINTER (GRL_METADATA_KEY_MIME));
  } else {
    sqlite3_bind_null (sql_stmt, BOOKMARK_MIME);
  }
  if (desc) {
    sqlite3_bind_text (sql_stmt, BOOKMARK_DESC, desc, -1, SQLITE_STATIC);
    *keylist = g_list_remove (*keylist,
                              GRLKEYID_TO_POINTER (GRL_METADATA_KEY_DESCRIPTION));
  } else {
    sqlite3_bind_null (sql_stmt, BOOKMARK_DESC);
  }

  while ((r = sqlite3_step (sql_stmt)) == SQLITE_BUSY);

  if (r != SQLITE_DONE) {
    GRL_WARNING ("Failed to store bookmark '%s': %s", title,
                 sqlite3_errmsg (bookmarks_source->priv->db));
    *error = g_error_new (GRL_CORE_ERROR,
			  GRL_CORE_ERROR_STORE_FAILED,
			  "Failed to store bookmark '%s'", title);
    sqlite3_finalize (sql_stmt);
    return;
  }

  sqlite3_finalize (sql_stmt);

  id = g_strdup_printf ("%llu",
                        sqlite3_last_insert_rowid (bookmarks_source->priv->db));
  grl_media_set_id (bookmark, id);
  g_free (id);

  if (bookmarks_source->priv->notify_changes) {
    grl_source_notify_change (GRL_SOURCE (bookmarks_source),
                              GRL_MEDIA (parent),
                              GRL_CONTENT_ADDED,
                              FALSE);
  }
}

/* ================== API Implementation ================ */

static const GList *
grl_bookmarks_source_supported_keys (GrlSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_URL,
                                      GRL_METADATA_KEY_CHILDCOUNT,
                                      GRL_METADATA_KEY_DESCRIPTION,
                                      GRL_BOOKMARKS_KEY_BOOKMARK_TIME,
                                      NULL);
  }
  return keys;
}

static void
grl_bookmarks_source_browse (GrlSource *source,
                             GrlSourceBrowseSpec *bs)
{
  GRL_DEBUG ("grl_bookmarks_source_browse");

  OperationSpec *os;
  GrlBookmarksSource *bookmarks_source;
  GError *error = NULL;

  bookmarks_source = GRL_BOOKMARKS_SOURCE (source);
  if (!bookmarks_source->priv->db) {
    GRL_WARNING ("Can't execute operation: no database connection.");
    error = g_error_new (GRL_CORE_ERROR,
			 GRL_CORE_ERROR_BROWSE_FAILED,
			 "No database connection");
    bs->callback (bs->source, bs->operation_id, NULL, 0, bs->user_data, error);
    g_error_free (error);
  }

  /* Configure browse operation */
  os = g_slice_new0 (OperationSpec);
  os->source = bs->source;
  os->operation_id = bs->operation_id;
  os->media_id = grl_media_get_id (bs->container);
  os->count = grl_operation_options_get_count (bs->options);
  os->skip = grl_operation_options_get_skip (bs->options);
  os->callback = bs->callback;
  os->user_data = bs->user_data;
  os->error_code = GRL_CORE_ERROR_BROWSE_FAILED;

  produce_bookmarks_from_category (os, os->media_id ? os->media_id : "0");
  g_slice_free (OperationSpec, os);
}

static void
grl_bookmarks_source_search (GrlSource *source,
                             GrlSourceSearchSpec *ss)
{
  GRL_DEBUG ("grl_bookmarks_source_search");

  GrlBookmarksSource *bookmarks_source;
  OperationSpec *os;
  GError *error = NULL;

  bookmarks_source = GRL_BOOKMARKS_SOURCE (source);
  if (!bookmarks_source->priv->db) {
    GRL_WARNING ("Can't execute operation: no database connection.");
    error = g_error_new (GRL_CORE_ERROR,
			 GRL_CORE_ERROR_QUERY_FAILED,
			 "No database connection");
    ss->callback (ss->source, ss->operation_id, NULL, 0, ss->user_data, error);
    g_error_free (error);
  }

  os = g_slice_new0 (OperationSpec);
  os->source = ss->source;
  os->operation_id = ss->operation_id;
  os->count = grl_operation_options_get_count (ss->options);
  os->skip = grl_operation_options_get_skip (ss->options);
  os->callback = ss->callback;
  os->user_data = ss->user_data;
  os->error_code = GRL_CORE_ERROR_SEARCH_FAILED;
  produce_bookmarks_by_text (os, ss->text);
  g_slice_free (OperationSpec, os);
}

static void
grl_bookmarks_source_query (GrlSource *source,
                            GrlSourceQuerySpec *qs)
{
  GRL_DEBUG ("grl_bookmarks_source_query");

  GrlBookmarksSource *bookmarks_source;
  OperationSpec *os;
  GError *error = NULL;

  bookmarks_source = GRL_BOOKMARKS_SOURCE (source);
  if (!bookmarks_source->priv->db) {
    GRL_WARNING ("Can't execute operation: no database connection.");
    error = g_error_new (GRL_CORE_ERROR,
			 GRL_CORE_ERROR_QUERY_FAILED,
			 "No database connection");
    qs->callback (qs->source, qs->operation_id, NULL, 0, qs->user_data, error);
    g_error_free (error);
  }

  os = g_slice_new0 (OperationSpec);
  os->source = qs->source;
  os->operation_id = qs->operation_id;
  os->count = grl_operation_options_get_count (qs->options);
  os->skip = grl_operation_options_get_skip (qs->options);
  os->callback = qs->callback;
  os->user_data = qs->user_data;
  os->error_code = GRL_CORE_ERROR_SEARCH_FAILED;
  produce_bookmarks_by_query (os, qs->query);
  g_slice_free (OperationSpec, os);
}

static void
grl_bookmarks_source_store (GrlSource *source, GrlSourceStoreSpec *ss)
{
  GError *error = NULL;
  GList *keylist;

  GRL_DEBUG ("grl_bookmarks_source_store");

  /* FIXME: Try to guess bookmark mime somehow */
  keylist = grl_data_get_keys (GRL_DATA (ss->media));
  store_bookmark (GRL_BOOKMARKS_SOURCE (ss->source),
                  &keylist, ss->parent, ss->media, &error);
  ss->callback (ss->source, ss->media, keylist, ss->user_data, error);
  if (error) {
    g_error_free (error);
  }
}

static void grl_bookmarks_source_remove (GrlSource *source,
                                         GrlSourceRemoveSpec *rs)
{
  GRL_DEBUG (__FUNCTION__);
  GError *error = NULL;
  remove_bookmark (GRL_BOOKMARKS_SOURCE (rs->source),
		   rs->media_id, &error);
  rs->callback (rs->source, rs->media, rs->user_data, error);
  if (error) {
    g_error_free (error);
  }
}

static void
grl_bookmarks_source_resolve (GrlSource *source,
                              GrlSourceResolveSpec *rs)
{
  GRL_DEBUG (__FUNCTION__);

  GrlBookmarksSource *bookmarks_source;
  GError *error = NULL;

  bookmarks_source = GRL_BOOKMARKS_SOURCE (source);
  if (!bookmarks_source->priv->db) {
    GRL_WARNING ("Can't execute operation: no database connection.");
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_RESOLVE_FAILED,
                         "No database connection");
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);
    g_error_free (error);
  }

  bookmark_resolve (rs);
}

static GrlSupportedOps
grl_bookmarks_source_supported_operations (GrlSource *source)
{
  GrlSupportedOps caps;

  caps = GRL_OP_BROWSE | GRL_OP_RESOLVE | GRL_OP_SEARCH | GRL_OP_QUERY |
    GRL_OP_STORE | GRL_OP_STORE_PARENT | GRL_OP_REMOVE | GRL_OP_NOTIFY_CHANGE;

  return caps;
}

static gboolean
grl_bookmarks_source_notify_change_start (GrlSource *source,
                                          GError **error)
{
  GrlBookmarksSource *bookmarks_source = GRL_BOOKMARKS_SOURCE (source);

  bookmarks_source->priv->notify_changes = TRUE;

  return TRUE;
}

static gboolean
grl_bookmarks_source_notify_change_stop (GrlSource *source,
                                         GError **error)
{
  GrlBookmarksSource *bookmarks_source = GRL_BOOKMARKS_SOURCE (source);

  bookmarks_source->priv->notify_changes = FALSE;

  return TRUE;
}
