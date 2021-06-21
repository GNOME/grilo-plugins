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
#include <net/grl-net.h>
#include <libxml/xpath.h>
#include <sqlite3.h>
#include <string.h>
#include <totem-pl-parser.h>

#include "grl-podcasts.h"

#define GRL_ROOT_TITLE "Podcasts"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT podcasts_log_domain
GRL_LOG_DOMAIN_STATIC(podcasts_log_domain);

/* --- Database --- */

#define GRL_SQL_DB "grl-podcasts.db"

#define GRL_SQL_CREATE_TABLE_PODCASTS           \
  "CREATE TABLE IF NOT EXISTS podcasts ("       \
  "id    INTEGER  PRIMARY KEY AUTOINCREMENT,"   \
  "title TEXT,"                                 \
  "url   TEXT,"                                 \
  "desc  TEXT,"                                 \
  "last_refreshed DATE,"                        \
  "image TEXT)"

#define GRL_SQL_CREATE_TABLE_STREAMS		 \
  "CREATE TABLE IF NOT EXISTS streams ( "        \
  "podcast INTEGER REFERENCES podcasts (id), "   \
  "url     TEXT, "				 \
  "title   TEXT, "                               \
  "length  INTEGER, "                            \
  "mime    TEXT, "                               \
  "date    TEXT, "                               \
  "desc    TEXT, "                               \
  "image   TEXT)"

#define GRL_SQL_GET_PODCASTS				\
  "SELECT p.*, count(s.podcast <> '') "			\
  "FROM podcasts p LEFT OUTER JOIN streams s "		\
  "  ON p.id = s.podcast "				\
  "GROUP BY p.id "					\
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

#define GRL_SQL_STORE_STREAM                                    \
  "INSERT INTO streams "                                        \
  "(podcast, url, title, length, mime, date, desc, image) "     \
  "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"

#define GRL_SQL_DELETE_PODCAST_STREAMS          \
  "DELETE FROM streams WHERE podcast='%s'"

#define GRL_SQL_GET_PODCAST_STREAMS             \
  "SELECT * FROM streams "                      \
  "WHERE podcast='%s' "                         \
  "LIMIT %u  OFFSET %u"

#define GRL_SQL_GET_PODCAST_STREAMS_BY_TEXT                     \
  "SELECT s.* "                                                 \
  "FROM streams s LEFT OUTER JOIN podcasts p "			\
  "  ON s.podcast = p.id "					\
  "WHERE s.title LIKE '%%%s%%' OR s.desc LIKE '%%%s%%' "	\
  "  OR p.title LIKE '%%%s%%' OR p.desc LIKE '%%%s%%' "         \
  "LIMIT %u OFFSET %u"

#define GRL_SQL_GET_PODCAST_STREAMS_ALL         \
  "SELECT * FROM streams "                      \
  "LIMIT %u OFFSET %u"

#define GRL_SQL_GET_PODCAST_STREAM              \
  "SELECT * FROM streams "                      \
  "WHERE url='%s' "                             \
  "LIMIT 1"

#define GRL_SQL_TOUCH_PODCAST			\
  "UPDATE podcasts "				\
  "SET last_refreshed=?, "			\
  "    desc=?, "                                \
  "    image=? "                                \
  "WHERE id=?"

/* --- Other --- */

#define DEFAULT_CACHE_TIME (24 * 60 * 60)

/* --- Plugin information --- */

#define SOURCE_ID   "grl-podcasts"
#define SOURCE_NAME "Podcasts"
#define SOURCE_DESC _("A source for browsing podcasts")

enum {
  PODCAST_ID = 0,
  PODCAST_TITLE,
  PODCAST_URL,
  PODCAST_DESC,
  PODCAST_LAST_REFRESHED,
  PODCAST_IMAGE,
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
  STREAM_IMAGE,
};

typedef void (*AsyncReadCbFunc) (gchar *data, gpointer user_data);

typedef struct {
  AsyncReadCbFunc callback;
  gchar *url;
  gpointer user_data;
} AsyncReadCb;

typedef struct {
  gchar *image;
  gchar *desc;
  gchar *published;
} PodcastData;

typedef struct {
  gchar *id;
  gchar *url;
  gchar *title;
  gchar *published;
  gchar *duration;
  gchar *summary;
  gchar *mime;
  gchar *image;
} Entry;

struct _GrlPodcastsPrivate {
  sqlite3 *db;
  GrlNetWc *wc;
  gboolean notify_changes;
  gint cache_time;
};

typedef struct {
  GrlSource *source;
  guint operation_id;
  const gchar *media_id;
  guint skip;
  guint count;
  const gchar *text;
  GrlSourceResultCb callback;
  guint error_code;
  gboolean is_query;
  time_t last_refreshed;
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

static const GList *grl_podcasts_source_supported_keys (GrlSource *source);

static void grl_podcasts_source_browse (GrlSource *source,
                                        GrlSourceBrowseSpec *bs);

static void grl_podcasts_source_search (GrlSource *source,
                                        GrlSourceSearchSpec *ss);

static void grl_podcasts_source_query (GrlSource *source,
                                       GrlSourceQuerySpec *qs);

static void grl_podcasts_source_resolve (GrlSource *source,
                                         GrlSourceResolveSpec *rs);

static void grl_podcasts_source_store (GrlSource *source,
                                       GrlSourceStoreSpec *ss);

static void grl_podcasts_source_remove (GrlSource *source,
                                        GrlSourceRemoveSpec *rs);

static gboolean grl_podcasts_source_notify_change_start (GrlSource *source,
                                                         GError **error);

static gboolean grl_podcasts_source_notify_change_stop (GrlSource *source,
                                                        GError **error);

/* =================== Podcasts Plugin  =============== */

static gboolean
grl_podcasts_plugin_init (GrlRegistry *registry,
                          GrlPlugin *plugin,
                          GList *configs)
{
  GrlConfig *config;
  gint config_count;
  gint cache_time;

  GRL_LOG_DOMAIN_INIT (podcasts_log_domain, "podcasts");

  GRL_DEBUG ("podcasts_plugin_init");

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  GrlPodcastsSource *source = grl_podcasts_source_new ();
  g_object_add_weak_pointer (G_OBJECT (source), (gpointer *) &source);
  grl_registry_register_source (registry,
                                plugin,
                                GRL_SOURCE (source),
                                NULL);
  if (source == NULL)
    return TRUE;
  g_object_remove_weak_pointer (G_OBJECT (source), (gpointer *) &source);

  source->priv->cache_time = DEFAULT_CACHE_TIME;
  if (!configs || !configs->data) {
    return TRUE;
  }

  config_count = g_list_length (configs);
  if (config_count > 1) {
    GRL_INFO ("Provided %d configs, but will only use one", config_count);
  }

  config = GRL_CONFIG (configs->data);

  cache_time = grl_config_get_int (config, "cache-time");
  if (cache_time <= 0) {
    /* Disable cache */
    source->priv->cache_time = 0;
    GRL_INFO ("Disabling cache");
  } else {
    /* Cache time in seconds */
    source->priv->cache_time = cache_time;
    GRL_INFO ("Setting cache time to %d seconds", cache_time);
  }

  return TRUE;
}

GRL_PLUGIN_DEFINE (GRL_MAJOR,
                   GRL_MINOR,
                   PODCASTS_PLUGIN_ID,
                   "Podcasts",
                   "A plugin for browsing podcasts",
                   "Igalia S.L.",
                   VERSION,
                   "LGPL-2.1-or-later",
                   "http://www.igalia.com",
                   grl_podcasts_plugin_init,
                   NULL,
                   NULL);

/* ================== Podcasts GObject ================ */

G_DEFINE_TYPE_WITH_PRIVATE (GrlPodcastsSource, grl_podcasts_source, GRL_TYPE_SOURCE)

static GrlPodcastsSource *
grl_podcasts_source_new (void)
{
  const char *tags[] = {
    "net:internet",
    NULL
  };
  GRL_DEBUG ("grl_podcasts_source_new");
  return g_object_new (GRL_PODCASTS_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       "source-tags", tags,
		       NULL);
}

static void
grl_podcasts_source_class_init (GrlPodcastsSourceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

  gobject_class->finalize = grl_podcasts_source_finalize;

  source_class->supported_keys = grl_podcasts_source_supported_keys;
  source_class->browse = grl_podcasts_source_browse;
  source_class->search = grl_podcasts_source_search;
  source_class->query = grl_podcasts_source_query;
  source_class->resolve = grl_podcasts_source_resolve;
  source_class->store = grl_podcasts_source_store;
  source_class->remove = grl_podcasts_source_remove;
  source_class->notify_change_start = grl_podcasts_source_notify_change_start;
  source_class->notify_change_stop = grl_podcasts_source_notify_change_stop;
}

static void
grl_podcasts_source_init (GrlPodcastsSource *source)
{
  gint r;
  gchar *path;
  gchar *db_path;
  gchar *sql_error = NULL;

  source->priv = grl_podcasts_source_get_instance_private (source);

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
    source->priv->db = NULL;
    return;
  }
  GRL_DEBUG ("  OK");

  GRL_DEBUG ("Checking database tables...");
  r = sqlite3_exec (source->priv->db, GRL_SQL_CREATE_TABLE_PODCASTS,
		    NULL, NULL, &sql_error);

  if (!r) {
    /* TODO: if this fails, sqlite stays in an unreliable state fix that */
    r = sqlite3_exec (source->priv->db, GRL_SQL_CREATE_TABLE_STREAMS,
		      NULL, NULL, &sql_error);
  }
  if (r) {
    if (sql_error) {
      GRL_WARNING ("Failed to create database tables: %s", sql_error);
      g_clear_pointer (&sql_error, sqlite3_free);
    } else {
      GRL_WARNING ("Failed to create database tables.");
    }
    sqlite3_close (source->priv->db);
    source->priv->db = NULL;
    return;
  }
  GRL_DEBUG ("  OK");
}

static void
grl_podcasts_source_finalize (GObject *object)
{
  GrlPodcastsSource *source;

  GRL_DEBUG ("grl_podcasts_source_finalize");

  source = GRL_PODCASTS_SOURCE (object);

  g_clear_object (&source->priv->wc);

  if (source->priv->db)
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
  g_slice_free (Entry, entry);
}

static void
free_podcast_data (PodcastData *data)
{
  g_free (data->image);
  g_free (data->desc);
  g_free (data->published);
  g_slice_free (PodcastData, data);
}

static void
read_done_cb (GObject *source_object,
              GAsyncResult *res,
              gpointer user_data)
{
  AsyncReadCb *arc = (AsyncReadCb *) user_data;
  GError *wc_error = NULL;
  gchar *content = NULL;

  GRL_DEBUG ("  Done");

  grl_net_wc_request_finish (GRL_NET_WC (source_object),
                         res,
                         &content,
                         NULL,
                         &wc_error);
  if (wc_error) {
    GRL_WARNING ("Failed to open '%s': %s", arc->url, wc_error->message);
    g_error_free (wc_error);
  } else {
    arc->callback (content, arc->user_data);
  }
  g_free (arc->url);
  g_slice_free (AsyncReadCb, arc);
}

static void
read_url_async (GrlPodcastsSource *source,
                const gchar *url,
                AsyncReadCbFunc callback,
                gpointer user_data)
{
  AsyncReadCb *arc;

  GRL_DEBUG ("Opening async '%s'", url);

  arc = g_slice_new0 (AsyncReadCb);
  arc->url = g_strdup (url);
  arc->callback = callback;
  arc->user_data = user_data;

  /* We would need a different Wc if we change of URL.
   * In this case, as we don't know the previous URL,
   * we ditch the Wc and create another. It's cheap.
   */
  g_clear_object (&source->priv->wc);
  source->priv->wc = grl_net_wc_new ();
  grl_net_wc_request_async (source->priv->wc, url, NULL, read_done_cb, arc);
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
  return mime && g_str_has_prefix (mime, "video/");
}

static gboolean
mime_is_audio (const gchar *mime)
{
  return mime && g_str_has_prefix (mime, "audio/");
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
             const gchar *image,
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
      media = grl_media_container_new ();
    }

    grl_media_set_id (media, id);
    if (desc)
      grl_media_set_description (media, desc);
    grl_media_set_childcount (media, childcount);
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
    if (date) {
      guint64 epoch;
      epoch = totem_pl_parser_parse_date (date, FALSE);
      if (epoch != -1) {
        GDateTime *time;
        time = g_date_time_new_from_unix_utc (epoch);
        grl_media_set_publication_date (media, time);
        g_date_time_unref (time);
      }
    }
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
  if (image)
    grl_media_add_thumbnail (media, image);

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
		       entry->image, duration, 0);
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
  gchar *image;
  guint duration;
  guint childcount;

  if (is_podcast) {
    id = (gchar *) sqlite3_column_text (sql_stmt, PODCAST_ID);
    title = (gchar *) sqlite3_column_text (sql_stmt, PODCAST_TITLE);
    url = (gchar *) sqlite3_column_text (sql_stmt, PODCAST_URL);
    desc = (gchar *) sqlite3_column_text (sql_stmt, PODCAST_DESC);
    image = (gchar *) sqlite3_column_text (sql_stmt, PODCAST_IMAGE);
    childcount = (guint) sqlite3_column_int (sql_stmt, PODCAST_LAST);
    media = build_media (content, is_podcast, id,
                         title, url, desc, NULL, NULL, image, 0, childcount);
  } else {
    mime = (gchar *) sqlite3_column_text (sql_stmt, STREAM_MIME);
    url = (gchar *) sqlite3_column_text (sql_stmt, STREAM_URL);
    title = (gchar *) sqlite3_column_text (sql_stmt, STREAM_TITLE);
    date = (gchar *) sqlite3_column_text (sql_stmt, STREAM_DATE);
    desc = (gchar *) sqlite3_column_text (sql_stmt, STREAM_DESC);
    duration = sqlite3_column_int (sql_stmt, STREAM_LENGTH);
    image = (gchar *) sqlite3_column_text (sql_stmt, STREAM_IMAGE);
    media = build_media (content, is_podcast, url,
                         title, url, desc, mime, date, image, duration, 0);
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

  GRL_DEBUG ("produce_podcast_contents_from_db");

  db = GRL_PODCASTS_SOURCE (os->source)->priv->db;
  /* Check if searching or browsing */
  if (os->is_query) {
    if (os->text) {
      /* Search text */
      sql = g_strdup_printf (GRL_SQL_GET_PODCAST_STREAMS_BY_TEXT,
                             os->text, os->text, os->text, os->text,
                             os->count, os->skip);
    } else {
      /* Return all */
      sql = g_strdup_printf (GRL_SQL_GET_PODCAST_STREAMS_ALL,
                             os->count, os->skip);
    }
  } else {
    sql = g_strdup_printf (GRL_SQL_GET_PODCAST_STREAMS,
                           os->media_id, os->count, os->skip);
  }
  GRL_DEBUG ("%s", sql);
  r = sqlite3_prepare_v2 (db, sql, strlen (sql), &sql_stmt, NULL);
  g_free (sql);

  if (r != SQLITE_OK) {
    GRL_WARNING ("Failed to retrieve podcast streams: %s", sqlite3_errmsg (db));
    error = g_error_new (GRL_CORE_ERROR,
                         os->error_code,
                         _("Failed to get podcast streams: %s"),
                         sqlite3_errmsg (db));
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
    GRL_WARNING ("Failed to retrieve podcast streams: %s", sqlite3_errmsg (db));
    error = g_error_new (GRL_CORE_ERROR,
                         os->error_code,
                         _("Failed to get podcast streams: %s"),
                         sqlite3_errmsg (db));
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
  GRL_DEBUG ("%s", sql);
  r = sqlite3_exec (db, sql, NULL, NULL, &sql_error);
  g_free (sql);
  if (r) {
    GRL_WARNING ("Failed to remove podcast streams cache: %s", sql_error);
    *error = g_error_new (GRL_CORE_ERROR,
                          GRL_CORE_ERROR_REMOVE_FAILED,
                          _("Failed to remove: %s"),
                          sql_error);
    sqlite3_free (error);
  }
}

static void
remove_podcast (GrlPodcastsSource *podcasts_source,
                const gchar *podcast_id,
                GError **error)
{
  gint r;
  gchar *sql_error;
  gchar *sql;

  GRL_DEBUG ("remove_podcast");

  remove_podcast_streams (podcasts_source->priv->db, podcast_id, error);
  if (*error) {
    return;
  }

  sql = g_strdup_printf (GRL_SQL_REMOVE_PODCAST, podcast_id);
  GRL_DEBUG ("%s", sql);
  r = sqlite3_exec (podcasts_source->priv->db, sql, NULL, NULL, &sql_error);
  g_free (sql);

  if (r != SQLITE_OK) {
    GRL_WARNING ("Failed to remove podcast '%s': %s", podcast_id, sql_error);
    g_set_error (error,
                 GRL_CORE_ERROR,
                 GRL_CORE_ERROR_REMOVE_FAILED,
                 _("Failed to remove: %s"),
                 sql_error);
    sqlite3_free (sql_error);
  } else if (podcasts_source->priv->notify_changes) {
    grl_source_notify_change (GRL_SOURCE (podcasts_source),
                              NULL,
                              GRL_CONTENT_REMOVED,
                              TRUE);
  }
}

static void
remove_stream (GrlPodcastsSource *podcasts_source,
               const gchar *url,
               GError **error)
{
  gint r;
  gchar *sql_error;
  gchar *sql;

  GRL_DEBUG ("remove_stream");

  sql = g_strdup_printf (GRL_SQL_REMOVE_STREAM, url);
  GRL_DEBUG ("%s", sql);
  r = sqlite3_exec (podcasts_source->priv->db, sql, NULL, NULL, &sql_error);
  g_free (sql);

  if (r != SQLITE_OK) {
    GRL_WARNING ("Failed to remove podcast stream '%s': %s", url, sql_error);
    g_set_error (error,
                 GRL_CORE_ERROR,
                 GRL_CORE_ERROR_REMOVE_FAILED,
                 _("Failed to remove: %s"),
                 sql_error);
    sqlite3_free (sql_error);
  } else if (podcasts_source->priv->notify_changes) {
    grl_source_notify_change (GRL_SOURCE (podcasts_source),
                              NULL,
                              GRL_CONTENT_REMOVED,
                              TRUE);
  }
}

static void
store_podcast (GrlPodcastsSource *podcasts_source,
               GList **keylist,
               GrlMedia *podcast,
               GError **error)
{
  gint r;
  sqlite3_stmt *sql_stmt = NULL;
  const gchar *title;
  const gchar *url;
  const gchar *desc;
  gchar *id;

  GRL_DEBUG ("store_podcast");

  title = grl_media_get_title (podcast);
  url = grl_media_get_url (podcast);
  desc = grl_media_get_description (podcast);

  GRL_DEBUG ("%s", GRL_SQL_STORE_PODCAST);
  r = sqlite3_prepare_v2 (podcasts_source->priv->db,
			  GRL_SQL_STORE_PODCAST,
			  strlen (GRL_SQL_STORE_PODCAST),
			  &sql_stmt, NULL);
  if (r != SQLITE_OK) {
    GRL_WARNING ("Failed to store podcast '%s': %s", title,
                 sqlite3_errmsg (podcasts_source->priv->db));
    g_set_error (error,
                 GRL_CORE_ERROR,
                 GRL_CORE_ERROR_STORE_FAILED,
                 _("Failed to store: %s"),
                 sqlite3_errmsg (podcasts_source->priv->db));
    return;
  }

  sqlite3_bind_text (sql_stmt, 1, url, -1, SQLITE_STATIC);
  *keylist = g_list_remove (*keylist,
                            GRLKEYID_TO_POINTER (GRL_METADATA_KEY_URL));

  if (title) {
    sqlite3_bind_text (sql_stmt, 2, title, -1, SQLITE_STATIC);
    *keylist = g_list_remove (*keylist,
                              GRLKEYID_TO_POINTER (GRL_METADATA_KEY_TITLE));
  } else {
    sqlite3_bind_text (sql_stmt, 2, url, -1, SQLITE_STATIC);
  }

  if (desc) {
    sqlite3_bind_text (sql_stmt, 3, desc, -1, SQLITE_STATIC);
    *keylist = g_list_remove (*keylist,
                              GRLKEYID_TO_POINTER (GRL_METADATA_KEY_DESCRIPTION));
  } else {
    sqlite3_bind_text (sql_stmt, 3, "", -1, SQLITE_STATIC);
  }

  while ((r = sqlite3_step (sql_stmt)) == SQLITE_BUSY);

  if (r != SQLITE_DONE) {
    GRL_WARNING ("Failed to store podcast '%s': %s", title,
                 sqlite3_errmsg (podcasts_source->priv->db));
    g_set_error (error,
                 GRL_CORE_ERROR,
                 GRL_CORE_ERROR_STORE_FAILED,
                 _("Failed to store: %s"),
                 sqlite3_errmsg (podcasts_source->priv->db));
    sqlite3_finalize (sql_stmt);
    return;
  }

  sqlite3_finalize (sql_stmt);

  id = g_strdup_printf ("%llu",
                        sqlite3_last_insert_rowid (podcasts_source->priv->db));
  grl_media_set_id (podcast, id);
  g_free (id);

  if (podcasts_source->priv->notify_changes) {
    grl_source_notify_change (GRL_SOURCE (podcasts_source),
                              NULL,
                              GRL_CONTENT_ADDED,
                              FALSE);
  }
}

static void
store_stream (sqlite3 *db, const gchar *podcast_id, Entry *entry)
{
  gint r;
  guint seconds;
  sqlite3_stmt *sql_stmt = NULL;

  if (!entry->url || entry->url[0] == '\0') {
    GRL_DEBUG ("Podcast stream has no URL, skipping");
    return;
  }

  seconds = duration_to_seconds (entry->duration);
  GRL_DEBUG ("%s", GRL_SQL_STORE_STREAM);
  r = sqlite3_prepare_v2 (db,
			  GRL_SQL_STORE_STREAM,
			  strlen (GRL_SQL_STORE_STREAM),
			  &sql_stmt, NULL);
  if (r != SQLITE_OK) {
    GRL_WARNING ("Failed to store podcast stream '%s': %s",
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
  sqlite3_bind_text (sql_stmt, 8, entry->image, -1, SQLITE_STATIC);

  while ((r = sqlite3_step (sql_stmt)) == SQLITE_BUSY);

  if (r != SQLITE_DONE) {
    GRL_WARNING ("Failed to store podcast stream '%s': %s",
                 entry->url, sqlite3_errmsg (db));
  }

  sqlite3_finalize (sql_stmt);
}

static PodcastData *
parse_podcast_data (xmlDocPtr doc, xmlXPathObjectPtr xpathObj)
{
  xmlNodeSetPtr nodes;
  xmlNodePtr node;
  PodcastData *podcast_data = NULL;

  nodes = xpathObj->nodesetval;
  if (!nodes || !nodes->nodeTab) {
    return NULL;
  }

  /* Loop through the podcast data (we skip the "item" tags, since
     the podcast entries will be parsed later on */

  /* At the moment we are only interested in
     'image', 'description' and 'pubDate' tags */
  podcast_data = g_slice_new0 (PodcastData);
  node = nodes->nodeTab[0]->xmlChildrenNode;
  while (node && xmlStrcmp (node->name, (const xmlChar *) "item")) {
    if (!xmlStrcmp (node->name, (const xmlChar *) "image")) {
      xmlNodePtr imgNode = node->xmlChildrenNode;
      while (imgNode && xmlStrcmp (imgNode->name, (const xmlChar *) "url")) {
        imgNode = imgNode->next;
      }
      if (imgNode) {
        podcast_data->image =
          (gchar *) xmlNodeListGetString (doc, imgNode->xmlChildrenNode, 1);
      }
    } else if (!xmlStrcmp (node->name, (const xmlChar *) "description")) {
      podcast_data->desc =
	(gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    } else if (!xmlStrcmp (node->name, (const xmlChar *) "pubDate")) {
      podcast_data->published =
	(gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    }
    node = node->next;
  }

  return podcast_data;
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
    } else if (!xmlStrcmp (node->name, (const xmlChar *) "image")) {
      if (!data->image) {
        data->image = (gchar *) xmlGetProp (node, (xmlChar *) "href");
      }
    } else if (!xmlStrcmp (node->name, (const xmlChar *) "thumbnail")) {
      g_clear_pointer (&data->image, g_free);
      data->image = (gchar *) xmlGetProp (node, (xmlChar *) "url");
    }
    node = node->next;
  }
}

static void
touch_podcast (sqlite3 *db, const gchar *podcast_id, PodcastData *data)
{
  gint r;
  sqlite3_stmt *sql_stmt = NULL;
  GTimeVal now;
  gchar *now_str;
  gchar *img;
  gchar *desc;

  GRL_DEBUG ("touch_podcast");

  g_get_current_time (&now);
  now_str = g_time_val_to_iso8601 (&now);
  desc = data->desc ? data->desc : "";
  img = data->image ? data->image : "";

  r = sqlite3_prepare_v2 (db,
			  GRL_SQL_TOUCH_PODCAST,
			  strlen (GRL_SQL_TOUCH_PODCAST),
			  &sql_stmt, NULL);
  if (r != SQLITE_OK) {
    GRL_WARNING ("Failed to touch podcast '%s': %s",
                 podcast_id, sqlite3_errmsg (db));
  } else {
    sqlite3_bind_text (sql_stmt, 1, now_str, -1, SQLITE_STATIC);
    sqlite3_bind_text (sql_stmt, 2, desc, -1, SQLITE_STATIC);
    sqlite3_bind_text (sql_stmt, 3, img, -1, SQLITE_STATIC);
    sqlite3_bind_text (sql_stmt, 4, podcast_id, -1, SQLITE_STATIC);

    while ((r = sqlite3_step (sql_stmt)) == SQLITE_BUSY);
    if (r != SQLITE_DONE) {
      GRL_WARNING ("Failed to touch podcast '%s': %s", podcast_id,
                   sqlite3_errmsg (db));
    }

    sqlite3_finalize (sql_stmt);
  }

  g_free (now_str);
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
  Entry *entry = g_slice_new0 (Entry);
  if (nodes->nodeTab) {
    parse_entry (osp->doc, nodes->nodeTab[osp->parse_index], entry);
  }
  if (0) print_entry (entry);

  /* Check if entry is valid */
  if (!entry->url || entry->url[0] == '\0') {
    GRL_DEBUG ("Podcast stream has no URL, skipping");
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
    /* Notify about changes */
    if (GRL_PODCASTS_SOURCE (osp->os->source)->priv->notify_changes) {
      media = grl_media_container_new ();
      grl_media_set_id (media, osp->os->media_id);
      grl_source_notify_change (GRL_SOURCE (osp->os->source),
                                media,
                                GRL_CONTENT_CHANGED,
                                FALSE);
      g_object_unref (media);
    }
    g_slice_free (OperationSpec, osp->os);
    xmlXPathFreeObject (osp->xpathObj);
    xmlXPathFreeContext (osp->xpathCtx);
    xmlFreeDoc (osp->doc);
    g_slice_free (OperationSpecParse, osp);
  }

  return osp->parse_index < osp->parse_count;
}

static void
parse_feed (OperationSpec *os, const gchar *str, GError **error)
{
  GrlPodcastsSource *source;
  GrlMedia *podcast = NULL;
  xmlDocPtr doc = NULL;
  xmlXPathContextPtr xpathCtx = NULL;
  xmlXPathObjectPtr xpathObj = NULL;
  guint stream_count;
  PodcastData *podcast_data = NULL;
  guint id;

  GRL_DEBUG ("parse_feed");

  source = GRL_PODCASTS_SOURCE (os->source);

  doc = xmlParseDoc ((xmlChar *) str);
  if (!doc) {
    *error = g_error_new_literal (GRL_CORE_ERROR,
                                  os->error_code,
                                  _("Failed to parse content"));
    goto free_resources;
  }

  /* Get feed stream list */
  xpathCtx = xmlXPathNewContext (doc);
  if (!xpathCtx) {
    *error = g_error_new_literal (GRL_CORE_ERROR,
                                  os->error_code,
                                  _("Failed to parse content"));
    goto free_resources;
  }

  /* Check podcast data */
  xpathObj = xmlXPathEvalExpression ((xmlChar *) "/rss/channel",
				     xpathCtx);
  if(xpathObj == NULL) {
    *error = g_error_new_literal (GRL_CORE_ERROR,
                                  os->error_code,
                                  _("Failed to parse content"));
    goto free_resources;
  }

  podcast_data = parse_podcast_data (doc, xpathObj);
  xmlXPathFreeObject (xpathObj);
  xpathObj = NULL;

  if(podcast_data == NULL) {
    *error = g_error_new_literal (GRL_CORE_ERROR,
                                  os->error_code,
                                  _("Failed to parse podcast contents"));
    goto free_resources;
  }

  /* Check podcast pubDate (if available), if it has not been updated
     recently then we can use the cache and avoid parsing the feed */
  if (podcast_data->published != NULL) {
    guint64 pub_time;
    pub_time = totem_pl_parser_parse_date (podcast_data->published, FALSE);
    if (pub_time != -1) {
      GRL_DEBUG ("Invalid podcast pubDate: '%s'", podcast_data->published);
      /* We will parse the feed again just in case */
    } else if (os->last_refreshed >= pub_time) {
      GRL_DEBUG ("Podcast feed is up-to-date");
      /* We do not need to parse again, we already have the contents in cache */
      produce_podcast_contents_from_db (os);
      g_slice_free (OperationSpec, os);
      goto free_resources;
    }
  }

  /* The podcast has been updated since the last time
     we processed it, we have to parse it again */

  xpathObj = xmlXPathEvalExpression ((xmlChar *) "/rss/channel/item",
				     xpathCtx);
  if(xpathObj == NULL) {
    *error = g_error_new_literal (GRL_CORE_ERROR,
                                  os->error_code,
                                  _("Failed to parse podcast contents"));
    goto free_resources;
  }

  /* Feed is ok, let's process it */

  /* First, remove old entries for this podcast */
  remove_podcast_streams (source->priv->db,  os->media_id, error);
  if (*error) {
    (*error)->code = os->error_code;
    goto free_resources;
  }

  /* Then update the podcast data, including the last_refreshed date */
  touch_podcast (source->priv->db, os->media_id, podcast_data);

  /* If the feed contains no streams, notify and bail out */
  stream_count = xpathObj->nodesetval ? xpathObj->nodesetval->nodeNr : 0;
  GRL_DEBUG ("Got %d streams", stream_count);

  if (stream_count <= 0) {
    if (GRL_PODCASTS_SOURCE (os->source)->priv->notify_changes) {
      podcast = grl_media_container_new ();
      grl_media_set_id (podcast, os->media_id);
      grl_source_notify_change (GRL_SOURCE (os->source),
                                podcast,
                                GRL_CONTENT_CHANGED,
                                FALSE);
      g_object_unref (podcast);
    }
    os->callback (os->source,
		  os->operation_id,
		  NULL,
		  0,
		  os->user_data,
		  NULL);
    goto free_resources;
  }

  /* Otherwise parse the streams in idle loop to prevent blocking */
  OperationSpecParse *osp = g_slice_new0 (OperationSpecParse);
  osp->os = os;
  osp->doc = doc;
  osp->xpathCtx = xpathCtx;
  osp->xpathObj = xpathObj;
  osp->parse_count = stream_count;
  id = g_idle_add (parse_entry_idle, osp);
  g_source_set_name_by_id (id, "[podcasts] parse_entry_idle");
  return;

 free_resources:
  g_clear_pointer (&podcast_data, free_podcast_data);
  g_clear_pointer (&xpathObj, xmlXPathFreeObject);
  g_clear_pointer (&xpathCtx, xmlXPathFreeContext);
  g_clear_pointer (&doc, xmlFreeDoc);
}

static void
read_feed_cb (gchar *xmldata, gpointer user_data)
{
  GError *error = NULL;
  OperationSpec *os = (OperationSpec *) user_data;

  if (!xmldata) {
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_BROWSE_FAILED,
                                 _("Empty response"));
  } else {
    parse_feed (os, xmldata, &error);
  }

  if (error) {
    os->callback (os->source,
		  os->operation_id,
		  NULL,
		  0,
		  os->user_data,
		  error);
    g_error_free (error);
    g_slice_free (OperationSpec, os);
  }
}

static sqlite3_stmt *
get_podcast_info (sqlite3 *db, const gchar *podcast_id)
{
  gint r;
  sqlite3_stmt *sql_stmt = NULL;
  gchar *sql;

  GRL_DEBUG ("get_podcast_info");

  sql = g_strdup_printf (GRL_SQL_GET_PODCAST_BY_ID, podcast_id);
  GRL_DEBUG ("%s", sql);
  r = sqlite3_prepare_v2 (db, sql, strlen (sql), &sql_stmt, NULL);
  g_free (sql);

  if (r != SQLITE_OK) {
    GRL_WARNING ("Failed to retrieve podcast '%s': %s",
                 podcast_id, sqlite3_errmsg (db));
    return NULL;
  }

  while ((r = sqlite3_step (sql_stmt)) == SQLITE_BUSY);

  if (r == SQLITE_ROW) {
    return sql_stmt;
  } else {
    GRL_WARNING ("Failed to retrieve podcast information: %s",
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

  GRL_DEBUG ("produce_podcast_contents");

  /* First we get some information about the podcast */
  db = GRL_PODCASTS_SOURCE (os->source)->priv->db;
  sql_stmt = get_podcast_info (db, os->media_id);
  if (sql_stmt) {
    gchar *lr_str;
    GTimeVal lr;
    GTimeVal now;

    /* Check if we have to refresh the podcast */
    lr_str = (gchar *) sqlite3_column_text (sql_stmt, PODCAST_LAST_REFRESHED);
    GRL_DEBUG ("Podcast last-refreshed: '%s'", lr_str);
    g_time_val_from_iso8601 (lr_str ? lr_str : "", &lr);
    os->last_refreshed = lr.tv_sec;
    g_get_current_time (&now);
    now.tv_sec -= GRL_PODCASTS_SOURCE (os->source)->priv->cache_time;
    if (lr_str == NULL || now.tv_sec >= lr.tv_sec) {
      /* We have to read the podcast feed again */
      GRL_DEBUG ("Refreshing podcast '%s'...", os->media_id);
      url = g_strdup ((gchar *) sqlite3_column_text (sql_stmt, PODCAST_URL));
      read_url_async (GRL_PODCASTS_SOURCE (os->source), url, read_feed_cb, os);
      g_free (url);
    } else {
      /* We can read the podcast entries from the database cache */
      produce_podcast_contents_from_db (os);
      g_slice_free (OperationSpec, os);
    }
    sqlite3_finalize (sql_stmt);
  } else {
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 os->error_code,
                                 _("Failed to get podcast information"));
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
    g_slice_free (OperationSpec, os);
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

  GRL_DEBUG ("produce_podcasts");

  db = GRL_PODCASTS_SOURCE (os->source)->priv->db;

  if (os->is_query) {
    /* Query */
    sql = g_strdup_printf (GRL_SQL_GET_PODCASTS_BY_QUERY,
			   os->text, os->count, os->skip);
  } else {
    /* Browse */
    sql = g_strdup_printf (GRL_SQL_GET_PODCASTS, os->count, os->skip);
  }
  GRL_DEBUG ("%s", sql);
  r = sqlite3_prepare_v2 (db, sql, strlen (sql), &sql_stmt, NULL);
  g_free (sql);

  if (r != SQLITE_OK) {
    GRL_WARNING ("Failed to retrieve podcasts: %s", sqlite3_errmsg (db));
    error = g_error_new (GRL_CORE_ERROR,
                         os->error_code,
                         _("Failed to get podcasts list: %s"),
                         sqlite3_errmsg (db));
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
    GRL_WARNING ("Failed to retrieve podcasts: %s", sqlite3_errmsg (db));
    error = g_error_new (GRL_CORE_ERROR,
                         os->error_code,
                         _("Failed to get podcasts list: %s"),
                         sqlite3_errmsg (db));
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
  g_clear_pointer (&sql_stmt, sqlite3_finalize);
}

static void
stream_resolve (GrlSourceResolveSpec *rs)
{
  gint r;
  sqlite3_stmt *sql_stmt = NULL;
  sqlite3 *db;
  GError *error = NULL;
  gchar *sql;
  const gchar *id;

  GRL_DEBUG (__FUNCTION__);

  db = GRL_PODCASTS_SOURCE (rs->source)->priv->db;

  id = grl_media_get_id (rs->media);
  sql = g_strdup_printf (GRL_SQL_GET_PODCAST_STREAM, id);
  GRL_DEBUG ("%s", sql);
  r = sqlite3_prepare_v2 (db, sql, strlen (sql), &sql_stmt, NULL);
  g_free (sql);

  if (r != SQLITE_OK) {
    GRL_WARNING ("Failed to get podcast stream: %s", sqlite3_errmsg (db));
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_RESOLVE_FAILED,
                                 _("Failed to get podcast stream metadata"));
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);
    g_error_free (error);
    return;
  }

  while ((r = sqlite3_step (sql_stmt)) == SQLITE_BUSY);

  if (r == SQLITE_ROW) {
    build_media_from_stmt (rs->media, sql_stmt, FALSE);
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
  } else {
    GRL_WARNING ("Failed to get podcast stream: %s", sqlite3_errmsg (db));
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_RESOLVE_FAILED,
                                 _("Failed to get podcast stream metadata"));
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);
    g_error_free (error);
  }

  sqlite3_finalize (sql_stmt);
}

static void
podcast_resolve (GrlSourceResolveSpec *rs)
{
  sqlite3_stmt *sql_stmt = NULL;
  sqlite3 *db;
  GError *error = NULL;
  const gchar *id;

  GRL_DEBUG (__FUNCTION__);

  db = GRL_PODCASTS_SOURCE (rs->source)->priv->db;

  id = grl_media_get_id (rs->media);
  if (!id) {
    /* Root category: special case */
    grl_media_set_title (rs->media, GRL_ROOT_TITLE);
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
    return;
  }

  sql_stmt = get_podcast_info (db, id);

  if (sql_stmt) {
    build_media_from_stmt (rs->media, sql_stmt, TRUE);
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
    sqlite3_finalize (sql_stmt);
  } else {
    GRL_WARNING ("Failed to get podcast: %s", sqlite3_errmsg (db));
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_RESOLVE_FAILED,
                                 _("Failed to get podcast metadata"));
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);
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
grl_podcasts_source_supported_keys (GrlSource *source)
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
grl_podcasts_source_browse (GrlSource *source,
                            GrlSourceBrowseSpec *bs)
{
  GRL_DEBUG ("grl_podcasts_source_browse");

  OperationSpec *os;
  GrlPodcastsSource *podcasts_source;
  GError *error = NULL;

  podcasts_source = GRL_PODCASTS_SOURCE (source);
  if (!podcasts_source->priv->db) {
    GRL_WARNING ("Can't execute operation: no database connection.");
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_BROWSE_FAILED,
                                 _("No database connection"));
    bs->callback (bs->source, bs->operation_id, NULL, 0, bs->user_data, error);
    g_error_free (error);
    return;
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

  if (!os->media_id) {
    /* Browsing podcasts list */
    produce_podcasts (os);
    g_slice_free (OperationSpec, os);
  } else {
    /* Browsing a particular podcast. We may need to parse
       the feed (async) and in that case we will need to keep
       os, so we do not free os here */
    produce_podcast_contents (os);
  }
}

static void
grl_podcasts_source_search (GrlSource *source,
                            GrlSourceSearchSpec *ss)
{
  GRL_DEBUG (__FUNCTION__);

  GrlPodcastsSource *podcasts_source;
  OperationSpec *os;
  GError *error = NULL;

  podcasts_source = GRL_PODCASTS_SOURCE (source);
  if (!podcasts_source->priv->db) {
    GRL_WARNING ("Can't execute operation: no database connection.");
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_QUERY_FAILED,
                                 _("No database connection"));
    ss->callback (ss->source, ss->operation_id, NULL, 0, ss->user_data, error);
    g_error_free (error);
    return;
  }

  os = g_slice_new0 (OperationSpec);
  os->source = ss->source;
  os->operation_id = ss->operation_id;
  os->text = ss->text;
  os->count = grl_operation_options_get_count (ss->options);
  os->skip = grl_operation_options_get_skip (ss->options);
  os->callback = ss->callback;
  os->user_data = ss->user_data;
  os->is_query = TRUE;
  os->error_code = GRL_CORE_ERROR_SEARCH_FAILED;
  produce_podcast_contents_from_db (os);
  g_slice_free (OperationSpec, os);
}

static void
grl_podcasts_source_query (GrlSource *source, GrlSourceQuerySpec *qs)
{
  GRL_DEBUG ("grl_podcasts_source_query");

  GrlPodcastsSource *podcasts_source;
  OperationSpec *os;
  GError *error = NULL;

  podcasts_source = GRL_PODCASTS_SOURCE (source);
  if (!podcasts_source->priv->db) {
    GRL_WARNING ("Can't execute operation: no database connection.");
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_QUERY_FAILED,
                                 _("No database connection"));
    qs->callback (qs->source, qs->operation_id, NULL, 0, qs->user_data, error);
    g_error_free (error);
    return;
  }

  os = g_slice_new0 (OperationSpec);
  os->source = qs->source;
  os->operation_id = qs->operation_id;
  os->text = qs->query;
  os->count = grl_operation_options_get_count (qs->options);
  os->skip = grl_operation_options_get_skip (qs->options);
  os->callback = qs->callback;
  os->user_data = qs->user_data;
  os->is_query = TRUE;
  os->error_code = GRL_CORE_ERROR_QUERY_FAILED;
  produce_podcasts (os);
  g_slice_free (OperationSpec, os);
}

static void
grl_podcasts_source_resolve (GrlSource *source,
                             GrlSourceResolveSpec *rs)
{
  GRL_DEBUG (__FUNCTION__);

  GrlPodcastsSource *podcasts_source;
  GError *error = NULL;
  const gchar *media_id;

  podcasts_source = GRL_PODCASTS_SOURCE (source);
  if (!podcasts_source->priv->db) {
    GRL_WARNING ("Can't execute operation: no database connection.");
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_RESOLVE_FAILED,
                                 _("No database connection"));
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);
    g_error_free (error);
    return;
  }

  media_id = grl_media_get_id (rs->media);
  if (!media_id || media_id_is_podcast (media_id)) {
    podcast_resolve (rs);
  } else {
    stream_resolve (rs);
  }
}

static void
grl_podcasts_source_store (GrlSource *source, GrlSourceStoreSpec *ss)
{
  GError *error = NULL;
  GList *keylist;

  GRL_DEBUG ("grl_podcasts_source_store");

  keylist = grl_data_get_keys (GRL_DATA (ss->media));

  if (grl_media_is_container (ss->media)) {
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_STORE_FAILED,
                                 _("Cannot create containers. Only feeds are accepted"));
  } else if (!grl_data_has_key (GRL_DATA (ss->media), GRL_METADATA_KEY_URL)) {
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_STORE_FAILED,
                         _("Failed to store: %s"),
                         _("URL required"));
  } else {
    store_podcast (GRL_PODCASTS_SOURCE (ss->source),
                   &keylist,
                   ss->media,
                   &error);
  }

  ss->callback (ss->source, ss->media, keylist, ss->user_data, error);
  g_clear_error (&error);
}

static void
grl_podcasts_source_remove (GrlSource *source,
                            GrlSourceRemoveSpec *rs)
{
  GRL_DEBUG (__FUNCTION__);
  GError *error = NULL;
  if (media_id_is_podcast (rs->media_id)) {
    remove_podcast (GRL_PODCASTS_SOURCE (rs->source),
		    rs->media_id, &error);
  } else {
    remove_stream (GRL_PODCASTS_SOURCE (rs->source),
		   rs->media_id, &error);
  }
  rs->callback (rs->source, rs->media, rs->user_data, error);
  g_clear_error (&error);
}

static gboolean
grl_podcasts_source_notify_change_start (GrlSource *source,
                                         GError **error)
{
  GrlPodcastsSource *podcasts_source = GRL_PODCASTS_SOURCE (source);

  podcasts_source->priv->notify_changes = TRUE;

  return TRUE;
}

static gboolean
grl_podcasts_source_notify_change_stop (GrlSource *source,
                                        GError **error)
{
  GrlPodcastsSource *podcasts_source = GRL_PODCASTS_SOURCE (source);

  podcasts_source->priv->notify_changes = FALSE;

  return TRUE;
}
