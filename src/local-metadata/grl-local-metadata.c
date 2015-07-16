/*
 * Copyright (C) 2010-2011 Igalia S.L.
 *
 * Contact: Guillaume Emont <gemont@igalia.com>
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

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>

#include <grilo.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <libmediaart/mediaart.h>

#include "grl-local-metadata.h"

#define GRL_LOG_DOMAIN_DEFAULT local_metadata_log_domain
GRL_LOG_DOMAIN_STATIC(local_metadata_log_domain);

#define PLUGIN_ID   LOCALMETADATA_PLUGIN_ID

#define SOURCE_ID   "grl-local-metadata"
#define SOURCE_NAME _("Local Metadata Provider")
#define SOURCE_DESC _("A source providing locally available metadata")

/**/

#define TV_REGEX                                \
  "(?<showname>.*)\\."                          \
  "(?<season>[sS\\.]\\d{1,2}|\\d{1,2})"         \
  "\\.?(?<episode>(?:(?i)ep|[ex\\.])\\d{1,2})"  \
  "\\.?(?<name>(?:\().*|.*))?"
#define MOVIE_REGEX                             \
  "(?<name>.*)"                                 \
  "(?<year>19\\d{2}|20\\d{2})"

/**/

#define GRL_LOCAL_METADATA_SOURCE_GET_PRIVATE(object)           \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                        \
                               GRL_LOCAL_METADATA_SOURCE_TYPE,	\
                               GrlLocalMetadataSourcePriv))

enum {
  PROP_0,
  PROP_GUESS_VIDEO,
};

struct _GrlLocalMetadataSourcePriv {
  gboolean guess_video;
  GrlKeyID hash_keyid;
};

/**/

typedef enum {
  FLAG_VIDEO_TITLE         = 1,
  FLAG_VIDEO_SHOWNAME      = 1 << 1,
  FLAG_VIDEO_DATE          = 1 << 2,
  FLAG_VIDEO_SEASON        = 1 << 3,
  FLAG_VIDEO_EPISODE       = 1 << 4,
  FLAG_VIDEO_EPISODE_TITLE = 1 << 5,
  FLAG_THUMBNAIL           = 1 << 6,
  FLAG_GIBEST_HASH         = 1 << 7
} resolution_flags_t;

const gchar *video_blacklisted_prefix[] = {
  "tpz-", NULL
};

const char *video_blacklisted_words[] = {
  "720p", "1080p", "x264", "ws", "proper", "repack", "real.repack",
  "hdtv", "pdtv", "notv", "dsr", "dvdrip", "divx", "xvid",
  NULL
};

/**/

static void grl_local_metadata_source_set_property (GObject      *object,
                                                    guint         propid,
                                                    const GValue *value,
                                                    GParamSpec   *pspec);

static GrlLocalMetadataSource *grl_local_metadata_source_new (gboolean guess_video);

static void grl_local_metadata_source_resolve (GrlSource *source,
                                               GrlSourceResolveSpec *rs);

static const GList *grl_local_metadata_source_supported_keys (GrlSource *source);

static void grl_local_metadata_source_cancel (GrlSource *source,
                                              guint operation_id);

static gboolean grl_local_metadata_source_may_resolve (GrlSource *source,
                                                       GrlMedia *media,
                                                       GrlKeyID key_id,
                                                       GList **missing_keys);

gboolean grl_local_metadata_source_plugin_init (GrlRegistry *registry,
                                                GrlPlugin *plugin,
                                                GList *configs);
static resolution_flags_t get_resolution_flags (GList                      *keys,
                                                GrlLocalMetadataSourcePriv *priv);

/**/

/* =================== GrlLocalMetadata Plugin  =============== */

gboolean
grl_local_metadata_source_plugin_init (GrlRegistry *registry,
                                       GrlPlugin *plugin,
                                       GList *configs)
{
  guint config_count;
  gboolean guess_video = TRUE;
  GrlConfig *config;

  GRL_LOG_DOMAIN_INIT (local_metadata_log_domain, "local-metadata");

  GRL_DEBUG ("grl_local_metadata_source_plugin_init");

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  if (!configs) {
    GRL_INFO ("\tConfiguration not provided! Using default configuration.");
  } else {
    config_count = g_list_length (configs);
    if (config_count > 1) {
      GRL_INFO ("\tProvided %i configs, but will only use one", config_count);
    }

    config = GRL_CONFIG (configs->data);

    guess_video = grl_config_get_boolean (config, "guess-video");
  }

  GrlLocalMetadataSource *source = grl_local_metadata_source_new (guess_video);
  grl_registry_register_source (registry,
                                plugin,
                                GRL_SOURCE (source),
                                NULL);
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_local_metadata_source_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== GrlLocalMetadata GObject ================ */

static GrlLocalMetadataSource *
grl_local_metadata_source_new (gboolean guess_video)
{
  GRL_DEBUG ("grl_local_metadata_source_new");
  return g_object_new (GRL_LOCAL_METADATA_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
                       "guess-video", guess_video,
		       NULL);
}

static void
grl_local_metadata_source_class_init (GrlLocalMetadataSourceClass * klass)
{
  GObjectClass           *g_class        = G_OBJECT_CLASS (klass);
  GrlSourceClass         *source_class   = GRL_SOURCE_CLASS (klass);

  g_class->set_property = grl_local_metadata_source_set_property;

  source_class->supported_keys = grl_local_metadata_source_supported_keys;
  source_class->cancel = grl_local_metadata_source_cancel;
  source_class->may_resolve = grl_local_metadata_source_may_resolve;
  source_class->resolve = grl_local_metadata_source_resolve;

  g_object_class_install_property (g_class,
                                   PROP_GUESS_VIDEO,
                                   g_param_spec_boolean ("guess-video",
                                                         "Guess video",
                                                         "Guess video metadata "
                                                         "from filename",
                                                         TRUE,
                                                         G_PARAM_WRITABLE |
                                                         G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (klass, sizeof (GrlLocalMetadataSourcePriv));
}

static void
grl_local_metadata_source_init (GrlLocalMetadataSource *source)
{
}

G_DEFINE_TYPE (GrlLocalMetadataSource,
               grl_local_metadata_source,
               GRL_TYPE_SOURCE);

static void
grl_local_metadata_source_set_property (GObject      *object,
                                        guint         propid,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GrlLocalMetadataSourcePriv *priv =
    GRL_LOCAL_METADATA_SOURCE_GET_PRIVATE (object);

  switch (propid) {
  case PROP_GUESS_VIDEO:
    priv->guess_video = g_value_get_boolean (value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, propid, pspec);
  }
}

/* ======================= Utilities ==================== */

typedef struct {
  GrlSource *source;  /* owned */
  GrlSourceResolveSpec *rs;  /* unowned */
  guint n_pending_operations;  /* always ≥ 1 */
  gboolean has_invoked_callback;
} ResolveData;

static void
resolve_data_start_operation (ResolveData  *data,
                              const gchar  *op_name)
{
  g_assert (data->n_pending_operations >= 1);
  data->n_pending_operations++;

  GRL_DEBUG ("Starting operation %s; %u operations now pending.",
             op_name, data->n_pending_operations);
}

/* The caller is responsible for freeing @error. */
static void
resolve_data_finish_operation (ResolveData   *data,
                               const gchar   *op_name,
                               const GError  *error)
{
  g_assert (data->n_pending_operations >= 1);
  data->n_pending_operations--;

  GRL_DEBUG ("Finishing operation %s; %u operations still pending.",
             op_name, data->n_pending_operations);

  if (!data->has_invoked_callback &&
      (data->n_pending_operations == 0 || error != NULL)) {
    GrlSourceResolveSpec *rs = data->rs;

    /* All sub-operations have finished (or one has errored), so the callback
     * can be invoked. */
    data->has_invoked_callback = TRUE;
    rs->callback (data->source, rs->operation_id, rs->media,
                  rs->user_data, error);
  }

  /* All sub-operations have finished, so we can free the closure. */
  if (data->n_pending_operations == 0) {
    g_assert (data->has_invoked_callback);

    g_object_unref (data->source);
    g_slice_free (ResolveData, data);
  }
}

static gboolean
is_nonalnum (const gchar *str)
{
  gunichar uchar;

  if (str == NULL) {
    return FALSE;
  }

  uchar = g_utf8_get_char (str);

  if (g_unichar_isalnum (uchar)) {
    return FALSE;
  }

  if (uchar == '!' || uchar == '?' || uchar == '.') {
    return FALSE;
  }

  return TRUE;
}

static gchar *
video_sanitise_string (const gchar *str)
{
  int    i;
  gchar *line, *line_end;
  GRegex *regex;

  line = (gchar *) str;
  for (i = 0; video_blacklisted_prefix[i]; i++) {
    if (g_str_has_prefix (str, video_blacklisted_prefix[i])) {
      int len = strlen (video_blacklisted_prefix[i]);

      line = (gchar *) str + len;
    }
  }

  /* Get the substring limited by the first blacklisted word */
  line_end = line + strlen (line);
  for (i = 0; video_blacklisted_words[i]; i++) {
    gchar *end;

    end = strcasestr (line, video_blacklisted_words[i]);
    if (end && end < line_end) {
      line_end = end;
    }
  }

  if (*line_end != '\0') {
    /* After removing substring with blacklisted word, ignore non alpha-numeric
     * char in the end of the sanitised string */
    do {
      line_end = g_utf8_find_prev_char (line, line_end);
    } while (is_nonalnum (line_end));

    /* If everything in the string is blacklisted, just ignore
     * the blackisting logic.
     */
    if (line_end == NULL) {
      return g_strdup (str);
    }

    return g_strndup (line, line_end - line);
  }

  regex = g_regex_new ("\\.-\\.", 0, 0, NULL);
  line = g_regex_replace_literal(regex, line, -1, 0, ".", 0, NULL);
  g_regex_unref(regex);

  return line;
}

/* tidies strings before we run them through the regexes */
static gchar *
video_display_name_to_metadata (const gchar *display_name)
{
  gchar *ext, *name, *whitelisted;

  ext = strrchr (display_name, '.');
  if (ext) {
    name = g_strndup (display_name, ext - display_name);
  } else {
    name = g_strdup (display_name);
  }

  /* Replace _ <space> with . */
  g_strdelimit (name, "_ ", '.');
  whitelisted = video_sanitise_string (name);
  g_free (name);

  return whitelisted;
}

static void
video_guess_values_from_display_name (const gchar *display_name,
                                      gchar      **title,
                                      gchar      **showname,
                                      GDateTime  **date,
                                      gint        *season,
                                      gint        *episode)
{
  gchar      *metadata;
  GRegex     *regex;
  GMatchInfo *info;

  metadata = video_display_name_to_metadata (display_name);

  regex = g_regex_new (MOVIE_REGEX, 0, 0, NULL);
  g_regex_match (regex, metadata, 0, &info);

  if (g_match_info_matches (info)) {
    if (title) {
      *title = g_match_info_fetch_named (info, "name");
      /* Replace "." with <space> */
      g_strdelimit (*title, ".", ' ');
      *title = g_strstrip (*title);
    }

    if (date) {
      gchar *year = g_match_info_fetch_named (info, "year");

      *date = g_date_time_new_utc (atoi (year), 1, 1, 0, 0, 0.0);
      g_free (year);
    }

    if (showname) {
      *showname = NULL;
    }

    if (season) {
      *season = 0;
    }

    if (episode) {
      *episode = 0;
    }

    g_regex_unref (regex);
    g_match_info_free (info);
    g_free (metadata);

    return;
  }

  g_regex_unref (regex);
  g_match_info_free (info);

  regex = g_regex_new (TV_REGEX, 0, 0, NULL);
  g_regex_match (regex, metadata, 0, &info);

  if (g_match_info_matches (info)) {
    if (title) {
      *title = g_match_info_fetch_named (info, "name");
      g_strdelimit (*title, ".()", ' ');
      *title = g_strstrip (*title);
      if (*title[0] == '\0') {
        g_free (*title);
        *title = NULL;
      }
    }

    if (showname) {
      *showname = g_match_info_fetch_named (info, "showname");
      g_strdelimit (*showname, ".", ' ');
      *showname = g_strstrip (*showname);
    }

    if (season) {
      gchar *s = g_match_info_fetch_named (info, "season");
      if (s) {
        if (*s == 's' || *s == 'S') {
          *season = atoi (s + 1);
        } else {
          *season = atoi (s);
        }
      } else {
        *season = 0;
      }

      g_free (s);
    }

    if (episode) {
      gchar *e = g_match_info_fetch_named (info, "episode");
      if (e) {
        if (g_ascii_strncasecmp (e, "ep", 2) == 0) {
          *episode = atoi (e + 2);
        } else if (*e == 'e' || *e == 'E' || *e == 'x' || *e == '.') {
          *episode = atoi (e + 1);
        } else {
          *episode = atoi (e);
        }
      } else {
        *episode = 0;
      }

      g_free (e);
    }

    if (date) {
      *date = NULL;
    }

    g_regex_unref (regex);
    g_match_info_free (info);
    g_free (metadata);

    return;
  }

  g_regex_unref (regex);
  g_match_info_free (info);

  /* The filename doesn't look like a movie or a TV show, just use the
     filename without extension as the title */
  if (title) {
    *title = g_strdelimit (metadata, ".", ' ');
  }

  if (showname) {
    *showname = NULL;
  }

  if (date) {
    *date = NULL;
  }

  if (season) {
    *season = 0;
  }

  if (episode) {
    *episode = 0;
  }
}

/* Returns: (transfer none) */
static GCancellable *
resolve_data_ensure_cancellable (ResolveData *resolve_data)
{
  GCancellable *cancellable;

  cancellable = grl_operation_get_data (resolve_data->rs->operation_id);

  if (cancellable)
    return cancellable;

  cancellable = g_cancellable_new ();
  /* The operation owns the cancellable */
  grl_operation_set_data_full (resolve_data->rs->operation_id,
                               cancellable,
                               (GDestroyNotify) g_object_unref);
  return cancellable;
}

static void
extract_gibest_hash_done (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  GError *error = NULL;
  ResolveData *resolve_data = user_data;

  g_task_propagate_boolean (G_TASK (res), &error);
  resolve_data_finish_operation (resolve_data, "image", error);
  g_clear_error (&error);
}

#define CHUNK_N_BYTES (2 << 15)

static void
extract_gibest_hash (GTask        *task,
                     gpointer      source_object,
                     gpointer      task_data,
                     GCancellable *cancellable)
{
  GFile *file = source_object;
  guint64 buffer[2][CHUNK_N_BYTES/8];
  GInputStream *stream = NULL;
  gssize n_bytes, file_size;
  GError *error = NULL;
  guint64 hash = 0;
  gint i;
  char *str;
  ResolveData *resolve_data = task_data;
  GrlLocalMetadataSourcePriv *priv;

  priv = GRL_LOCAL_METADATA_SOURCE_GET_PRIVATE (resolve_data->source);

  stream = G_INPUT_STREAM (g_file_read (file, cancellable, &error));
  if (stream == NULL)
    goto fail;

  /* Extract start/end chunks of the file */
  n_bytes = g_input_stream_read (stream, buffer[0], CHUNK_N_BYTES, cancellable, &error);
  if (n_bytes == -1)
    goto fail;

  if (!g_seekable_seek (G_SEEKABLE (stream), -CHUNK_N_BYTES, G_SEEK_END, cancellable, &error))
    goto fail;

  n_bytes = g_input_stream_read (stream, buffer[1], CHUNK_N_BYTES, cancellable, &error);
  if (n_bytes == -1)
    goto fail;

  for (i = 0; i < G_N_ELEMENTS (buffer[0]); i++)
    hash += buffer[0][i] + buffer[1][i];

  file_size = g_seekable_tell (G_SEEKABLE (stream));

  if (file_size < CHUNK_N_BYTES)
    goto fail;

  /* Include file size */
  hash += file_size;
  g_object_unref (stream);

  str = g_strdup_printf ("%" G_GINT64_FORMAT, hash);
  grl_data_set_string (GRL_DATA (resolve_data->rs->media), priv->hash_keyid, str);
  g_free (str);

  g_task_return_boolean (task, TRUE);
  return;

fail:
  GRL_DEBUG ("Could not get file hash: %s\n", error ? error->message : "Unknown error");
  g_task_return_error (task, error);
  g_clear_object (&stream);
}

static void
extract_gibest_hash_async (ResolveData          *resolve_data,
                           GFile                *file,
                           GCancellable         *cancellable)
{
  GTask *task;

  task = g_task_new (G_OBJECT (file), cancellable, extract_gibest_hash_done,
                     resolve_data);
  g_task_run_in_thread (task, extract_gibest_hash);
}

static void
got_file_info (GFile *file,
               GAsyncResult *result,
               gpointer user_data)
{
  GCancellable *cancellable;
  GFileInfo *info;
  GError *error = NULL;
  const gchar *thumbnail_path;
  gboolean thumbnail_is_valid;
  GrlLocalMetadataSourcePriv *priv;
  ResolveData *resolve_data = user_data;
  GrlSourceResolveSpec *rs = resolve_data->rs;

  GRL_DEBUG ("got_file_info");

  priv = GRL_LOCAL_METADATA_SOURCE_GET_PRIVATE (resolve_data->source);

  cancellable = resolve_data_ensure_cancellable (resolve_data);

  info = g_file_query_info_finish (file, result, &error);
  if (error)
    goto error;

  thumbnail_path =
      g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
#if GLIB_CHECK_VERSION (2, 39, 0)
  thumbnail_is_valid =
      g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_THUMBNAIL_IS_VALID);
#else
  thumbnail_is_valid = TRUE;
#endif


  if (thumbnail_path && thumbnail_is_valid) {
    gchar *thumbnail_uri = g_filename_to_uri (thumbnail_path, NULL, &error);
    if (error)
      goto error;

    GRL_INFO ("Got thumbnail %s for media: %s", thumbnail_uri,
              grl_media_get_url (rs->media));
    grl_media_set_thumbnail (rs->media, thumbnail_uri);
    g_free (thumbnail_uri);
  } else if (thumbnail_path && !thumbnail_is_valid) {
    GRL_INFO ("Found outdated thumbnail %s for media: %s", thumbnail_path,
              grl_media_get_url (rs->media));
  } else {
    GRL_INFO ("Could not find thumbnail for media: %s",
              grl_media_get_url (rs->media));
  }

  if (get_resolution_flags (rs->keys, priv) & FLAG_GIBEST_HASH) {
    extract_gibest_hash_async (resolve_data, file, cancellable);
  } else {
    resolve_data_finish_operation (resolve_data, "image", NULL);
  }

  goto exit;

error:
    {
      GError *new_error = g_error_new (GRL_CORE_ERROR,
                                       GRL_CORE_ERROR_RESOLVE_FAILED,
                                       _("Failed to resolve: %s"),
                                       error->message);
      resolve_data_finish_operation (resolve_data, "image", new_error);

      g_error_free (error);
      g_error_free (new_error);
    }

exit:
    g_clear_object (&info);
}

static void
resolve_video (ResolveData         *resolve_data,
               GrlKeyID             key,
               resolution_flags_t   flags)
{
  gchar *title, *showname, *display_name;
  GDateTime *date;
  gint season, episode;
  GrlData *data = GRL_DATA (resolve_data->rs->media);
  resolution_flags_t miss_flags = 0, fill_flags;

  GRL_DEBUG ("%s",__FUNCTION__);

  resolve_data_start_operation (resolve_data, "video");

  if (!(flags & (FLAG_VIDEO_TITLE |
                 FLAG_VIDEO_SHOWNAME |
                 FLAG_VIDEO_DATE |
                 FLAG_VIDEO_SEASON |
                 FLAG_VIDEO_EPISODE |
                 FLAG_VIDEO_EPISODE_TITLE))) {
    resolve_data_finish_operation (resolve_data, "video", NULL);
    return;
  }

  if (grl_data_has_key (data, GRL_METADATA_KEY_TITLE)) {
    if (grl_data_get_boolean (data, GRL_METADATA_KEY_TITLE_FROM_FILENAME)) {
      miss_flags = FLAG_VIDEO_TITLE;
    } else
      miss_flags = 0;
  } else {
    miss_flags = FLAG_VIDEO_TITLE;
  }
  miss_flags |= grl_data_has_key (data, GRL_METADATA_KEY_SHOW) ?
    0 : FLAG_VIDEO_SHOWNAME;
  miss_flags |= grl_data_has_key (data, GRL_METADATA_KEY_PUBLICATION_DATE) ?
    0 : FLAG_VIDEO_DATE;
  miss_flags |= grl_data_has_key (data, GRL_METADATA_KEY_SEASON) ?
    0 : FLAG_VIDEO_SEASON;
  miss_flags |= grl_data_has_key (data, GRL_METADATA_KEY_EPISODE) ?
    0 : FLAG_VIDEO_EPISODE;
  miss_flags |= grl_data_has_key (data, GRL_METADATA_KEY_EPISODE_TITLE) ?
    0 : FLAG_VIDEO_EPISODE_TITLE;

  fill_flags = flags & miss_flags;

  if (!fill_flags) {
    resolve_data_finish_operation (resolve_data, "video", NULL);
    return;
  }

  if (key == GRL_METADATA_KEY_URL) {
    GFile *file;

    file = g_file_new_for_uri (grl_media_get_url (resolve_data->rs->media));
    display_name = g_file_get_basename (file);
    g_object_unref (file);
  } else {
    display_name = g_strdup (grl_media_get_title (resolve_data->rs->media));
  }

  video_guess_values_from_display_name (display_name,
                                        &title, &showname, &date,
                                        &season, &episode);

  g_free (display_name);

  GRL_DEBUG ("\tfound title=%s/showname=%s/year=%i/season=%i/episode=%i",
             title, showname,
             date != NULL ? g_date_time_get_year (date) : 0,
             season, episode);

  if (showname) {
    if (fill_flags & FLAG_VIDEO_SHOWNAME) {
      grl_data_set_string (data, GRL_METADATA_KEY_SHOW, showname);
    }
    g_free (showname);

    if (fill_flags & FLAG_VIDEO_EPISODE_TITLE &&
        title != NULL) {
      grl_data_set_string (data, GRL_METADATA_KEY_EPISODE_TITLE, title);
    }
  } else {
    /* As this is just a guess, don't erase already provided values,
     * unless GRL_METADATA_KEY_TITLE_FROM_FILENAME is set */
    if (grl_data_get_boolean (data, GRL_METADATA_KEY_TITLE_FROM_FILENAME)) {
      if (fill_flags & FLAG_VIDEO_TITLE) {
        grl_data_set_string (data, GRL_METADATA_KEY_TITLE, title);
      }
    }
  }
  g_free (title);

  if (date) {
    if (fill_flags & FLAG_VIDEO_DATE) {
      grl_data_set_boxed (data, GRL_METADATA_KEY_PUBLICATION_DATE, date);
    }
    g_date_time_unref (date);
  }

  if (season && (fill_flags & FLAG_VIDEO_SEASON)) {
    grl_data_set_int (data, GRL_METADATA_KEY_SEASON, season);
  }

  if (episode && (fill_flags & FLAG_VIDEO_EPISODE)) {
    grl_data_set_int (data, GRL_METADATA_KEY_EPISODE, episode);
  }

  resolve_data_finish_operation (resolve_data, "video", NULL);
}

static void
resolve_image (ResolveData         *resolve_data,
               resolution_flags_t   flags)
{
  GFile *file;
  GCancellable *cancellable;

  GRL_DEBUG ("resolve_image");

  resolve_data_start_operation (resolve_data, "image");

  if (flags & FLAG_THUMBNAIL) {
    const gchar *attributes;

    file = g_file_new_for_uri (grl_media_get_url (resolve_data->rs->media));

    cancellable = resolve_data_ensure_cancellable (resolve_data);

#if GLIB_CHECK_VERSION (2, 39, 0)
    attributes = G_FILE_ATTRIBUTE_THUMBNAIL_PATH "," \
                 G_FILE_ATTRIBUTE_THUMBNAIL_IS_VALID;
#else
    attributes = G_FILE_ATTRIBUTE_THUMBNAIL_PATH;
#endif

    g_file_query_info_async (file, attributes,
                             G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT, cancellable,
                             (GAsyncReadyCallback)got_file_info, resolve_data);
    g_object_unref (file);
  } else {
    resolve_data_finish_operation (resolve_data, "image", NULL);
  }
}

static void
resolve_album_art_cb (GObject       *source_object,
                      GAsyncResult  *result,
                      gpointer       user_data)
{
  GFile *cache_file;
  ResolveData *resolve_data;
  GFileInfo *info = NULL;
  GError *error = NULL;

  cache_file = G_FILE (source_object);
  resolve_data = user_data;

  info = g_file_query_info_finish (cache_file, result, &error);

  if (info != NULL) {
    /* Success, the album art exists. */
    gchar *cache_uri = g_file_get_uri (cache_file);
    grl_media_set_thumbnail (resolve_data->rs->media, cache_uri);
    g_free (cache_uri);
  } else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
    /* Ignore G_IO_ERROR_NOT_FOUND. */
    g_clear_error (&error);
  }

  resolve_data_finish_operation (resolve_data, "album-art", error);

  g_clear_object (&info);
  g_clear_error (&error);
}

static void
resolve_album_art (ResolveData         *resolve_data,
                   resolution_flags_t   flags)
{
  const gchar *artist, *album;
  GCancellable *cancellable = NULL;
  GFile *cache_file = NULL;

  resolve_data_start_operation (resolve_data, "album-art");

  artist = grl_media_audio_get_artist (GRL_MEDIA_AUDIO (resolve_data->rs->media));
  album = grl_media_audio_get_album (GRL_MEDIA_AUDIO (resolve_data->rs->media));

  if (!artist || !album)
    goto done;

  cancellable = resolve_data_ensure_cancellable (resolve_data);

  media_art_get_file (artist, album, "album", &cache_file);

  if (cache_file) {
    /* Check whether the cache file exists. */
    resolve_data_start_operation (resolve_data, "album-art");
    g_file_query_info_async (cache_file, G_FILE_ATTRIBUTE_ACCESS_CAN_READ,
                             G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT,
                             cancellable, resolve_album_art_cb, resolve_data);
  } else {
    GRL_DEBUG ("Found no thumbnail for artist %s and album %s", artist, album);
  }

done:
  resolve_data_finish_operation (resolve_data, "album-art", NULL);

  g_clear_object (&cache_file);
}

static gboolean
is_supported_scheme (const char *scheme)
{
  GVfs *vfs;
  const gchar * const * schemes;
  guint i;

  if (scheme == NULL)
    return FALSE;

  vfs = g_vfs_get_default ();
  schemes = g_vfs_get_supported_uri_schemes (vfs);

  if (!schemes) {
    return FALSE;
  }

  for (i = 0; schemes[i] != NULL; i++) {
    if (g_str_equal (schemes[i], scheme))
      return TRUE;
  }

  return FALSE;
}

static gboolean
has_compatible_media_url (GrlMedia *media)
{
  gboolean ret = FALSE;
  const gchar *url;
  gchar *scheme;

  /* HACK: Cheat slightly, we don't want to use UPnP URLs */
  if (grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_SOURCE)) {
    const char *source;

    source = grl_data_get_string (GRL_DATA (media), GRL_METADATA_KEY_SOURCE);

    if (g_str_has_prefix (source, "grl-upnp-uuid:"))
      return FALSE;
    if (g_str_has_prefix (source, "grl-dleyna-uuid:"))
      return FALSE;
  }

  url = grl_media_get_url (media);
  if (!url) {
    return FALSE;
  }
  scheme = g_uri_parse_scheme (url);

  ret = is_supported_scheme (scheme);

  g_free (scheme);

  return ret;
}

static resolution_flags_t
get_resolution_flags (GList                      *keys,
                      GrlLocalMetadataSourcePriv *priv)
{
  GList *iter = keys;
  resolution_flags_t flags = 0;

  while (iter != NULL) {
    GrlKeyID key = GRLPOINTER_TO_KEYID (iter->data);
    if (key == GRL_METADATA_KEY_TITLE)
      flags |= FLAG_VIDEO_TITLE;
    else if (key == GRL_METADATA_KEY_SHOW)
      flags |= FLAG_VIDEO_SHOWNAME;
    else if (key == GRL_METADATA_KEY_PUBLICATION_DATE)
      flags |= FLAG_VIDEO_DATE;
    else if (key == GRL_METADATA_KEY_SEASON)
      flags |= FLAG_VIDEO_SEASON;
    else if (key == GRL_METADATA_KEY_EPISODE)
      flags |= FLAG_VIDEO_EPISODE;
    else if (key == GRL_METADATA_KEY_THUMBNAIL)
      flags |= FLAG_THUMBNAIL;
    else if (key == priv->hash_keyid)
      flags |= FLAG_GIBEST_HASH;
    else if (key == GRL_METADATA_KEY_EPISODE_TITLE)
      flags |= FLAG_VIDEO_EPISODE_TITLE;

    iter = iter->next;
  }

  return flags;
}

/* ================== API Implementation ================ */

static void
ensure_hash_keyid (GrlLocalMetadataSourcePriv *priv)
{
  if (priv->hash_keyid == GRL_METADATA_KEY_INVALID) {
    GrlRegistry *registry = grl_registry_get_default ();
    priv->hash_keyid = grl_registry_lookup_metadata_key (registry, "gibest-hash");
  }
}

static const GList *
grl_local_metadata_source_supported_keys (GrlSource *source)
{
  static GList *keys = NULL;
  GrlLocalMetadataSourcePriv *priv =
          GRL_LOCAL_METADATA_SOURCE_GET_PRIVATE (source);

  ensure_hash_keyid (priv);
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_THUMBNAIL,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_SHOW,
                                      GRL_METADATA_KEY_PUBLICATION_DATE,
                                      GRL_METADATA_KEY_SEASON,
                                      GRL_METADATA_KEY_EPISODE,
                                      GRL_METADATA_KEY_EPISODE_TITLE,
                                      priv->hash_keyid,
                                      NULL);
  }
  return keys;
}

static gboolean
grl_local_metadata_source_may_resolve (GrlSource *source,
                                       GrlMedia *media,
                                       GrlKeyID key_id,
                                       GList **missing_keys)
{
  GrlLocalMetadataSourcePriv *priv =
    GRL_LOCAL_METADATA_SOURCE_GET_PRIVATE (source);

  if (!media)
    return FALSE;

  if (GRL_IS_MEDIA_AUDIO (media)) {
    gboolean have_artist = FALSE, have_album = FALSE;

    if ((have_artist = grl_data_has_key (GRL_DATA (media),
                                         GRL_METADATA_KEY_ARTIST))
        &&
        (have_album = grl_data_has_key (GRL_DATA (media),
                                        GRL_METADATA_KEY_ALBUM))
        &&
        key_id == GRL_METADATA_KEY_THUMBNAIL) {
      return TRUE;

    } else if (missing_keys) {
      GList *result = NULL;
      if (!have_artist)
        result = g_list_append (result,
                                GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ARTIST));
      if (!have_album)
        result = g_list_append (result,
                                GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ALBUM));

      if (result)
        *missing_keys = result;
    }

    return FALSE;
  }

  if (GRL_IS_MEDIA_IMAGE (media)) {
    if (key_id != GRL_METADATA_KEY_THUMBNAIL)
      return FALSE;
    if (!grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_URL))
      goto missing_url;
    if (!has_compatible_media_url (media))
      return FALSE;
    return TRUE;
  }

  if (GRL_IS_MEDIA_VIDEO (media)) {
    switch (key_id) {
    case GRL_METADATA_KEY_TITLE:
    case GRL_METADATA_KEY_SHOW:
    case GRL_METADATA_KEY_PUBLICATION_DATE:
    case GRL_METADATA_KEY_SEASON:
    case GRL_METADATA_KEY_EPISODE:
    case GRL_METADATA_KEY_EPISODE_TITLE:
      if (!priv->guess_video)
        return FALSE;
      if (grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_URL) &&
          has_compatible_media_url (media))
        return TRUE;
      if (!grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_TITLE))
        goto missing_title;
      return TRUE;
    case GRL_METADATA_KEY_THUMBNAIL:
      if (grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_URL) == FALSE)
        goto missing_url;
      return has_compatible_media_url (media);
    default:
      if (key_id == priv->hash_keyid)
        return has_compatible_media_url (media);
    }
  }

missing_title:
  if (missing_keys) {
    if (grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_URL) == FALSE)
      *missing_keys = grl_metadata_key_list_new (GRL_METADATA_KEY_TITLE, GRL_METADATA_KEY_URL, NULL);
    else
      *missing_keys = grl_metadata_key_list_new (GRL_METADATA_KEY_TITLE, NULL);
  }
  return FALSE;

missing_url:
  if (missing_keys)
    *missing_keys = grl_metadata_key_list_new (GRL_METADATA_KEY_URL, NULL);

  return FALSE;
}

static void
grl_local_metadata_source_resolve (GrlSource *source,
                                   GrlSourceResolveSpec *rs)
{
  GError *error = NULL;
  resolution_flags_t flags;
  GrlLocalMetadataSourcePriv *priv =
    GRL_LOCAL_METADATA_SOURCE_GET_PRIVATE (source);
  gboolean can_access;
  ResolveData *data = NULL;

  GRL_DEBUG (__FUNCTION__);

  /* Wrap the whole resolve operation in a GTask, as there are various async
   * components which need to run in parallel. */
  data = g_slice_new0 (ResolveData);
  data->source = g_object_ref (source);
  data->rs = rs;
  data->n_pending_operations = 1;  /* to track the initial checks */

  /* Can we access the media through gvfs? */
  can_access = has_compatible_media_url (rs->media);

  flags = get_resolution_flags (rs->keys, priv);
  if (grl_data_get_boolean (GRL_DATA (rs->media), GRL_METADATA_KEY_TITLE_FROM_FILENAME))
    flags |= FLAG_VIDEO_TITLE;

   if (!flags)
     error = g_error_new_literal (GRL_CORE_ERROR,
                                  GRL_CORE_ERROR_RESOLVE_FAILED,
                                  _("Cannot resolve any of the given keys"));
   if (GRL_IS_MEDIA_IMAGE (rs->media) && can_access == FALSE)
     error = g_error_new_literal (GRL_CORE_ERROR,
                                  GRL_CORE_ERROR_RESOLVE_FAILED,
                                  _("A GIO supported URL for images is required"));

  if (error) {
    /* No can do! */
    resolve_data_finish_operation (data, "root", error);
    g_error_free (error);
    return;
  }

  GRL_DEBUG ("\ttrying to resolve for: %s", grl_media_get_url (rs->media));

  if (GRL_IS_MEDIA_VIDEO (rs->media)) {
    if (priv->guess_video)
      resolve_video (data, can_access ? GRL_METADATA_KEY_URL : GRL_METADATA_KEY_TITLE, flags);
    if (can_access)
      resolve_image (data, flags);
  } else if (GRL_IS_MEDIA_IMAGE (rs->media)) {
    resolve_image (data, flags);
  } else if (GRL_IS_MEDIA_AUDIO (rs->media)) {
    resolve_album_art (data, flags);
  }

  /* Finish the overall operation (this might not call the callback if there
   * are still some sub-operations pending). */
  resolve_data_finish_operation (data, "root", NULL);
}

static void
grl_local_metadata_source_cancel (GrlSource *source,
                                  guint operation_id)
{
  GCancellable *cancellable =
          (GCancellable *) grl_operation_get_data (operation_id);

  if (cancellable) {
    g_cancellable_cancel (cancellable);
  }
}
