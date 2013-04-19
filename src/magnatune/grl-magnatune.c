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
#include <glib/gi18n-lib.h>
#include <grilo.h>
#include <sqlite3.h>

#include "grl-magnatune.h"

#define GRL_MAGNATUNE_GET_PRIVATE(object)                 \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                  \
                               GRL_MAGNATUNE_SOURCE_TYPE, \
                               GrlMagnatunePrivate))

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

/* --- Files --- */

#define GRL_SQL_DB      "grl-magnatune.db"

/* --- URLs --- */

#define URL_SONG_PLAY "http://he3.magnatune.com/all"

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

struct _GrlMagnatunePrivate {
  sqlite3 *db;
};

struct _OperationSpec {
  GrlSource *source;
  guint operation_id;
  const gchar *media_id;
  guint skip;
  guint count;
  const gchar *text;
  GrlSourceResultCb callback;
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
  if (source->priv->db == NULL)
    return FALSE;

  grl_registry_register_source(registry,
                               plugin,
                               GRL_SOURCE(source),
                               NULL);
  return TRUE;
}

GRL_PLUGIN_REGISTER(grl_magnatune_plugin_init, NULL, SOURCE_ID);

/* ================== Magnatune GObject ================= */

static GrlMagnatuneSource *
grl_magnatune_source_new(void)
{
  GObject *object;
  GrlMagnatuneSource *source;

  GRL_DEBUG("magnatune_source_new");

  object = g_object_new(GRL_MAGNATUNE_SOURCE_TYPE,
                        "source-id", SOURCE_ID,
                        "source-name", SOURCE_NAME,
                        "source-desc", SOURCE_DESC,
                        "supported-media", GRL_MEDIA_TYPE_AUDIO,
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

  g_type_class_add_private(klass, sizeof(GrlMagnatunePrivate));
}

static void
grl_magnatune_source_init(GrlMagnatuneSource *source)
{
  gint ret;
  gchar *path;
  gchar *db_path;

  GRL_DEBUG("magnatune_source_init");

  source->priv = GRL_MAGNATUNE_GET_PRIVATE(source);
  source->priv->db = NULL;

  path = g_build_filename(g_get_user_data_dir(), "grilo-plugins", NULL);
  db_path = g_build_filename(path, GRL_SQL_DB, NULL);

  if(!g_file_test(path, G_FILE_TEST_IS_DIR)) {
    g_mkdir_with_parents(path, 0775);
  }

  if (g_file_test(db_path, G_FILE_TEST_EXISTS) == TRUE) {
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
    GRL_DEBUG("No database was found.");
  }

  g_free(db_path);
  g_free(path);
}

G_DEFINE_TYPE(GrlMagnatuneSource, grl_magnatune_source, GRL_TYPE_SOURCE);

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

static GrlMedia *
build_media(gint track_id,
            const gchar *artist_name,
            const gchar *album_name,
            const gchar *track_name,
            gint track_number,
            gint duration,
            const gchar *url_to_mp3)
{
  GrlMedia *media = NULL;
  GrlMediaAudio *audio = NULL;
  gchar *str_track_id = NULL;

  media = grl_media_audio_new();
  audio = GRL_MEDIA_AUDIO(media);
  grl_media_audio_set_track_number(audio, track_number);
  grl_media_audio_set_artist(audio, artist_name);
  grl_media_audio_set_album(audio, album_name);

  grl_media_set_url(media, url_to_mp3);
  grl_media_set_duration(media, duration);
  grl_media_set_title(media, track_name);

  str_track_id = g_strdup_printf("%d", track_id);
  grl_media_set_id(media, str_track_id);
  g_free(str_track_id);

  return media;
}

static GrlMedia *
build_media_track_from_stmt(sqlite3_stmt *sql_stmt)
{
  GrlMedia *media = NULL;

  gint track_id;
  gint duration;
  gint track_number;
  const gchar *artist_name;
  const gchar *album_name;
  const gchar *track_name;
  const gchar *raw_url;
  gchar *encoded_url;
  gchar *url_to_mp3;

  track_id = (guint) sqlite3_column_int(sql_stmt, MAGNATUNE_TRACK_ID);
  artist_name = (gchar *) sqlite3_column_text(sql_stmt, MAGNATUNE_ARTIST_NAME);
  album_name = (gchar *) sqlite3_column_text(sql_stmt, MAGNATUNE_ALBUM_NAME);
  duration = (guint) sqlite3_column_int(sql_stmt, MAGNATUNE_TRACK_DURATION);
  track_number = (guint) sqlite3_column_int(sql_stmt, MAGNATUNE_TRACK_NUMBER);
  track_name = (gchar *) sqlite3_column_text(sql_stmt, MAGNATUNE_TRACK_NAME);
  raw_url = (gchar *) sqlite3_column_text(sql_stmt, MAGNATUNE_TRACK_URL_TO_MP3);

  encoded_url = g_uri_escape_string(raw_url, "", FALSE);
  url_to_mp3 = g_strdup_printf("%s/%s", URL_SONG_PLAY, encoded_url);
  media = build_media(track_id, artist_name, album_name, track_name,
                      track_number, duration, url_to_mp3);

  g_free(encoded_url);
  g_free(url_to_mp3);

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

end_search:
  if (err != NULL) {
    os->callback(os->source, os->operation_id, NULL, 0, os->user_data, err);
    g_error_free(err);
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
  magnatune_execute_search(os);
}
