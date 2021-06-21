/*
 * Copyright (C) 2013 Victor Toso.
 *
 * Contact: Victor Toso <me@victortoso.com>
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
#include <net/grl-net.h>

#include "grl-magnatune.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT magnatune_log_domain
GRL_LOG_DOMAIN_STATIC(magnatune_log_domain);

/* --- Database --- */

#define GRL_SQL_SONGS_QUERY_ALL                                 \
  "SELECT DISTINCT son.song_id, art.name, alb.name, son.name, " \
    "son.track_no, son.duration, son.mp3 "                      \
  "FROM songs son "                                             \
  "LEFT OUTER JOIN albums alb "                                 \
    "ON (alb.album_id = son.album_id) "                         \
  "LEFT OUTER JOIN artists art "                                \
    "ON (art.artists_id = alb.artist_id) "                      \
  "WHERE (art.name like '%%%s%%') "                             \
    "OR (alb.name like '%%%s%%') "                              \
    "OR (son.name like '%%%s%%') "                              \
    "LIMIT %u OFFSET %u"

#define GRL_SQL_ARTISTS_QUERY_ALL                               \
  "SELECT DISTINCT art.artists_id, art.name "                   \
  "FROM artists art "                                           \
    "LIMIT %u OFFSET %u"

#define GRL_SQL_ALBUMS_QUERY_ALL                                \
  "SELECT DISTINCT alb.album_id, alb.name "                     \
  "FROM albums alb "                                            \
    "LIMIT %u OFFSET %u"

#define GRL_SQL_GENRES_QUERY_ALL                                \
  "SELECT DISTINCT gen.genre_id, gen.name "                     \
  "FROM genres gen "                                            \
    "LIMIT %u OFFSET %u"

#define GRL_SQL_ALBUMS_BY_GENRE                                 \
  "SELECT DISTINCT alb.album_id, alb.name "                     \
  "FROM albums alb "                                            \
  "LEFT OUTER JOIN genres_albums genalb "                       \
    "ON (alb.album_id = genalb.album_id) "                      \
  "WHERE (genalb.genre_id = %u) "                               \
    "LIMIT %u OFFSET %u"

#define GRL_SQL_ALBUMS_BY_ARTIST                                \
  "SELECT DISTINCT alb.album_id, alb.name "                     \
  "FROM albums alb "                                            \
  "WHERE (alb.artist_id = %u) "                                 \
    "LIMIT %u OFFSET %u"

#define GRL_SQL_SONGS_BY_ALBUM                                  \
  "SELECT DISTINCT son.song_id, art.name, alb.name, son.name, " \
    "son.track_no, son.duration, son.mp3 "                      \
  "FROM songs son "                                             \
  "LEFT OUTER JOIN albums alb "                                 \
    "ON (alb.album_id = son.album_id) "                         \
  "LEFT OUTER JOIN artists art "                                \
    "ON (art.artists_id = alb.artist_id) "                      \
  "WHERE (alb.album_id = %u) "                                  \
    "LIMIT %u OFFSET %u"

/* --- Files --- */

#define GRL_SQL_DB      "grl-magnatune.db"
#define GRL_SQL_NEW_DB  "grl-magnatune-new.db"
#define GRL_SQL_CRC     "grl-magnatune-db.crc"
#define GRL_SQL_NEW_CRC "grl-magnatune-new.crc"

/* --- URLs --- */

#define URL_GET_DB     "http://he3.magnatune.com/info/sqlite_normalized.db"
#define URL_GET_CRC    "http://magnatune.com/info/changed.txt"

#define URL_SONG_PLAY  "http://he3.magnatune.com/all"
#define URL_SONG_COVER "http://he3.magnatune.com/music"

/* --- Cover Art --- */

#define URL_SONG_COVER_FORMAT URL_SONG_COVER "/%s/%s/cover_%d.jpg"
static gint cover_art_sizes[] = { 50, 75, 100, 160, 200, 300, 600, 1400 };

/* --- Other --- */

#define DB_UPDATE_TIME_INTERVAL   (60 * 60 * 24 * 7)
#define CRC_UPDATE_TIME_INTERVAL  (60 * 60 * 12)

#define MAGNATUNE_ROOT_ARTIST     _("Artists")
#define MAGNATUNE_ROOT_ALBUM      _("Albums")
#define MAGNATUNE_ROOT_GENRE      _("Genres")

#define MAGNATUNE_NAME_ID_SEP     "-"

/* --- Plugin information --- */

#define SOURCE_ID       "grl-magnatune"
#define SOURCE_NAME     "Magnatune"
#define SOURCE_DESC     _("A source for browsing music")

enum {
  MAGNATUNE_TRACK_ID,
  MAGNATUNE_ARTIST_NAME,
  MAGNATUNE_ALBUM_NAME,
  MAGNATUNE_TRACK_NAME,
  MAGNATUNE_TRACK_NUMBER,
  MAGNATUNE_TRACK_DURATION,
  MAGNATUNE_TRACK_URL_TO_MP3,
};

typedef enum {
  MAGNATUNE_ARTIST_CAT,
  MAGNATUNE_ALBUM_CAT,
  MAGNATUNE_GENRE_CAT,
  MAGNATUNE_NUM_CAT,
} MagnatuneCategory;

struct _GrlMagnatunePrivate {
  sqlite3 *db;
};

struct _OperationSpec;
typedef void (*GrlMagnatuneExecCb)(struct _OperationSpec *);

struct _OperationSpec {
  GrlSource *source;
  guint operation_id;
  const gchar *media_id;
  guint skip;
  guint count;
  const gchar *text;
  GrlMagnatuneExecCb magnatune_cb;
  GrlSourceResultCb callback;
  GrlMedia *container;
  gpointer user_data;
  guint error_code;
};

typedef struct _OperationSpec OperationSpec;

typedef GrlMedia* (MagnatuneBuildMediaFn)(sqlite3_stmt *);

static GrlMagnatuneSource *grl_magnatune_source_new(void);

static void grl_magnatune_source_finalize(GObject *object);

static const GList *grl_magnatune_source_supported_keys(GrlSource *source);

static void grl_magnatune_source_search(GrlSource *source,
                                        GrlSourceSearchSpec *ss);

static void grl_magnatune_source_browse(GrlSource *source,
                                        GrlSourceBrowseSpec *bs);

static void magnatune_get_db_async(OperationSpec *os);

/* ================== Magnatune Plugin  ================= */

static gboolean
grl_magnatune_plugin_init(GrlRegistry *registry,
                          GrlPlugin *plugin,
                          GList *configs)
{
  GrlMagnatuneSource *source;

  GRL_LOG_DOMAIN_INIT(magnatune_log_domain, "magnatune");

  GRL_DEBUG("magnatune_plugin_init");

  source = grl_magnatune_source_new();
  grl_registry_register_source(registry,
                               plugin,
                               GRL_SOURCE(source),
                               NULL);
  return TRUE;
}

GRL_PLUGIN_DEFINE (GRL_MAJOR,
                   GRL_MINOR,
                   MAGNATUNE_PLUGIN_ID,
                   "Magnatune",
                   "A plugin for searching music",
                   "Victor Toso",
                   VERSION,
                   "LGPL-2.1-or-later",
                   "http://www.igalia.com",
                   grl_magnatune_plugin_init,
                   NULL,
                   NULL);

/* ================== Magnatune GObject ================= */

G_DEFINE_TYPE_WITH_PRIVATE (GrlMagnatuneSource, grl_magnatune_source, GRL_TYPE_SOURCE)

static GrlMagnatuneSource *
grl_magnatune_source_new(void)
{
  GObject *object;
  GrlMagnatuneSource *source;
  const char *tags[] = {
    "net:internet",
    NULL
  };

  GRL_DEBUG("magnatune_source_new");

  object = g_object_new(GRL_MAGNATUNE_SOURCE_TYPE,
                        "source-id", SOURCE_ID,
                        "source-name", SOURCE_NAME,
                        "source-desc", SOURCE_DESC,
                        "supported-media", GRL_SUPPORTED_MEDIA_AUDIO,
                        "source-tags", tags,
                        NULL);

  source = GRL_MAGNATUNE_SOURCE(object);

  return source;
}

static void
grl_magnatune_source_class_init(GrlMagnatuneSourceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  GrlSourceClass *source_class = GRL_SOURCE_CLASS(klass);

  gobject_class->finalize = grl_magnatune_source_finalize;

  source_class->supported_keys = grl_magnatune_source_supported_keys;
  source_class->search = grl_magnatune_source_search;
  source_class->browse = grl_magnatune_source_browse;
}

static void
grl_magnatune_source_init(GrlMagnatuneSource *source)
{
  gint ret;
  gchar *path;
  gchar *db_path;
  gchar *crc_path;
  gchar *new_db_path;
  gchar *new_crc_path;

  GRL_DEBUG("magnatune_source_init");

  source->priv = grl_magnatune_source_get_instance_private (source);
  source->priv->db = NULL;

  path = g_build_filename(g_get_user_data_dir(), "grilo-plugins", NULL);
  db_path = g_build_filename(path, GRL_SQL_DB, NULL);
  crc_path = g_build_filename(path, GRL_SQL_CRC, NULL);
  new_db_path = g_build_filename(path, GRL_SQL_NEW_DB, NULL);
  new_crc_path = g_build_filename(path, GRL_SQL_NEW_CRC, NULL);

  if(!g_file_test(path, G_FILE_TEST_IS_DIR)) {
    g_mkdir_with_parents(path, 0775);
  }

  if (g_file_test(db_path, G_FILE_TEST_EXISTS) == TRUE) {
    if (g_file_test(new_db_path, G_FILE_TEST_EXISTS) == TRUE
        && g_rename(new_db_path, db_path) == 0) {
        GRL_DEBUG("New database in use.");
    }

    if (g_file_test(new_crc_path, G_FILE_TEST_EXISTS) == TRUE
        && g_rename(new_crc_path, crc_path) == 0) {
        GRL_DEBUG("New crc file in use.");
    }

    GRL_DEBUG("Opening database connection.");
    ret = sqlite3_open(db_path, &source->priv->db);
    if (ret != SQLITE_OK) {
      GRL_WARNING("Failed to open database '%s': %s",
                  db_path,
                  sqlite3_errmsg(source->priv->db));
      sqlite3_close(source->priv->db);
      source->priv->db = NULL;
    }
  } else {
    GRL_DEBUG("No database was found. Download when user interact.");
  }

  g_free(new_crc_path);
  g_free(new_db_path);
  g_free(crc_path);
  g_free(db_path);
  g_free(path);
}

static void
grl_magnatune_source_finalize(GObject *object)
{
  GrlMagnatuneSource *source;

  GRL_DEBUG("grl_magnatune_source_finalize");

  source = GRL_MAGNATUNE_SOURCE(object);

  if (source->priv->db != NULL) {
    sqlite3_close(source->priv->db);
  }

  G_OBJECT_CLASS(grl_magnatune_source_parent_class)->finalize(object);
}

/* ======================= Utilities ==================== */

static void
magnatune_get_crc_done(GObject *source_object,
                       GAsyncResult *res,
                       gpointer user_data)
{
  gchar *new_crc_path = NULL;
  gchar *content = NULL;
  gsize length = 0;
  gboolean ret = FALSE;
  GError *err = NULL;

  GRL_DEBUG("magnatune_get_crc_done");

  ret = grl_net_wc_request_finish(GRL_NET_WC(source_object),
                                  res,
                                  &content,
                                  &length,
                                  &err);
  g_object_unref(source_object);

  if (ret == TRUE) {
    new_crc_path = g_build_filename(g_get_user_data_dir(), "grilo-plugins",
                                    GRL_SQL_NEW_CRC, NULL);

    ret = g_file_set_contents(new_crc_path,
                              content,
                              length,
                              &err);
    if (ret == FALSE) {
      GRL_WARNING("Failed to save crc-file from magnatune to: '%s' - '%s'",
                  new_crc_path, err->message);
    }
    g_free(new_crc_path);

  } else {
    GRL_WARNING("Failed to get crc-file from magnatune: %s", err->message);
  }
}

static void
magnatune_get_crc_async(void)
{
  GrlNetWc *wc = NULL;

  GRL_DEBUG("magnatune_get_crc_async");

  wc = grl_net_wc_new();
  grl_net_wc_request_async(wc,
                           URL_GET_CRC,
                           NULL,
                           magnatune_get_crc_done,
                           NULL);
}

static void
magnatune_get_db_done(GObject *source_object,
                      GAsyncResult *res,
                      gpointer user_data)
{
  gchar *db_path = NULL;
  gchar *new_db_path = NULL;
  gchar *content = NULL;
  gsize length = 0;
  gboolean ret = FALSE;
  gboolean first_run = FALSE;
  GError *err = NULL;
  GError *err_fn = NULL;
  OperationSpec *os = NULL;
  GrlMagnatuneSource *source = NULL;

  GRL_DEBUG("magnatune_get_db_done");
  os = (OperationSpec *) user_data;
  ret = grl_net_wc_request_finish(GRL_NET_WC(source_object),
                                  res,
                                  &content,
                                  &length,
                                  &err_fn);
  g_object_unref(source_object);

  if (ret == FALSE) {
    err = g_error_new(GRL_CORE_ERROR,
                      GRL_CORE_ERROR_MEDIA_NOT_FOUND,
                      _("Failed to get database from magnatune: %s"),
                      err_fn->message);
    g_error_free(err_fn);

    if (os != NULL)
      os->callback(os->source, os->operation_id, NULL, 0, os->user_data, err);

  } else {
    db_path = g_build_filename(g_get_user_data_dir(), "grilo-plugins",
                               GRL_SQL_DB, NULL);

    /* If this is a first run, new database must be ready to use */
    if (g_file_test(db_path, G_FILE_TEST_EXISTS) == FALSE) {
      new_db_path = db_path;
      first_run = TRUE;
    } else {
      g_free(db_path);
      new_db_path = g_build_filename(g_get_user_data_dir(), "grilo-plugins",
                                     GRL_SQL_NEW_DB, NULL);
    }

    GRL_WARNING("Saving database to path '%s'", new_db_path);
    ret = g_file_set_contents(new_db_path,
                              content,
                              length,
                              &err_fn);

    if (ret == FALSE) {
      err = g_error_new(GRL_CORE_ERROR,
                        GRL_CORE_ERROR_MEDIA_NOT_FOUND,
                        _("Failed to save database from magnatune: “%s”"),
                        err_fn->message);
      g_error_free(err_fn);

      if (os != NULL)
        os->callback(os->source, os->operation_id, NULL, 0, os->user_data, err);

    } else if (first_run == TRUE) {
      source = GRL_MAGNATUNE_SOURCE(os->source);

      if (source->priv->db == NULL) {
        GRL_DEBUG("Opening database connection.");
        if (sqlite3_open(db_path, &source->priv->db) != SQLITE_OK) {
          GRL_WARNING("Failed to open database '%s': %s",
                      db_path,
                      sqlite3_errmsg(source->priv->db));
          sqlite3_close(source->priv->db);
          source->priv->db = NULL;
        }
      }
    }

    g_free(new_db_path);
  }

  if (ret == TRUE && os != NULL) {
    /* execute application's request */
    os->magnatune_cb(os);
  }
}

static void
magnatune_get_db_async(OperationSpec *os)
{
  GrlNetWc *wc = NULL;

  GRL_DEBUG("magnatune_get_db_async");

  wc = grl_net_wc_new();
  grl_net_wc_request_async(wc,
                           URL_GET_DB,
                           NULL,
                           magnatune_get_db_done,
                           os);
}

static void
magnatune_check_update_done(GObject *source_object,
                            GAsyncResult *res,
                            gpointer user_data)
{
  gchar *crc_path = NULL;
  gchar *new_crc_path = NULL;
  gchar *new_crc = NULL;
  gchar *old_crc = NULL;
  gsize length = 0;
  gboolean ret = FALSE;
  GError *err = NULL;

  ret = grl_net_wc_request_finish(GRL_NET_WC(source_object),
                                  res,
                                  &new_crc,
                                  &length,
                                  &err);
  g_object_unref(source_object);

  if (ret == TRUE) {
    new_crc_path = g_build_filename(g_get_user_data_dir(), "grilo-plugins",
                                    GRL_SQL_NEW_CRC, NULL);

    g_file_set_contents(new_crc_path,
                              new_crc,
                              length,
                              &err);

    crc_path = g_build_filename(g_get_user_data_dir(), "grilo-plugins",
                                GRL_SQL_CRC, NULL);

    g_file_get_contents(crc_path,
                        &old_crc,
                        &length,
                        &err);

    if (g_strcmp0(new_crc, old_crc) != 0) {
      magnatune_get_db_async(NULL);
    }

    g_free(new_crc_path);
    g_free(crc_path);
    g_free(old_crc);
  }
}

static void
magnatune_check_update(void)
{
  gchar *db_path = NULL;
  gchar *new_db_path = NULL;
  gchar *new_crc_path = NULL;
  static gboolean already_checked = FALSE;
  struct stat file_st;
  GTimeVal tv;
  GrlNetWc *wc = NULL;

  GRL_DEBUG("magnatune_check_update");

  if (already_checked == TRUE)
    return;

  already_checked = TRUE;

  g_get_current_time(&tv);

  new_db_path = g_build_filename(g_get_user_data_dir(), "grilo-plugins",
                                 GRL_SQL_NEW_DB, NULL);

  if (g_file_test(new_db_path, G_FILE_TEST_EXISTS) == FALSE) {

    db_path = g_build_filename(g_get_user_data_dir(), "grilo-plugins",
                               GRL_SQL_DB, NULL);
    g_stat(db_path, &file_st);
    if (tv.tv_sec - file_st.st_mtime > DB_UPDATE_TIME_INTERVAL) {

      new_crc_path = g_build_filename(g_get_user_data_dir(), "grilo-plugins",
                                      GRL_SQL_NEW_CRC, NULL);
      g_stat(new_crc_path, &file_st);
      if ((g_file_test(new_crc_path, G_FILE_TEST_EXISTS) == FALSE)
           || (tv.tv_sec - file_st.st_mtime > CRC_UPDATE_TIME_INTERVAL)) {

        wc = grl_net_wc_new();
        grl_net_wc_request_async(wc,
                                 URL_GET_CRC,
                                 NULL,
                                 magnatune_check_update_done,
                                 NULL);
      }
      g_free(new_crc_path);
    }
    g_free(db_path);
  }
  g_free(new_db_path);
}

static void
add_cover (gpointer url_to_cover, gpointer media)
{
  grl_media_add_thumbnail((GrlMedia *) media, url_to_cover);
}


static GrlMedia *
build_media(gint track_id,
            const gchar *artist_name,
            const gchar *album_name,
            const gchar *track_name,
            gint track_number,
            gint duration,
            const gchar *url_to_mp3,
            GPtrArray *url_to_covers)
{
  GrlMedia *media = NULL;
  gchar *str_track_id = NULL;

  media = grl_media_audio_new();
  grl_media_set_track_number(media, track_number);
  grl_media_set_artist(media, artist_name);
  grl_media_set_album(media, album_name);
  grl_media_set_url(media, url_to_mp3);
  grl_media_set_duration(media, duration);
  grl_media_set_title(media, track_name);

  g_ptr_array_foreach(url_to_covers, add_cover, media);

  str_track_id = g_strdup_printf("%d", track_id);
  grl_media_set_id(media, str_track_id);
  g_free(str_track_id);

  return media;
}

static GrlMedia *
build_media_track_from_stmt(sqlite3_stmt *sql_stmt)
{
  GrlMedia *media = NULL;

  gint i;
  gint track_id;
  gint duration;
  gint track_number;
  const gchar *artist_name;
  const gchar *album_name;
  const gchar *track_name;
  const gchar *raw_url;
  gchar *encoded_url;
  gchar *url_to_mp3;
  gchar *encoded_artist;
  gchar *encoded_album;
  GPtrArray *url_to_covers;

  track_id = (guint) sqlite3_column_int(sql_stmt, MAGNATUNE_TRACK_ID);
  artist_name = (gchar *) sqlite3_column_text(sql_stmt, MAGNATUNE_ARTIST_NAME);
  album_name = (gchar *) sqlite3_column_text(sql_stmt, MAGNATUNE_ALBUM_NAME);
  duration = (guint) sqlite3_column_int(sql_stmt, MAGNATUNE_TRACK_DURATION);
  track_number = (guint) sqlite3_column_int(sql_stmt, MAGNATUNE_TRACK_NUMBER);
  track_name = (gchar *) sqlite3_column_text(sql_stmt, MAGNATUNE_TRACK_NAME);
  raw_url = (gchar *) sqlite3_column_text(sql_stmt, MAGNATUNE_TRACK_URL_TO_MP3);

  encoded_url = g_uri_escape_string(raw_url, "", FALSE);
  url_to_mp3 = g_strdup_printf("%s/%s", URL_SONG_PLAY, encoded_url);

  encoded_artist = g_uri_escape_string(artist_name, "", FALSE);
  encoded_album = g_uri_escape_string(album_name, "", FALSE);
  url_to_covers = g_ptr_array_new();
  for (i = 0; i < G_N_ELEMENTS(cover_art_sizes); i++) {
    gchar *cover = g_strdup_printf(URL_SONG_COVER_FORMAT, encoded_artist,
                                   encoded_album, cover_art_sizes[i]);
    g_ptr_array_add(url_to_covers, cover);
  }

  media = build_media(track_id, artist_name, album_name, track_name,
                      track_number, duration, url_to_mp3, url_to_covers);

  g_free(encoded_url);
  g_free(url_to_mp3);
  g_free(encoded_artist);
  g_free(encoded_album);
  g_ptr_array_free(url_to_covers, TRUE);

  return media;
}

static GrlMedia*
build_media_id_name_from_stmt(sqlite3_stmt *sql_stmt)
{
  GrlMedia *media = NULL;
  guint media_id = 0;
  gchar *id = NULL;
  const gchar *media_name = NULL;

  media = grl_media_container_new();
  media_id = (guint) sqlite3_column_int(sql_stmt, 0);
  media_name = (gchar *) sqlite3_column_text(sql_stmt, 1);
  id = g_strdup_printf("%d", media_id);
  grl_media_set_id(media, id);
  grl_media_set_title(media, media_name);
  g_free(id);

  return media;
}

static GList*
magnatune_sqlite_execute(OperationSpec *os,
                         gchar *sql,
                         MagnatuneBuildMediaFn build_media_fn,
                         GError **error)
{
  GrlMedia *media = NULL;
  sqlite3 *db = NULL;
  sqlite3_stmt *sql_stmt = NULL;
  gint ret = 0;
  GError *err = NULL;
  GList *list_medias = NULL;

  GRL_DEBUG("magnatune_sqlite_execute");

  db = GRL_MAGNATUNE_SOURCE(os->source)->priv->db;

  ret = sqlite3_prepare_v2(db, sql, strlen(sql), &sql_stmt, NULL);
  if (ret != SQLITE_OK) {
    err = g_error_new(GRL_CORE_ERROR,
                      os->error_code,
                      _("Failed to get table from magnatune db: %s"),
                      sqlite3_errmsg(db));
    goto end_sqlite_execute;
  }

  while ((ret = sqlite3_step(sql_stmt)) == SQLITE_BUSY);

  while (ret == SQLITE_ROW) {
    media = build_media_fn(sql_stmt);
    list_medias = g_list_prepend(list_medias, media);
    ret = sqlite3_step(sql_stmt);
  }

  if (ret != SQLITE_DONE) {
    err = g_error_new(GRL_CORE_ERROR,
                      os->error_code,
                      _("Fail before returning media to user: %s"),
                      sqlite3_errmsg(db));

    g_list_free_full(list_medias, g_object_unref);
    goto end_sqlite_execute;
  }

  list_medias = g_list_reverse(list_medias);

end_sqlite_execute:
  sqlite3_finalize(sql_stmt);

  if (err != NULL) {
    *error = err;
    return NULL;
  }

  return list_medias;
}

static void
magnatune_browse_root(OperationSpec *os)
{
  GrlMedia *media = NULL;
  guint num = 0;
  gchar *id = NULL;

  GRL_DEBUG("magnatune_browse_root");

  if (os->skip > 1 || os->count == 0) {
    os->callback(os->source, os->operation_id, NULL, 0, os->user_data, NULL);
    return;
  }

  num = (os->count > MAGNATUNE_NUM_CAT) ? MAGNATUNE_NUM_CAT: os->count;

  media = grl_media_container_new();
  grl_media_set_title(media, MAGNATUNE_ROOT_ARTIST);
  id = g_strdup_printf("root-%d", MAGNATUNE_ARTIST_CAT);
  grl_media_set_id(media, id);
  num--;
  os->callback(os->source, os->operation_id, media, num, os->user_data, NULL);
  g_free(id);

  if (num == 0)
    return;

  media = grl_media_container_new();
  grl_media_set_title(media, MAGNATUNE_ROOT_ALBUM);
  id = g_strdup_printf("root-%d", MAGNATUNE_ALBUM_CAT);
  grl_media_set_id(media, id);
  num--;
  os->callback(os->source, os->operation_id, media, num, os->user_data, NULL);
  g_free(id);

  if (num == 0)
    return;

  media = grl_media_container_new();
  grl_media_set_title(media, MAGNATUNE_ROOT_GENRE);
  id = g_strdup_printf("root-%d", MAGNATUNE_GENRE_CAT);
  grl_media_set_id(media, id);
  num--;
  os->callback(os->source, os->operation_id, media, num, os->user_data, NULL);
  g_free(id);
}

static void
magnatune_execute_browse(OperationSpec *os)
{
  MagnatuneBuildMediaFn *build_fn;
  GrlMedia *media = NULL;
  const gchar *container_id = NULL;  gchar *sql = NULL;
  gchar **touple = NULL;
  gchar *new_container_id = NULL;
  gchar *category_str_id = NULL;
  gint id = 0;
  gint num_medias = 0;
  static GList *iter = NULL;
  static GList *list_medias = NULL;
  GError *err = NULL;

  GRL_DEBUG("magnatune_execute_browse");

  container_id = grl_media_get_id(os->container);
  if (container_id == NULL) {
    magnatune_browse_root(os);
    goto end_browse;
  }

  touple = g_strsplit_set(container_id, MAGNATUNE_NAME_ID_SEP, 0);
  id = g_ascii_strtoll(touple[1], NULL, 10);
  build_fn = build_media_id_name_from_stmt;

  if (strcmp(touple[0], "root") == 0) {
    switch (id) {
    case MAGNATUNE_ARTIST_CAT:
      category_str_id = g_strdup("artist");
      sql = g_strdup_printf(GRL_SQL_ARTISTS_QUERY_ALL, os->count, os->skip);
      break;

    case MAGNATUNE_ALBUM_CAT:
      category_str_id = g_strdup("album");
      sql = g_strdup_printf(GRL_SQL_ALBUMS_QUERY_ALL, os->count, os->skip);
      break;

    case MAGNATUNE_GENRE_CAT:
      category_str_id = g_strdup("genre");
      sql = g_strdup_printf(GRL_SQL_GENRES_QUERY_ALL, os->count, os->skip);
      break;
    }

  } else if (strcmp(touple[0], "artist") == 0) {
    category_str_id = g_strdup("album");
    sql = g_strdup_printf(GRL_SQL_ALBUMS_BY_ARTIST, id, os->count, os->skip);

  } else if (strcmp(touple[0], "album") == 0) {
    category_str_id = g_strdup("track");
    sql = g_strdup_printf(GRL_SQL_SONGS_BY_ALBUM, id, os->count, os->skip);
    build_fn = build_media_track_from_stmt;

  } else if (strcmp(touple[0], "genre") == 0) {
    category_str_id = g_strdup("album");
    sql = g_strdup_printf(GRL_SQL_ALBUMS_BY_GENRE, id, os->count, os->skip);

  } else {
    err = g_error_new(GRL_CORE_ERROR,
                      GRL_CORE_ERROR_BROWSE_FAILED,
                      _("Invalid container identifier %s"),
                      container_id);
  }
  g_strfreev(touple);

  if (sql == NULL || err != NULL)
    goto end_browse;

  /* We have the right sql-query, execute */
  list_medias = magnatune_sqlite_execute(os, sql, build_fn, &err);
  g_free(sql);

  if (list_medias == NULL)
    goto end_browse;

  num_medias = g_list_length(list_medias) - 1;;
  for (iter = list_medias; iter; iter = iter->next) {
    media = iter->data;
    new_container_id = g_strdup_printf("%s-%s",
                             category_str_id,
                             grl_media_get_id(media));
    grl_media_set_id(media, new_container_id);
    g_free(new_container_id);

    os->callback(os->source,
                 os->operation_id,
                 media,
                 num_medias,
                 os->user_data,
                 NULL);
    num_medias--;
  }

  g_list_free(list_medias);

end_browse:
  if (err != NULL) {
    os->callback(os->source, os->operation_id, NULL, 0, os->user_data, err);
    g_error_free(err);
  }

  g_clear_pointer (&category_str_id, g_free);

  g_slice_free(OperationSpec, os);
}

static void
magnatune_execute_search(OperationSpec *os)
{
  GrlMedia *media = NULL;
  gchar *sql = NULL;
  GList *list_medias = NULL;
  GList *iter = NULL;
  gint num_medias = 0;
  gchar *id = NULL;
  GError *err = NULL;

  GRL_DEBUG("magnatune_execute_search");

  sql = g_strdup_printf(GRL_SQL_SONGS_QUERY_ALL,
                        os->text, os->text, os->text,
                        os->count, os->skip);

  list_medias = magnatune_sqlite_execute(os,
                                         sql,
                                         build_media_track_from_stmt,
                                         &err);
  g_free(sql);

  if (list_medias == NULL)
    goto end_search;

  num_medias = g_list_length(list_medias) - 1;
  for (iter = list_medias; iter; iter = iter->next) {
    media = iter->data;
    id = g_strdup_printf("%s-%s", "track", grl_media_get_id(media));
    grl_media_set_id(media, id);
    g_free(id);
    os->callback(os->source,
                 os->operation_id,
                 media,
                 num_medias,
                 os->user_data,
                 NULL);
    num_medias--;
  }

  g_list_free(list_medias);

  g_slice_free(OperationSpec, os);
  return;

end_search:
  if (err != NULL) {
    os->callback(os->source, os->operation_id, NULL, 0, os->user_data, err);
    g_error_free(err);
  } else {
    os->callback(os->source, os->operation_id, NULL, 0, os->user_data, NULL);
  }

  g_slice_free(OperationSpec, os);
}

/* ================== API Implementation ================ */

static const GList *
grl_magnatune_source_supported_keys(GrlSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new(GRL_METADATA_KEY_ID,
                                     GRL_METADATA_KEY_ARTIST,
                                     GRL_METADATA_KEY_ALBUM,
                                     GRL_METADATA_KEY_DURATION,
                                     GRL_METADATA_KEY_TITLE,
                                     GRL_METADATA_KEY_TRACK_NUMBER,
                                     GRL_METADATA_KEY_URL,
                                     GRL_METADATA_KEY_INVALID);
  }
  return keys;
}

static void
grl_magnatune_source_search(GrlSource *source, GrlSourceSearchSpec *ss)
{
  OperationSpec *os = NULL;

  os = g_slice_new0(OperationSpec);
  os->source = ss->source;
  os->operation_id = ss->operation_id;
  os->text = (ss->text == NULL) ? "": ss->text;
  os->count = grl_operation_options_get_count(ss->options);
  os->skip = grl_operation_options_get_skip(ss->options);
  os->callback = ss->callback;
  os->user_data = ss->user_data;
  os->error_code = GRL_CORE_ERROR_SEARCH_FAILED;
  os->magnatune_cb = NULL;

  if (GRL_MAGNATUNE_SOURCE(source)->priv->db == NULL) {
    /* Get database first, then execute the search */
    os->magnatune_cb = magnatune_execute_search;
    magnatune_get_crc_async();
    magnatune_get_db_async(os);
  } else {
    magnatune_execute_search(os);
    magnatune_check_update();
  }
}

static void
grl_magnatune_source_browse(GrlSource *source, GrlSourceBrowseSpec *bs)
{
  OperationSpec *os = NULL;

  os = g_slice_new0(OperationSpec);
  os->source = bs->source;
  os->operation_id = bs->operation_id;
  os->container = bs->container;
  os->count = grl_operation_options_get_count(bs->options);
  os->skip = grl_operation_options_get_skip(bs->options);
  os->callback = bs->callback;
  os->user_data = bs->user_data;
  os->error_code = GRL_CORE_ERROR_BROWSE_FAILED;
  os->magnatune_cb = NULL;

  if (GRL_MAGNATUNE_SOURCE(source)->priv->db == NULL) {
    /* Get database first, then execute the browse */
    os->magnatune_cb = magnatune_execute_browse;
    magnatune_get_crc_async();
    magnatune_get_db_async(os);
  } else {
    magnatune_execute_browse(os);
    magnatune_check_update();
  }
}
