/*
 * Copyright (C) 2010, 2011 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Joaquim Rocha <jrocha@igalia.com>
            Juan A. Suarez Romero <jasuarez@igalia.com>
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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>

#include "grl-vimeo.h"
#include "gvimeo.h"

#define GRL_VIMEO_SOURCE_GET_PRIVATE(object)                           \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                                \
                               GRL_VIMEO_SOURCE_TYPE,                  \
                               GrlVimeoSourcePrivate))

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT vimeo_log_domain
GRL_LOG_DOMAIN_STATIC(vimeo_log_domain);

/* --- Plugin information --- */

#define PLUGIN_ID   VIMEO_PLUGIN_ID

#define SOURCE_ID   "grl-vimeo"
#define SOURCE_NAME "Vimeo"
#define SOURCE_DESC _("A source for browsing and searching Vimeo videos")

#define MAX_ELEMENTS 50

typedef struct {
  GrlSourceSearchSpec *ss;
  GVimeo *vimeo;
  GQueue *queue;
  gint offset;
  gint page;
  gboolean get_url;
} SearchData;

typedef struct {
  GrlMedia *media;
  SearchData *sd;
  gint index;
  gboolean computed;
} AddMediaUrlData;

struct _GrlVimeoSourcePrivate {
  GVimeo *vimeo;
};

static GrlVimeoSource *grl_vimeo_source_new (void);
static void grl_vimeo_source_finalize (GObject *object);

gboolean grl_vimeo_plugin_init (GrlRegistry *registry,
                                GrlPlugin *plugin,
                                GList *configs);

static const GList *grl_vimeo_source_supported_keys (GrlSource *source);

static const GList *grl_vimeo_source_slow_keys (GrlSource *source);

static void grl_vimeo_source_resolve (GrlSource *source,
                                      GrlSourceResolveSpec *rs);

static void grl_vimeo_source_search (GrlSource *source,
                                     GrlSourceSearchSpec *ss);

/* =================== Vimeo Plugin  =============== */

gboolean
grl_vimeo_plugin_init (GrlRegistry *registry,
                       GrlPlugin *plugin,
                       GList *configs)
{
  gchar *vimeo_key;
  gchar *vimeo_secret;
  gchar *format;
  GrlConfig *config;
  gint config_count;
  gboolean init_result = FALSE;
  GrlVimeoSource *source;

  GRL_LOG_DOMAIN_INIT (vimeo_log_domain, "vimeo");

  GRL_DEBUG ("vimeo_plugin_init");

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

#if !GLIB_CHECK_VERSION(2,32,0)
  if (!g_thread_supported ()) {
    g_thread_init (NULL);
  }
#endif

  if (!configs) {
    GRL_INFO ("Configuration not provided! Plugin not loaded");
    return FALSE;
  }

  config_count = g_list_length (configs);
  if (config_count > 1) {
    GRL_INFO ("Provided %d configs, but will only use one", config_count);
  }

  config = GRL_CONFIG (configs->data);

  vimeo_key = grl_config_get_api_key (config);
  vimeo_secret = grl_config_get_api_secret (config);

  if (!vimeo_key || !vimeo_secret) {
    GRL_INFO ("Required API key or secret configuration not provided."
              " Plugin not loaded");
    goto go_out;
  }

  source = grl_vimeo_source_new ();
  source->priv->vimeo = g_vimeo_new (vimeo_key, vimeo_secret);

  format = grl_config_get_string (config, "format");
  if (format) {
    g_object_set (source->priv->vimeo, "quvi-format", format, NULL);
    g_free (format);
  }

  grl_registry_register_source (registry,
                                plugin,
                                GRL_SOURCE (source),
                                NULL);
  init_result = TRUE;

 go_out:
  g_clear_pointer (&vimeo_key, g_free);
  g_clear_pointer (&vimeo_secret, g_free);

  return init_result;
}

GRL_PLUGIN_REGISTER (grl_vimeo_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== Vimeo GObject ================ */

static GrlVimeoSource *
grl_vimeo_source_new (void)
{
  GIcon *icon;
  GFile *file;
  GrlVimeoSource *source;

  GRL_DEBUG ("grl_vimeo_source_new");

  file = g_file_new_for_uri ("resource:///org/gnome/grilo/plugins/vimeo/channel-vimeo.svg");
  icon = g_file_icon_new (file);
  g_object_unref (file);

  source = g_object_new (GRL_VIMEO_SOURCE_TYPE,
                         "source-id", SOURCE_ID,
                         "source-name", SOURCE_NAME,
                         "source-desc", SOURCE_DESC,
                         "supported-media", GRL_MEDIA_TYPE_VIDEO,
                         "source-icon", icon,
                         NULL);
  g_object_unref (icon);

  return source;
}

static void
grl_vimeo_source_class_init (GrlVimeoSourceClass * klass)
{
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  source_class->supported_keys = grl_vimeo_source_supported_keys;
  source_class->slow_keys = grl_vimeo_source_slow_keys;
  source_class->resolve = grl_vimeo_source_resolve;
  source_class->search = grl_vimeo_source_search;

  object_class->finalize = grl_vimeo_source_finalize;

  g_type_class_add_private (klass, sizeof (GrlVimeoSourcePrivate));
}

static void
grl_vimeo_source_init (GrlVimeoSource *source)
{
  source->priv = GRL_VIMEO_SOURCE_GET_PRIVATE (source);

  grl_source_set_auto_split_threshold (GRL_SOURCE (source), MAX_ELEMENTS);
}

G_DEFINE_TYPE (GrlVimeoSource, grl_vimeo_source, GRL_TYPE_SOURCE);

static void
grl_vimeo_source_finalize (GObject *object)
{
  GrlVimeoSource *source = GRL_VIMEO_SOURCE (object);

  g_clear_object (&source->priv->vimeo);

  G_OBJECT_CLASS (grl_vimeo_source_parent_class)->finalize (object);
}

/* ======================= Utilities ==================== */

static gint
str_to_gint (gchar *str)
{
  gint number;

  errno = 0;
  number = (gint) g_ascii_strtod (str, NULL);
  if (errno == 0)
  {
    return number;
  }
  return 0;
}

static GDateTime *
parse_date (const gchar *date)
{
  /* code duplicated from the flickr plugin until we find a better place for
   * it.*/
  guint year, month, day, hours, minutes;
  gdouble seconds;

  sscanf (date, "%u-%u-%u %u:%u:%lf",
          &year, &month, &day, &hours, &minutes, &seconds);

  return g_date_time_new_utc (year, month, day, hours, minutes, seconds);
}

static void
update_media (GrlMedia *media, GHashTable *video)
{
  gchar *str;

  str = g_hash_table_lookup (video, VIMEO_VIDEO_ID);
  if (str)
  {
    char *external_url;

    grl_media_set_id (media, str);
    external_url = g_strdup_printf ("http://vimeo.com/%s", str);
    grl_media_set_external_url (media, external_url);
    g_free (external_url);
  }

  str = g_hash_table_lookup (video, VIMEO_VIDEO_TITLE);
  if (str)
  {
    grl_media_set_title (media, str);
  }

  str = g_hash_table_lookup (video, VIMEO_VIDEO_DESCRIPTION);
  if (str)
  {
    grl_media_set_description (media, str);
  }

  str = g_hash_table_lookup (video, VIMEO_VIDEO_DURATION);
  if (str)
  {
    grl_media_set_duration (media, str_to_gint (str));
  }

  str = g_hash_table_lookup (video, VIMEO_VIDEO_OWNER_NAME);
  if (str)
  {
    grl_media_set_author (media, str);
  }

  str = g_hash_table_lookup (video, VIMEO_VIDEO_UPLOAD_DATE);
  if (str)
  {
    GDateTime *date = parse_date (str);
    if (date) {
      grl_media_set_publication_date (media, date);
      g_date_time_unref (date);
    }
  }

  str = g_hash_table_lookup (video, VIMEO_VIDEO_THUMBNAIL);
  if (str)
  {
    grl_media_set_thumbnail (media, str);
  }

  str = g_hash_table_lookup (video, VIMEO_VIDEO_WIDTH);
  if (str)
  {
    grl_media_video_set_width (GRL_MEDIA_VIDEO (media), str_to_gint (str));
  }

  str = g_hash_table_lookup (video, VIMEO_VIDEO_HEIGHT);
  if (str)
  {
    grl_media_video_set_height (GRL_MEDIA_VIDEO (media), str_to_gint (str));
  }
}

static void
add_url_media_cb (const gchar *url, gpointer user_data)
{
  AddMediaUrlData *amud = (AddMediaUrlData *) user_data;
  SearchData *sd = amud->sd;

  if (url) {
    grl_media_set_url (amud->media, url);
  }

  amud->computed = TRUE;

  /* Try to send in order all the processed elements */
  while ((amud = g_queue_peek_tail (sd->queue)) &&
         amud != NULL &&
         amud->computed) {
    sd->ss->callback (sd->ss->source,
                      sd->ss->operation_id,
                      amud->media,
                      amud->index,
                      sd->ss->user_data,
                      NULL);
    g_queue_pop_tail (sd->queue);
    g_slice_free (AddMediaUrlData, amud);
  }

  /* If there are still elements not processed let's wait for them */
  if (amud == NULL) {
    g_queue_free (sd->queue);
    g_slice_free (SearchData, sd);
  }
}

static void
search_cb (GVimeo *vimeo, GList *video_list, gpointer user_data)
{
  GrlMedia *media = NULL;
  AddMediaUrlData *amud;
  SearchData *sd = (SearchData *) user_data;
  gint count = grl_operation_options_get_count (sd->ss->options);
  gint id;
  gchar *media_type;
  gint video_list_size;

  /* Go to offset element */
  video_list = g_list_nth (video_list, sd->offset);

  /* No more elements can be sent */
  if (!video_list) {
    sd->ss->callback (sd->ss->source,
                      sd->ss->operation_id,
                      NULL,
                      0,
                      sd->ss->user_data,
                      NULL);
    g_slice_free (SearchData, sd);
    return;
  }

  video_list_size = g_list_length (video_list);
  if (count > video_list_size) {
    count = video_list_size;
  }

  if (sd->get_url) {
    sd->queue = g_queue_new ();
  }

  while (video_list && count)
  {
    media_type = g_hash_table_lookup (video_list->data, "title");
    if (media_type) {
      media = grl_media_video_new ();
    } else {
      media = NULL;
    }

    if (media)
    {
      update_media (media, video_list->data);
      if (sd->get_url) {
        amud = g_slice_new (AddMediaUrlData);
        amud->computed = FALSE;
        amud->media = media;
        amud->index = --count;
        amud->sd = sd;
        g_queue_push_head (sd->queue, amud);
        id = (gint) g_ascii_strtod (grl_media_get_id (media), NULL);
        g_vimeo_video_get_play_url (sd->vimeo,
                                    id,
                                    add_url_media_cb,
                                    amud);
      } else {
        sd->ss->callback (sd->ss->source,
                          sd->ss->operation_id,
                          media,
                          --count,
                          sd->ss->user_data,
                          NULL);
      }
    }
    video_list = g_list_next (video_list);
  }

  if (!sd->get_url) {
    g_slice_free (SearchData, sd);
  }
}

static void
video_get_play_url_cb (const gchar *url, gpointer user_data)
{
  GrlSourceResolveSpec *rs = (GrlSourceResolveSpec *) user_data;

  if (url) {
    grl_media_set_url (rs->media, url);
  }

  rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
}

/* ================== API Implementation ================ */

static const GList *
grl_vimeo_source_supported_keys (GrlSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
				      GRL_METADATA_KEY_TITLE,
				      GRL_METADATA_KEY_DESCRIPTION,
				      GRL_METADATA_KEY_URL,
				      GRL_METADATA_KEY_AUTHOR,
				      GRL_METADATA_KEY_PUBLICATION_DATE,
				      GRL_METADATA_KEY_THUMBNAIL,
				      GRL_METADATA_KEY_DURATION,
				      GRL_METADATA_KEY_WIDTH,
				      GRL_METADATA_KEY_HEIGHT,
				      GRL_METADATA_KEY_EXTERNAL_URL,
				      GRL_METADATA_KEY_INVALID);
  }
  return keys;
}

static const GList *
grl_vimeo_source_slow_keys (GrlSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_URL,
                                      GRL_METADATA_KEY_INVALID);
  }
  return keys;
}

static void
grl_vimeo_source_resolve (GrlSource *source,
                          GrlSourceResolveSpec *rs)
{
  gint id;
  const gchar *id_str;

  if (!rs->media || (id_str = grl_media_get_id (rs->media)) == NULL) {
    goto send_unchanged;
  }

  /* As all the keys are added always, except URL, the only case is missing URL */
  if (g_list_find (rs->keys, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_URL)) != NULL &&
      grl_media_get_url (rs->media) == NULL) {
    errno = 0;
    id = (gint) g_ascii_strtod (id_str, NULL);
    if (errno != 0) {
      goto send_unchanged;
    }

    g_vimeo_video_get_play_url (GRL_VIMEO_SOURCE (source)->priv->vimeo,
                                id,
                                video_get_play_url_cb,
                                rs);
  } else {
    goto send_unchanged;
  }

  return;

 send_unchanged:
  rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
}

static void
grl_vimeo_source_search (GrlSource *source,
                         GrlSourceSearchSpec *ss)
{
  SearchData *sd;
  GError *error;
  gint per_page;
  GVimeo *vimeo = GRL_VIMEO_SOURCE (source)->priv->vimeo;
  guint skip = grl_operation_options_get_skip (ss->options);
  gint count = grl_operation_options_get_count (ss->options);

  if (!ss->text) {
    /* Vimeo does not support searching all */
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_SEARCH_NULL_UNSUPPORTED,
                         _("Failed to search: %s"),
                         _("non-NULL search text is required"));
    ss->callback (ss->source, ss->operation_id, NULL, 0, ss->user_data, error);
    g_error_free (error);
    return;
  }

  sd = g_slice_new0 (SearchData);
  sd->vimeo = vimeo;
  sd->get_url = (g_list_find (ss->keys,
                              GRLKEYID_TO_POINTER (GRL_METADATA_KEY_URL)) != NULL);

  /* Compute items per page and page offset */
  grl_paging_translate (skip,
                        count,
                        MAX_ELEMENTS,
                        (guint *) &per_page,
                        (guint *) &(sd->page),
                        (guint *) &(sd->offset));

  g_vimeo_set_per_page (vimeo, per_page);
  sd->ss = ss;

  g_vimeo_videos_search (vimeo, ss->text, sd->page, search_cb, sd);
}
