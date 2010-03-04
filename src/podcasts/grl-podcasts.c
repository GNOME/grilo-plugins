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
#include <gio/gio.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <sqlite3.h>
#include <string.h>
#include <stdlib.h>

#include "grl-podcasts.h"

#define GRL_PODCASTS_GET_PRIVATE(object)                        \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                        \
                               GRL_PODCASTS_SOURCE_TYPE,        \
                               GrlPodcastsPrivate))

/* --------- Logging  -------- */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "grl-podcasts"

/* --- Database --- */

#define GRL_SQL_DB        ".grl-podcasts"

#define GRL_SQL_CREATE_TABLE_PODCASTS           \
  "CREATE TABLE IF NOT EXISTS podcasts ("       \
  "id    INTEGER  PRIMARY KEY AUTOINCREMENT,"   \
  "title TEXT,"                                 \
  "url   TEXT,"                                 \
  "desc  TEXT,"                                 \
  "last_refreshed DATE)"

#define GRL_SQL_CREATE_TABLE_STREAMS		 \
  "CREATE TABLE IF NOT EXISTS streams ( "        \
  "podcast INTEGER REFERENCES podcasts (id), "   \
  "url     TEXT, "				 \
  "title   TEXT, "                               \
  "length  INTEGER, "                            \
  "mime    TEXT, "                               \
  "date    TEXT, "                               \
  "desc    TEXT)"

#define GRL_SQL_GET_PODCASTS				\
  "SELECT p.*, count(s.podcast <> '') "			\
  "FROM podcasts p LEFT OUTER JOIN streams s "		\
  "  ON p.id = s.podcast "				\
  "GROUP BY p.id "					\
  "LIMIT %u OFFSET %u"

#define GRL_SQL_GET_PODCASTS_BY_TEXT				\
  "SELECT p.*, count(s.podcast <> '') "				\
  "FROM podcasts p LEFT OUTER JOIN streams s "			\
  "  ON p.id = s.podcast "					\
  "WHERE p.title LIKE '%%%s%%' OR p.desc LIKE '%%%s%%' "	\
  "GROUP BY p.id "						\
  "LIMIT %u OFFSET %u"

#define GRL_SQL_GET_PODCASTS_BY_QUERY				\
  "SELECT p.*, count(s.podcast <> '') "				\
  "FROM podcasts p LEFT OUTER JOIN streams s "			\
  "  ON p.id = s.podcast "					\
  "WHERE %s "							\
  "GROUP BY p.id "						\
  "LIMIT %u OFFSET %u"

#define GRL_SQL_GET_PODCAST_BY_ID               \
  "SELECT * FROM podcasts "                     \
  "WHERE id='%s' "                              \
  "LIMIT 1"

#define GRL_SQL_STORE_PODCAST                   \
  "INSERT INTO podcasts "                       \
  "(url, title, desc) "                         \
  "VALUES (?, ?, ?)"

#define GRL_SQL_REMOVE_PODCAST                  \
  "DELETE FROM podcasts "                       \
  "WHERE id='%s'"

#define GRL_SQL_REMOVE_STREAM                   \
  "DELETE FROM streams "                        \
  "WHERE url='%s'"

#define GRL_SQL_STORE_STREAM                            \
  "INSERT INTO streams "                                \
  "(podcast, url, title, length, mime, date, desc) "    \
  "VALUES (?, ?, ?, ?, ?, ?, ?)"

#define GRL_SQL_DELETE_PODCAST_STREAMS          \
  "DELETE FROM streams WHERE podcast='%s'"

#define GRL_SQL_GET_PODCAST_STREAMS             \
  "SELECT * FROM streams "                      \
  "WHERE podcast='%s' "                         \
  "LIMIT %u  OFFSET %u"

#define GRL_SQL_GET_PODCAST_STREAM              \
  "SELECT * FROM streams "                      \
  "WHERE url='%s' "                             \
  "LIMIT 1"

#define GRL_SQL_TOUCH_PODCAST			\
  "UPDATE podcasts "				\
  "SET last_refreshed='%s' "			\
  "WHERE id='%s'"

/* --- Other --- */

#define CACHE_DURATION (24 * 60 * 60)

/* --- Plugin information --- */

#define PLUGIN_ID   "grl-podcasts"
#define PLUGIN_NAME "Podcasts"
#define PLUGIN_DESC "A plugin for browsing podcasts"

#define SOURCE_ID   "grl-podcasts"
#define SOURCE_NAME "Podcasts"
#define SOURCE_DESC "A source for browsing podcasts"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

enum {
  PODCAST_ID = 0,
  PODCAST_TITLE,
  PODCAST_URL,
  PODCAST_DESC,
  PODCAST_LAST_REFRESHED,
  PODCAST_LAST,
};

enum {
  STREAM_PODCAST = 0,
  STREAM_URL,
  STREAM_TITLE,
  STREAM_LENGTH,
  STREAM_MIME,
  STREAM_DATE,
  STREAM_DESC,
};

typedef void (*AsyncReadCbFunc) (gchar *data, gpointer user_data);

typedef struct {
  AsyncReadCbFunc callback;
  gchar *url;
  gpointer user_data;
} AsyncReadCb;

typedef struct {
  gchar *id;
  gchar *url;
  gchar *title;
  gchar *published;
  gchar *duration;
  gchar *summary;
  gchar *mime;
} Entry;

struct _GrlPodcastsPrivate {
  sqlite3 *db;
};

typedef struct {
  GrlMediaSource *source;
  guint operation_id;
  const gchar *media_id;
  guint skip;
  guint count;
  const gchar *text;
  GrlMediaSourceResultCb callback;
  guint error_code;
  gboolean is_query;
  gpointer user_data;
} OperationSpec;

typedef struct {
  OperationSpec *os;
  xmlDocPtr doc;
  xmlXPathContextPtr xpathCtx;
  xmlXPathObjectPtr xpathObj;
  guint parse_count;
  guint parse_index;
  guint parse_valid_index;
  GrlMedia *last_media;
} OperationSpecParse;

static GrlPodcastsSource *grl_podcasts_source_new (void);

static void grl_podcasts_source_finalize (GObject *plugin);

static const GList *grl_podcasts_source_supported_keys (GrlMetadataSource *source);

static void grl_podcasts_source_browse (GrlMediaSource *source,
                                        GrlMediaSourceBrowseSpec *bs);
static void grl_podcasts_source_search (GrlMediaSource *source,
                                        GrlMediaSourceSearchSpec *ss);
static void grl_podcasts_source_query (GrlMediaSource *source,
                                       GrlMediaSourceQuerySpec *qs);
static void grl_podcasts_source_metadata (GrlMediaSource *source,
                                          GrlMediaSourceMetadataSpec *ms);
static void grl_podcasts_source_store (GrlMediaSource *source,
                                       GrlMediaSourceStoreSpec *ss);
static void grl_podcasts_source_remove (GrlMediaSource *source,
                                        GrlMediaSourceRemoveSpec *rs);

/* =================== Podcasts Plugin  =============== */

static gboolean
grl_podcasts_plugin_init (GrlPluginRegistry *registry,
                          const GrlPluginInfo *plugin,
                          const GrlConfig *config)
{
  g_debug ("podcasts_plugin_init\n");

  GrlPodcastsSource *source = grl_podcasts_source_new ();
  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source));
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_podcasts_plugin_init,
                     NULL,
                     PLUGIN_ID,
                     PLUGIN_NAME,
                     PLUGIN_DESC,
                     PACKAGE_VERSION,
                     AUTHOR,
                     LICENSE,
                     SITE);

/* ================== Podcasts GObject ================ */

static GrlPodcastsSource *
grl_podcasts_source_new (void)
{
  g_debug ("grl_podcasts_source_new");
  return g_object_new (GRL_PODCASTS_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       NULL);
}

static void
grl_podcasts_source_class_init (GrlPodcastsSourceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GrlMediaSourceClass *source_class = GRL_MEDIA_SOURCE_CLASS (klass);
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);

  gobject_class->finalize = grl_podcasts_source_finalize;

  source_class->browse = grl_podcasts_source_browse;
  source_class->search = grl_podcasts_source_search;
  source_class->query = grl_podcasts_source_query;
  source_class->metadata = grl_podcasts_source_metadata;
  source_class->store = grl_podcasts_source_store;
  source_class->remove = grl_podcasts_source_remove;

  metadata_class->supported_keys = grl_podcasts_source_supported_keys;

  g_type_class_add_private (klass, sizeof (GrlPodcastsPrivate));
}

static void
grl_podcasts_source_init (GrlPodcastsSource *source)
{
  gint r;
  const gchar *home;
  gchar *db_path;
  gchar *sql_error = NULL;

  source->priv = GRL_PODCASTS_GET_PRIVATE (source);
  memset (source->priv, 0, sizeof (GrlPodcastsPrivate));

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
  r = sqlite3_exec (source->priv->db, GRL_SQL_CREATE_TABLE_PODCASTS,
		    NULL, NULL, &sql_error);

  if (!r) {
    /* TODO: if this fails, sqlite stays in an unreliable state fix that */
    r = sqlite3_exec (source->priv->db, GRL_SQL_CREATE_TABLE_STREAMS,
		      NULL, NULL, &sql_error);
  }
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

G_DEFINE_TYPE (GrlPodcastsSource, grl_podcasts_source, GRL_TYPE_MEDIA_SOURCE);

static void
grl_podcasts_source_finalize (GObject *object)
{
  GrlPodcastsSource *source;

  g_debug ("grl_podcasts_source_finalize");

  source = GRL_PODCASTS_SOURCE (object);

  sqlite3_close (source->priv->db);

  G_OBJECT_CLASS (grl_podcasts_source_parent_class)->finalize (object);
}

/* ======================= Utilities ==================== */

static void
print_entry (Entry *entry)
{
  g_print ("Entry Information:\n");
  g_print ("            ID: %s\n", entry->id);
  g_print ("         Title: %s\n", entry->title);
  g_print ("          Date: %s\n", entry->published);
  g_print ("      Duration: %s\n", entry->duration);
  g_print ("       Summary: %s\n", entry->summary);
  g_print ("           URL: %s\n", entry->url);
  g_print ("          Mime: %s\n", entry->mime);
}

static void
free_entry (Entry *entry)
{
  g_free (entry->id);
  g_free (entry->title);
  g_free (entry->published);
  g_free (entry->summary);
  g_free (entry->url);
  g_free (entry->mime);
}

static void
read_done_cb (GObject *source_object,
              GAsyncResult *res,
              gpointer user_data)
{
  AsyncReadCb *arc = (AsyncReadCb *) user_data;
  GError *vfs_error = NULL;
  gchar *content = NULL;

  g_debug ("  Done");

  g_file_load_contents_finish (G_FILE (source_object),
                               res,
                               &content,
                               NULL,
                               NULL,
                               &vfs_error);
  g_object_unref (source_object);
  if (vfs_error) {
    g_warning ("Failed to open '%s': %s", arc->url, vfs_error->message);
  } else {
    arc->callback (content, arc->user_data);
  }
  g_free (arc->url);
  g_free (arc);
}

static void
read_url_async (const gchar *url,
                AsyncReadCbFunc callback,
                gpointer user_data)
{
  GVfs *vfs;
  GFile *uri;
  AsyncReadCb *arc;

  vfs = g_vfs_get_default ();

  g_debug ("Opening async '%s'", url);

  arc = g_new0 (AsyncReadCb, 1);
  arc->url = g_strdup (url);
  arc->callback = callback;
  arc->user_data = user_data;
  uri = g_vfs_get_file_for_uri (vfs, url);
  g_file_load_contents_async (uri, NULL, read_done_cb, arc);
}

static gint
duration_to_seconds (const gchar *str)
{
  gint seconds = 0;
  gchar **parts;
  gint i;
  guint multiplier = 1;

  if (!str || str[0] == '\0') {
    return 0;
  }

  parts = g_strsplit (str, ":", 3);

  /* Get last portion (seconds) */
  i = 0;
  while (parts[i]) i++;
  if (i == 0) {
    g_strfreev (parts);
    return 0;
  } else {
    i--;
  }

  do {
    seconds += atoi (parts[i]) * multiplier;
    multiplier *= 60;
    i--;
  } while (i >= 0);

  g_strfreev (parts);

  return seconds;
}

static gboolean
mime_is_video (const gchar *mime)
{
  return mime && strstr (mime, "video") != NULL;
}

static gboolean
mime_is_audio (const gchar *mime)
{
  return mime && strstr (mime, "audio") != NULL;
}

static gchar *
get_site_from_url (const gchar *url)
{
  gchar *p;

  if (g_str_has_prefix (url, "file://")) {
    return NULL;
  }

  p = strstr (url, "://");
  if (!p) {
    return NULL;
  } else {
    p += 3;
  }

  while (*p != '/') p++;

  return g_strndup (url, p - url);
}

static GrlMedia *
build_media (GrlMedia *content,
	     gboolean is_podcast,
	     const gchar *id,
	     const gchar *title,
	     const gchar *url,
	     const gchar *desc,
	     const gchar *mime,
	     const gchar *date,
	     guint duration,
	     guint childcount)
{
  GrlMedia *media = NULL;
  gchar *site;

  if (content) {
    media = content;
  }

  if (is_podcast) {
    if (!media) {
      media = GRL_MEDIA (grl_media_box_new ());
    }

    grl_media_set_id (media, id);
    if (desc)
      grl_media_set_description (media, desc);
    grl_media_box_set_childcount (GRL_MEDIA_BOX (media), childcount);
  } else {
    if (!media) {
      if (mime_is_audio (mime)) {
	media = grl_media_audio_new ();
      } else if (mime_is_video (mime)) {
	media = grl_media_video_new ();
      } else {
	media = grl_media_new ();
      }
    }

    grl_media_set_id (media, url);
    if (date)
      grl_media_set_date (media, date);
    if (desc)
      grl_media_set_description (media, desc);
    if (mime)
      grl_media_set_mime (media, mime);
    if (duration > 0) {
      grl_media_set_duration (media, duration);
    }
  }

  grl_media_set_title (media, title);
  grl_media_set_url (media, url);

  site = get_site_from_url (url);
  if (site) {
    grl_media_set_site (media, site);
    g_free (site);
  }

  return media;
}

static GrlMedia *
build_media_from_entry (Entry *entry)
{
  GrlMedia *media;
  gint duration;

  duration = duration_to_seconds (entry->duration);
  media = build_media (NULL, FALSE,
		       entry->url, entry->title, entry->url,
		       entry->summary, entry->mime, entry->published,
		       duration, 0);
  return media;
}

static GrlMedia *
build_media_from_stmt (GrlMedia *content,
		       sqlite3_stmt *sql_stmt,
		       gboolean is_podcast)
{
  GrlMedia *media;
  gchar *id;
  gchar *title;
  gchar *url;
  gchar *desc;
  gchar *mime;
  gchar *date;
  guint duration;
  guint childcount;

  if (is_podcast) {
    id = (gchar *) sqlite3_column_text (sql_stmt, PODCAST_ID);
    title = (gchar *) sqlite3_column_text (sql_stmt, PODCAST_TITLE);
    url = (gchar *) sqlite3_column_text (sql_stmt, PODCAST_URL);
    desc = (gchar *) sqlite3_column_text (sql_stmt, PODCAST_DESC);
    childcount = (guint) sqlite3_column_int (sql_stmt, PODCAST_LAST);
    media = build_media (content, is_podcast,
			 id, title, url, desc, NULL, NULL, 0, childcount);
  } else {
    mime = (gchar *) sqlite3_column_text (sql_stmt, STREAM_MIME);
    url = (gchar *) sqlite3_column_text (sql_stmt, STREAM_URL);
    title = (gchar *) sqlite3_column_text (sql_stmt, STREAM_TITLE);
    date = (gchar *) sqlite3_column_text (sql_stmt, STREAM_DATE);
    desc = (gchar *) sqlite3_column_text (sql_stmt, STREAM_DESC);
    duration = sqlite3_column_int (sql_stmt, STREAM_LENGTH);
    media = build_media (content, is_podcast,
			 url, title, url, desc, mime, date, duration, 0);
  }

  return media;
}

static void
produce_podcast_contents_from_db (OperationSpec *os)
{
  sqlite3 *db;
  gchar *sql;
  sqlite3_stmt *sql_stmt = NULL;
  GList *iter, *medias = NULL;
  guint count = 0;
  GrlMedia *media;
  gint r;
  GError *error = NULL;

  g_debug ("produce_podcast_contents_from_db");

  db = GRL_PODCASTS_SOURCE (os->source)->priv->db;
  sql = g_strdup_printf (GRL_SQL_GET_PODCAST_STREAMS,
			 os->media_id, os->count, os->skip);
  g_debug ("%s", sql);
  r = sqlite3_prepare_v2 (db, sql, strlen (sql), &sql_stmt, NULL);
  g_free (sql);

  if (r != SQLITE_OK) {
    g_warning ("Failed to retrieve podcast streams: %s", sqlite3_errmsg (db));
    error = g_error_new (GRL_ERROR,
			 os->error_code,
			 "Failed to retrieve podcast streams");
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
    return;
  }

  while ((r = sqlite3_step (sql_stmt)) == SQLITE_BUSY);

  while (r == SQLITE_ROW) {
    media = build_media_from_stmt (NULL, sql_stmt, FALSE);
    medias = g_list_prepend (medias, media);
    count++;
    r = sqlite3_step (sql_stmt);
  }

  if (r != SQLITE_DONE) {
    g_warning ("Failed to retrive podcast streams: %s", sqlite3_errmsg (db));
    error = g_error_new (GRL_ERROR,
			 os->error_code,
			 "Failed to retrieve podcast streams");
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
    sqlite3_finalize (sql_stmt);
    return;
  }

  sqlite3_finalize (sql_stmt);

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
}

static void
remove_podcast_streams (sqlite3 *db, const gchar *podcast_id, GError **error)
{
  gchar *sql;
  gchar *sql_error;
  gint r;

  sql = g_strdup_printf (GRL_SQL_DELETE_PODCAST_STREAMS, podcast_id);
  g_debug ("%s", sql);
  r = sqlite3_exec (db, sql, NULL, NULL, &sql_error);
  g_free (sql);
  if (r) {
    g_warning ("Failed to remove podcast streams cache: %s", sql_error);
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_REMOVE_FAILED,
			  "Failed to remove podcast streams");
    sqlite3_free (error);
  }
}

static void
remove_podcast (sqlite3 *db, const gchar *podcast_id, GError **error)
{
  gint r;
  gchar *sql_error;
  gchar *sql;

  g_debug ("remove_podcast");

  remove_podcast_streams (db, podcast_id, error);
  if (*error) {
    return;
  }

  sql = g_strdup_printf (GRL_SQL_REMOVE_PODCAST, podcast_id);
  g_debug ("%s", sql);
  r = sqlite3_exec (db, sql, NULL, NULL, &sql_error);
  g_free (sql);

  if (r != SQLITE_OK) {
    g_warning ("Failed to remove podcast '%s': %s", podcast_id, sql_error);
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_REMOVE_FAILED,
			  "Failed to remove podcast");
    sqlite3_free (sql_error);
  }
}

static void
remove_stream (sqlite3 *db, const gchar *url, GError **error)
{
  gint r;
  gchar *sql_error;
  gchar *sql;

  g_debug ("remove_stream");

  sql = g_strdup_printf (GRL_SQL_REMOVE_STREAM, url);
  g_debug ("%s", sql);
  r = sqlite3_exec (db, sql, NULL, NULL, &sql_error);
  g_free (sql);

  if (r != SQLITE_OK) {
    g_warning ("Failed to remove podcast stream '%s': %s", url, sql_error);
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_REMOVE_FAILED,
			  "Failed to remove podcast stream");
    sqlite3_free (sql_error);
  }
}

static void
store_podcast (sqlite3 *db, GrlMedia *podcast, GError **error)
{
  gint r;
  sqlite3_stmt *sql_stmt = NULL;
  const gchar *title;
  const gchar *url;
  const gchar *desc;
  gchar *id;

  g_debug ("store_podcast");

  title = grl_media_get_title (podcast);
  url = grl_media_get_url (podcast);
  desc = grl_media_get_description (podcast);

  g_debug ("%s", GRL_SQL_STORE_PODCAST);
  r = sqlite3_prepare_v2 (db,
			  GRL_SQL_STORE_PODCAST,
			  strlen (GRL_SQL_STORE_PODCAST),
			  &sql_stmt, NULL);
  if (r != SQLITE_OK) {
    g_warning ("Failed to store podcast '%s': %s", title, sqlite3_errmsg (db));
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_STORE_FAILED,
			  "Failed to store podcast '%s'", title);
    return;
  }

  sqlite3_bind_text (sql_stmt, 1, url, -1, SQLITE_STATIC);
  sqlite3_bind_text (sql_stmt, 2, title, -1, SQLITE_STATIC);
  if (desc) {
    sqlite3_bind_text (sql_stmt, 3, desc, -1, SQLITE_STATIC);
  } else {
    sqlite3_bind_text (sql_stmt, 3, "", -1, SQLITE_STATIC);
  }

  while ((r = sqlite3_step (sql_stmt)) == SQLITE_BUSY);

  if (r != SQLITE_DONE) {
    g_warning ("Failed to store podcast '%s': %s", title, sqlite3_errmsg (db));
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_STORE_FAILED,
			  "Failed to store podcast '%s'", title);
    sqlite3_finalize (sql_stmt);
    return;
  }

  sqlite3_finalize (sql_stmt);

  id = g_strdup_printf ("%llu", sqlite3_last_insert_rowid (db));
  grl_media_set_id (podcast, id);
  g_free (id);
}

static void
store_stream (sqlite3 *db, const gchar *podcast_id, Entry *entry)
{
  gint r;
  guint seconds;
  sqlite3_stmt *sql_stmt = NULL;

  if (!entry->url || entry->url[0] == '\0') {
    g_debug ("Podcast stream has no URL, skipping");
    return;
  }

  seconds = duration_to_seconds (entry->duration);
  g_debug ("%s", GRL_SQL_STORE_STREAM);
  r = sqlite3_prepare_v2 (db,
			  GRL_SQL_STORE_STREAM,
			  strlen (GRL_SQL_STORE_STREAM),
			  &sql_stmt, NULL);
  if (r != SQLITE_OK) {
    g_warning ("Failed to store podcast stream '%s': %s",
	       entry->url, sqlite3_errmsg (db));
    return;
  }

  sqlite3_bind_text (sql_stmt, 1, podcast_id, -1, SQLITE_STATIC);
  sqlite3_bind_text (sql_stmt, 2, entry->url, -1, SQLITE_STATIC);
  sqlite3_bind_text (sql_stmt, 3, entry->title, -1, SQLITE_STATIC);
  sqlite3_bind_int  (sql_stmt, 4, seconds);
  sqlite3_bind_text (sql_stmt, 5, entry->mime, -1, SQLITE_STATIC);
  sqlite3_bind_text (sql_stmt, 6, entry->published, -1, SQLITE_STATIC);
  sqlite3_bind_text (sql_stmt, 7, entry->summary, -1, SQLITE_STATIC);

  while ((r = sqlite3_step (sql_stmt)) == SQLITE_BUSY);

  if (r != SQLITE_DONE) {
    g_warning ("Failed to store podcast stream '%s': %s",
	       entry->url, sqlite3_errmsg (db));
  }

  sqlite3_finalize (sql_stmt);
}

static void
parse_entry (xmlDocPtr doc, xmlNodePtr entry, Entry *data)
{
  xmlNodePtr node;
  node = entry->xmlChildrenNode;
  while (node) {
    if (!xmlStrcmp (node->name, (const xmlChar *) "title")) {
      data->title =
	(gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    } else if (!xmlStrcmp (node->name, (const xmlChar *) "enclosure")) {
      data->id = (gchar *) xmlGetProp (node, (xmlChar *) "url");
      data->url = g_strdup (data->id);
      data->mime = (gchar *) xmlGetProp (node, (xmlChar *) "type");
    } else if (!xmlStrcmp (node->name, (const xmlChar *) "summary")) {
      data->summary =
	(gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    } else if (!xmlStrcmp (node->name, (const xmlChar *) "pubDate")) {
      data->published =
	(gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    } else if (!xmlStrcmp (node->name, (const xmlChar *) "duration")) {
      data->duration =
	(gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    }
    node = node->next;
  }
}

static void
touch_podcast (sqlite3 *db, const gchar *podcast_id)
{
  gint r;
  gchar *sql, *sql_error;
  GTimeVal now;
  gchar *now_str;

  g_debug ("touch_podcast");

  g_get_current_time (&now);
  now_str = g_time_val_to_iso8601 (&now);

  sql = g_strdup_printf (GRL_SQL_TOUCH_PODCAST, now_str, podcast_id);
  g_debug ("%s", sql);
  r = sqlite3_exec (db, sql, NULL, NULL, &sql_error);
  g_free (sql);

  if (r != SQLITE_OK) {
    g_warning ("Failed to touch podcast, '%s': %s", podcast_id, sql_error);
    sqlite3_free (sql_error);
    return;
  }
}

static gboolean
parse_entry_idle (gpointer user_data)
{
  OperationSpecParse *osp = (OperationSpecParse *) user_data;
  xmlNodeSetPtr nodes;
  guint remaining;
  GrlMedia *media;

  nodes = osp->xpathObj->nodesetval;

  /* Parse entry */
  Entry *entry = g_new0 (Entry, 1);
  parse_entry (osp->doc, nodes->nodeTab[osp->parse_index], entry);
  if (0) print_entry (entry);

  /* Check if entry is valid */
  if (!entry->url || entry->url[0] == '\0') {
    g_debug ("Podcast stream has no URL, skipping");
  } else {
    /* Provide results to user as fast as possible */
    if (osp->parse_valid_index >= osp->os->skip &&
	osp->parse_valid_index < osp->os->skip + osp->os->count) {
      media = build_media_from_entry (entry);
      remaining = osp->os->skip + osp->os->count - osp->parse_valid_index - 1;

      /* Hack: if we emit the last result now the UI may request more results
	 right away while we are still parsing the XML, so we keep the last
	 result until we are done processing the whole feed, this way when
	 the next query arrives all the stuff is stored in the database 
	 and the query can be resolved normally */
      if (remaining > 0) {
	osp->os->callback (osp->os->source,
			   osp->os->operation_id,
			   media,
			   remaining,
			   osp->os->user_data,
			   NULL);
      } else {
	osp->last_media = media;
      }
    }

    osp->parse_valid_index++;

    /* And store stream in database cache */
    store_stream (GRL_PODCASTS_SOURCE (osp->os->source)->priv->db,
		  osp->os->media_id, entry);
  }

  osp->parse_index++;
  free_entry (entry);

  if (osp->parse_index >= osp->parse_count) {
    /* Send last result */
    osp->os->callback (osp->os->source,
		       osp->os->operation_id,
		       osp->last_media,
		       0,
		       osp->os->user_data,
		       NULL);
    
    g_free (osp->os);
    xmlXPathFreeObject (osp->xpathObj);
    xmlXPathFreeContext (osp->xpathCtx);
    xmlFreeDoc (osp->doc);
    g_free (osp);
  }
  
  return osp->parse_index < osp->parse_count;
}

static void
parse_feed (OperationSpec *os, const gchar *str, GError **error)
{
  xmlDocPtr doc = NULL;
  xmlXPathContextPtr xpathCtx = NULL;
  xmlXPathObjectPtr xpathObj = NULL;
  guint stream_count;

  g_debug ("parse_feed");

  doc = xmlParseDoc ((xmlChar *) str);
  if (!doc) {
    *error = g_error_new (GRL_ERROR,
			  os->error_code,
			  "Failed to parse podcast contents");
    goto free_resources;
  }

  /* Get feed stream list */
  xpathCtx = xmlXPathNewContext (doc);
  if (!xpathCtx) {
    *error = g_error_new (GRL_ERROR,
			  os->error_code,
			  "Failed to parse podcast contents");
    goto free_resources;
  }
  
  xpathObj = xmlXPathEvalExpression ((xmlChar *) "/rss/channel/item",
				     xpathCtx);
  if(xpathObj == NULL) {
    *error = g_error_new (GRL_ERROR,
			  os->error_code,
			  "Failed to parse podcast contents");
    goto free_resources;
  }

  /* Feed is ok, let's process it */

  /* First, remove old entries for this podcast */
  remove_podcast_streams (GRL_PODCASTS_SOURCE (os->source)->priv->db,
			  os->media_id, error);
  if (*error) {
    (*error)->code = os->error_code;
    goto free_resources;
  }

  /* Then update the last_refreshed date of the podcast */
  touch_podcast (GRL_PODCASTS_SOURCE (os->source)->priv->db, os->media_id);

  /* If the feed contains no streams, notify and bail out */
  stream_count = xpathObj->nodesetval ? xpathObj->nodesetval->nodeNr : 0;
  g_debug ("Got %d streams", stream_count);
  
  if (stream_count <= 0) {
    os->callback (os->source,
		  os->operation_id,
		  NULL,
		  0,
		  os->user_data,
		  NULL);
    goto free_resources;
  }

  /* Otherwise parse the streams in idle loop to prevent blocking */
  OperationSpecParse *osp = g_new0 (OperationSpecParse, 1);
  osp->os = os;
  osp->doc = doc;
  osp->xpathCtx = xpathCtx;
  osp->xpathObj = xpathObj;
  osp->parse_count = stream_count;
  g_idle_add (parse_entry_idle, osp);
  return;

 free_resources:
  if (xpathObj)
    xmlXPathFreeObject (xpathObj);
  if (xpathCtx)
    xmlXPathFreeContext (xpathCtx);
  if (doc)
    xmlFreeDoc (doc);
}

static void
read_feed_cb (gchar *xmldata, gpointer user_data)
{
  GError *error = NULL;
  OperationSpec *os = (OperationSpec *) user_data;

  if (!xmldata) {
    error = g_error_new (GRL_ERROR,
			 GRL_ERROR_BROWSE_FAILED,
			 "Failed to read data from podcast");
  } else {
    parse_feed (os, xmldata, &error);
    g_free (xmldata);
  }

  if (error) {
    os->callback (os->source,
		  os->operation_id,
		  NULL,
		  0,
		  os->user_data,
		  error);
    g_error_free (error);
    g_free (os);
  }
}

static sqlite3_stmt *
get_podcast_info (sqlite3 *db, const gchar *podcast_id)
{
  gint r;
  sqlite3_stmt *sql_stmt = NULL;
  gchar *sql;

  g_debug ("get_podcast_info");

  sql = g_strdup_printf (GRL_SQL_GET_PODCAST_BY_ID, podcast_id);
  g_debug ("%s", sql);
  r = sqlite3_prepare_v2 (db, sql, strlen (sql), &sql_stmt, NULL);
  g_free (sql);

  if (r != SQLITE_OK) {
    g_warning ("Failed to retrieve podcast '%s': %s",
	       podcast_id, sqlite3_errmsg (db));
    return NULL;
  }

  while ((r = sqlite3_step (sql_stmt)) == SQLITE_BUSY);

  if (r == SQLITE_ROW) {
    return sql_stmt;
  } else {
    g_warning ("Failed to retrieve podcast information: %s",
	       sqlite3_errmsg (db));
    sqlite3_finalize (sql_stmt);
    return NULL;
  }
}

static void
produce_podcast_contents (OperationSpec *os)
{
  sqlite3_stmt *sql_stmt = NULL;
  sqlite3 *db;
  GError *error;
  gchar *url;

  g_debug ("produce_podcast_contents");

  /* First we get some information about the podcast */
  db = GRL_PODCASTS_SOURCE (os->source)->priv->db;
  sql_stmt = get_podcast_info (db, os->media_id);
  if (sql_stmt) {
    gchar *lr_str;
    GTimeVal lr;
    GTimeVal now;

    /* Check if we have to refresh the podcast */
    lr_str = (gchar *) sqlite3_column_text (sql_stmt, PODCAST_LAST_REFRESHED);
    g_debug ("Podcast last-refreshed: '%s'", lr_str);
    g_time_val_from_iso8601 (lr_str ? lr_str : "", &lr);
    g_get_current_time (&now);
    now.tv_sec -= CACHE_DURATION;
    if (now.tv_sec >= lr.tv_sec) {
      /* We have to read the podcast feed again */
      g_debug ("Refreshing podcast '%s'...", os->media_id);
      url = g_strdup ((gchar *) sqlite3_column_text (sql_stmt, PODCAST_URL));
      read_url_async (url, read_feed_cb, os);
      g_free (url);
    } else {
      /* We can read the podcast entries from the database cache */
      produce_podcast_contents_from_db (os);
      g_free (os);
    }
    sqlite3_finalize (sql_stmt);
  } else {
    error = g_error_new (GRL_ERROR,
			 os->error_code,
			 "Failed to retrieve podcast information");
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
    g_free (os);
  }

}

static void
produce_podcasts (OperationSpec *os)
{
  gint r;
  sqlite3_stmt *sql_stmt = NULL;
  sqlite3 *db;
  GrlMedia *media;
  GError *error = NULL;
  GList *medias = NULL;
  guint count = 0;
  GList *iter;
  gchar *sql;

  g_debug ("produce_podcasts");

  db = GRL_PODCASTS_SOURCE (os->source)->priv->db;

  if (!os->text) {
    /* Browse */
    sql = g_strdup_printf (GRL_SQL_GET_PODCASTS, os->count, os->skip);
  } else if (os->is_query) {
    /* Query */
    sql = g_strdup_printf (GRL_SQL_GET_PODCASTS_BY_QUERY,
			   os->text, os->count, os->skip);
  } else {
    /* Search */
    sql = g_strdup_printf (GRL_SQL_GET_PODCASTS_BY_TEXT,
			   os->text, os->text, os->count, os->skip);
  }
  g_debug ("%s", sql);
  r = sqlite3_prepare_v2 (db, sql, strlen (sql), &sql_stmt, NULL);
  g_free (sql);

  if (r != SQLITE_OK) {
    g_warning ("Failed to retrieve podcasts: %s", sqlite3_errmsg (db));
    error = g_error_new (GRL_ERROR,
			 os->error_code,
			 "Failed to retrieve podcasts list");
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
    goto free_resources;
  }

  while ((r = sqlite3_step (sql_stmt)) == SQLITE_BUSY);

  while (r == SQLITE_ROW) {
    media = build_media_from_stmt (NULL, sql_stmt, TRUE);
    medias = g_list_prepend (medias, media);
    count++;
    r = sqlite3_step (sql_stmt);
  }

  if (r != SQLITE_DONE) {
    g_warning ("Failed to retrieve podcasts: %s", sqlite3_errmsg (db));
    error = g_error_new (GRL_ERROR,
			 os->error_code,
			 "Failed to retrieve podcasts list");
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
stream_metadata (GrlMediaSourceMetadataSpec *ms)
{
  gint r;
  sqlite3_stmt *sql_stmt = NULL;
  sqlite3 *db;
  GError *error = NULL;
  gchar *sql;
  const gchar *id;

  g_debug ("stream_metadata");

  db = GRL_PODCASTS_SOURCE (ms->source)->priv->db;

  id = grl_media_get_id (ms->media);
  sql = g_strdup_printf (GRL_SQL_GET_PODCAST_STREAM, id);
  g_debug ("%s", sql);
  r = sqlite3_prepare_v2 (db, sql, strlen (sql), &sql_stmt, NULL);
  g_free (sql);

  if (r != SQLITE_OK) {
    g_warning ("Failed to get podcast stream: %s", sqlite3_errmsg (db));
    error = g_error_new (GRL_ERROR,
			 GRL_ERROR_METADATA_FAILED,
			 "Failed to get podcast stream metadata");
    ms->callback (ms->source, ms->media, ms->user_data, error);
    g_error_free (error);
    return;
  }

  while ((r = sqlite3_step (sql_stmt)) == SQLITE_BUSY);

  if (r == SQLITE_ROW) {
    build_media_from_stmt (ms->media, sql_stmt, FALSE);
    ms->callback (ms->source, ms->media, ms->user_data, NULL);
  } else {
    g_warning ("Failed to get podcast stream: %s", sqlite3_errmsg (db));
    error = g_error_new (GRL_ERROR,
			 GRL_ERROR_METADATA_FAILED,
			 "Failed to get podcast stream metadata");
    ms->callback (ms->source, ms->media, ms->user_data, error);
    g_error_free (error);
  }

  sqlite3_finalize (sql_stmt);
}

static void
podcast_metadata (GrlMediaSourceMetadataSpec *ms)
{
  sqlite3_stmt *sql_stmt = NULL;
  sqlite3 *db;
  GError *error = NULL;
  const gchar *id;

  g_debug ("podcast_metadata");

  db = GRL_PODCASTS_SOURCE (ms->source)->priv->db;

  id = grl_media_get_id (ms->media);
  if (!id) {
    /* Root category: special case */
    grl_media_set_title (ms->media, "");
    ms->callback (ms->source, ms->media, ms->user_data, NULL);
    return;
  }

  sql_stmt = get_podcast_info (db, id);

  if (sql_stmt) {
    build_media_from_stmt (ms->media, sql_stmt, TRUE);
    ms->callback (ms->source, ms->media, ms->user_data, NULL);
    sqlite3_finalize (sql_stmt);
  } else {
    g_warning ("Failed to get podcast: %s", sqlite3_errmsg (db));
    error = g_error_new (GRL_ERROR,
			 GRL_ERROR_METADATA_FAILED,
			 "Failed to get podcast metadata");
    ms->callback (ms->source, ms->media, ms->user_data, error);
    g_error_free (error);
  }
}

static gboolean
media_id_is_podcast (const gchar *id)
{
  return g_ascii_strtoll (id, NULL, 10) != 0;
}

/* ================== API Implementation ================ */

static const GList *
grl_podcasts_source_supported_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_URL,
                                      GRL_METADATA_KEY_CHILDCOUNT,
                                      GRL_METADATA_KEY_SITE,
                                      NULL);
  }
  return keys;
}

static void
grl_podcasts_source_browse (GrlMediaSource *source,
                            GrlMediaSourceBrowseSpec *bs)
{
  g_debug ("grl_podcasts_source_browse");

  OperationSpec *os;
  GrlPodcastsSource *podcasts_source;
  GError *error = NULL;

  podcasts_source = GRL_PODCASTS_SOURCE (source);
  if (!podcasts_source->priv->db) {
    g_warning ("Can't execute operation: no database connection.");
    error = g_error_new (GRL_ERROR,
			 GRL_ERROR_BROWSE_FAILED,
			 "No database connection");
    bs->callback (bs->source, bs->browse_id, NULL, 0, bs->user_data, error);
    g_error_free (error);
  }

  /* Configure browse operation */
  os = g_new0 (OperationSpec, 1);
  os->source = bs->source;
  os->operation_id = bs->browse_id;
  os->media_id = grl_media_get_id (bs->container);
  os->count = bs->count;
  os->skip = bs->skip;
  os->callback = bs->callback;
  os->user_data = bs->user_data;
  os->error_code = GRL_ERROR_BROWSE_FAILED;

  if (!os->media_id) {
    /* Browsing podcasts list */
    produce_podcasts (os);
    g_free (os);
  } else {
    /* Browsing a particular podcast. We may need to parse
       the feed (async) and in that case we will need to keep
       os, so we do not free os here */
    produce_podcast_contents (os);
  }
}

static void
grl_podcasts_source_search (GrlMediaSource *source,
                            GrlMediaSourceSearchSpec *ss)
{
  g_debug ("grl_podcasts_source_search");

  GrlPodcastsSource *podcasts_source;
  OperationSpec *os;
  GError *error = NULL;

  podcasts_source = GRL_PODCASTS_SOURCE (source);
  if (!podcasts_source->priv->db) {
    g_warning ("Can't execute operation: no database connection.");
    error = g_error_new (GRL_ERROR,
			 GRL_ERROR_QUERY_FAILED,
			 "No database connection");
    ss->callback (ss->source, ss->search_id, NULL, 0, ss->user_data, error);
    g_error_free (error);
  }

  os = g_new0 (OperationSpec, 1);
  os->source = ss->source;
  os->operation_id = ss->search_id;
  os->text = ss->text;
  os->count = ss->count;
  os->skip = ss->skip;
  os->callback = ss->callback;
  os->user_data = ss->user_data;
  os->error_code = GRL_ERROR_SEARCH_FAILED;
  produce_podcasts (os);
  g_free (os);
}

static void
grl_podcasts_source_query (GrlMediaSource *source, GrlMediaSourceQuerySpec *qs)
{
  g_debug ("grl_podcasts_source_query");

  GrlPodcastsSource *podcasts_source;
  OperationSpec *os;
  GError *error = NULL;

  podcasts_source = GRL_PODCASTS_SOURCE (source);
  if (!podcasts_source->priv->db) {
    g_warning ("Can't execute operation: no database connection.");
    error = g_error_new (GRL_ERROR,
			 GRL_ERROR_QUERY_FAILED,
			 "No database connection");
    qs->callback (qs->source, qs->query_id, NULL, 0, qs->user_data, error);
    g_error_free (error);
  }

  os = g_new0 (OperationSpec, 1);
  os->source = qs->source;
  os->operation_id = qs->query_id;
  os->text = qs->query;
  os->count = qs->count;
  os->skip = qs->skip;
  os->callback = qs->callback;
  os->user_data = qs->user_data;
  os->is_query = TRUE;
  os->error_code = GRL_ERROR_QUERY_FAILED;
  produce_podcasts (os);
  g_free (os);
}

static void
grl_podcasts_source_metadata (GrlMediaSource *source,
                              GrlMediaSourceMetadataSpec *ms)
{
  g_debug ("grl_podcasts_source_metadata");

  GrlPodcastsSource *podcasts_source;
  GError *error = NULL;
  const gchar *media_id;

  podcasts_source = GRL_PODCASTS_SOURCE (source);
  if (!podcasts_source->priv->db) {
    g_warning ("Can't execute operation: no database connection.");
    error = g_error_new (GRL_ERROR,
			 GRL_ERROR_METADATA_FAILED,
			 "No database connection");
    ms->callback (ms->source, ms->media, ms->user_data, error);
    g_error_free (error);
  }

  media_id = grl_media_get_id (ms->media);
  if (!media_id || media_id_is_podcast (media_id)) {
    podcast_metadata (ms);
  } else {
    stream_metadata (ms);
  }
}

static void
grl_podcasts_source_store (GrlMediaSource *source, GrlMediaSourceStoreSpec *ss)
{
  g_debug ("grl_podcasts_source_store");
  GError *error = NULL;
  if (GRL_IS_MEDIA_BOX (ss->media)) {
    error = g_error_new (GRL_ERROR,
			 GRL_ERROR_STORE_FAILED,
			 "Cannot create containers. Only feeds are accepted.");
  } else {
    store_podcast (GRL_PODCASTS_SOURCE (ss->source)->priv->db,
		   ss->media,
		   &error);
  }
  ss->callback (ss->source, ss->parent, ss->media, ss->user_data, error);
  if (error) {
    g_error_free (error);
  }
}

static void
grl_podcasts_source_remove (GrlMediaSource *source,
                            GrlMediaSourceRemoveSpec *rs)
{
  g_debug ("grl_podcasts_source_remove");
  GError *error = NULL;
  if (media_id_is_podcast (rs->media_id)) {
    remove_podcast (GRL_PODCASTS_SOURCE (rs->source)->priv->db,
		    rs->media_id, &error);
  } else {
    remove_stream (GRL_PODCASTS_SOURCE (rs->source)->priv->db,
		   rs->media_id, &error);
  }
  rs->callback (rs->source, rs->media, rs->user_data, error);
  if (error) {
    g_error_free (error);
  }
}
