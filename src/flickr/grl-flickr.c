/*
 * Copyright (C) 2010 Igalia S.L.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Juan A. Suarez Romero <jasuarez@igalia.com>
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

#include "grl-flickr.h"
#include "gflickr.h"

#define GRL_FLICKR_SOURCE_GET_PRIVATE(object)                           \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                                \
                               GRL_FLICKR_SOURCE_TYPE,                  \
                               GrlFlickrSourcePrivate))

/* --------- Logging  -------- */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "grl-flickr"

/* --- Plugin information --- */

#define PLUGIN_ID   "grl-flickr"
#define PLUGIN_NAME "Flickr"
#define PLUGIN_DESC "A plugin for browsing and searching Flickr photos"

#define SOURCE_ID   "grl-flickr"
#define SOURCE_NAME "Flickr"
#define SOURCE_DESC "A source for browsing and searching Flickr photos"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

typedef struct {
  GrlMediaSourceSearchSpec *ss;
  gint offset;
  gint page;
} SearchData;

struct _GrlFlickrSourcePrivate {
  GFlickr *flickr;
};

static GrlFlickrSource *grl_flickr_source_new (void);

gboolean grl_flickr_plugin_init (GrlPluginRegistry *registry,
				 const GrlPluginInfo *plugin,
                                 const GrlDataConfig *config);

static const GList *grl_flickr_source_supported_keys (GrlMetadataSource *source);

static void grl_flickr_source_metadata (GrlMediaSource *source,
                                        GrlMediaSourceMetadataSpec *ss);

static void grl_flickr_source_search (GrlMediaSource *source,
                                      GrlMediaSourceSearchSpec *ss);

/* =================== Flickr Plugin  =============== */

gboolean
grl_flickr_plugin_init (GrlPluginRegistry *registry,
                        const GrlPluginInfo *plugin,
                        const GrlDataConfig *config)
{
  const gchar *flickr_key;
  const gchar *flickr_secret;
  const gchar *flickr_token;

  g_debug ("flickr_plugin_init\n");

  if (!config) {
    g_warning ("Missing configuration");
    return FALSE;
  }

  flickr_key = grl_data_config_get_api_key (config);
  flickr_token = grl_data_config_get_api_token (config);
  flickr_secret = grl_data_config_get_api_secret (config);

  if (!flickr_key || ! flickr_token || !flickr_secret) {
    g_warning ("Required configuration keys not set up");
    return FALSE;
  }

  GrlFlickrSource *source = grl_flickr_source_new ();
  source->priv->flickr = g_flickr_new (flickr_key, flickr_token, flickr_secret);

  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source));
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_flickr_plugin_init,
                     NULL,
                     PLUGIN_ID,
                     PLUGIN_NAME,
                     PLUGIN_DESC,
                     PACKAGE_VERSION,
                     AUTHOR,
                     LICENSE,
                     SITE);

/* ================== Flickr GObject ================ */

static GrlFlickrSource *
grl_flickr_source_new (void)
{
  g_debug ("grl_flickr_source_new");

  return g_object_new (GRL_FLICKR_SOURCE_TYPE,
                       "source-id", SOURCE_ID,
                       "source-name", SOURCE_NAME,
                       "source-desc", SOURCE_DESC,
                       NULL);
}

static void
grl_flickr_source_class_init (GrlFlickrSourceClass * klass)
{
  GrlMediaSourceClass *source_class = GRL_MEDIA_SOURCE_CLASS (klass);
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);

  source_class->metadata = grl_flickr_source_metadata;
  source_class->search = grl_flickr_source_search;
  metadata_class->supported_keys = grl_flickr_source_supported_keys;

  g_type_class_add_private (klass, sizeof (GrlFlickrSourcePrivate));
}

static void
grl_flickr_source_init (GrlFlickrSource *source)
{
  source->priv = GRL_FLICKR_SOURCE_GET_PRIVATE (source);
}

G_DEFINE_TYPE (GrlFlickrSource, grl_flickr_source, GRL_TYPE_MEDIA_SOURCE);

/* ======================= Utilities ==================== */

static void
update_media (GrlDataMedia *media, GHashTable *photo)
{
  gchar *author;
  gchar *date;
  gchar *description;
  gchar *id;
  gchar *thumbnail;
  gchar *title;
  gchar *url;

  author = g_hash_table_lookup (photo, "owner_realname");
  if (!author) {
    author = g_hash_table_lookup (photo, "photo_ownername");
  }
  date = g_hash_table_lookup (photo, "dates_taken");
  if (!date) {
    date = g_hash_table_lookup (photo, "photo_datetaken");
  }
  description = g_hash_table_lookup (photo, "description");
  id = g_hash_table_lookup (photo, "photo_id");
  thumbnail = g_strdup (g_hash_table_lookup (photo, "photo_url_t"));
  if (!thumbnail) {
    thumbnail = g_flickr_photo_url_thumbnail (NULL, photo);
  }
  title = g_hash_table_lookup (photo, "title");
  if (!title) {
    title = g_hash_table_lookup (photo, "photo_title");
  }
  url = g_strdup (g_hash_table_lookup (photo, "photo_url_o"));
  if (!url) {
    url = g_flickr_photo_url_original (NULL, photo);
  }

  if (author) {
    grl_data_media_set_author (media, author);
  }

  if (date) {
    grl_data_media_set_date (media, date);
  }

  if (description) {
    grl_data_media_set_description (media, description);
  }

  if (id) {
    grl_data_media_set_id (media, id);
  }

  if (thumbnail) {
    grl_data_media_set_thumbnail (media, thumbnail);
    g_free (thumbnail);
  }

  if (title) {
    grl_data_media_set_title (media, title);
  }

  if (url) {
    grl_data_media_set_url (media, url);
    g_free (url);
  }
}

static void
getInfo_cb (GFlickr *f, GHashTable *photo, gpointer user_data)
{
  GrlMediaSourceMetadataSpec *ms = (GrlMediaSourceMetadataSpec *) user_data;

  if (photo) {
    update_media (ms->media, photo);
  }

  ms->callback (ms->source, ms->media, ms->user_data, NULL);
}

static void
search_cb (GFlickr *f, GList *photolist, gpointer user_data)
{
  GrlDataMedia *media;
  SearchData *sd = (SearchData *) user_data;
  gchar *media_type;

  /* Go to offset element */
  photolist = g_list_nth (photolist, sd->offset);

  /* No more elements can be sent */
  if (!photolist) {
    sd->ss->callback (sd->ss->source,
                      sd->ss->search_id,
                      NULL,
                      0,
                      sd->ss->user_data,
                      NULL);
    g_free (sd);
    return;
  }

  while (photolist && sd->ss->count) {
    media_type = g_hash_table_lookup (photolist->data, "photo_media");
    if (strcmp (media_type, "photo") == 0) {
      media = grl_data_image_new ();
    } else {
      media = grl_data_video_new ();
    }
    update_media (media, photolist->data);
    sd->ss->callback (sd->ss->source,
                      sd->ss->search_id,
                      media,
                      sd->ss->count == 1? 0: -1,
                      sd->ss->user_data,
                      NULL);
    photolist = g_list_next (photolist);
    sd->ss->count--;
  }

  /* Get more elements */
  if (sd->ss->count) {
    sd->offset = 0;
    sd->page++;
    g_flickr_photos_search (f, sd->ss->text, sd->page, search_cb, sd);
  } else {
    g_free (sd);
  }
}

/* ================== API Implementation ================ */

static const GList *
grl_flickr_source_supported_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_AUTHOR,
                                      GRL_METADATA_KEY_DATE,
                                      GRL_METADATA_KEY_DESCRIPTION,
                                      GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_THUMBNAIL,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_URL,
                                      NULL);
  }
  return keys;
}

static void
grl_flickr_source_metadata (GrlMediaSource *source,
                            GrlMediaSourceMetadataSpec *ms)
{
  const gchar *id;

  if (!ms->media || (id = grl_data_media_get_id (ms->media)) == NULL) {
    ms->callback (ms->source, ms->media, ms->user_data, NULL);
    return;
  }

  g_flickr_photos_getInfo (GRL_FLICKR_SOURCE (source)->priv->flickr,
                           atol (id),
                           getInfo_cb,
                           ms);
}

static void
grl_flickr_source_search (GrlMediaSource *source,
                          GrlMediaSourceSearchSpec *ss)
{
  GFlickr *f = GRL_FLICKR_SOURCE (source)->priv->flickr;
  gint per_page;

  /* Compute items per page and page offset */
  per_page = CLAMP (1 + ss->skip + ss->count, 0, 100);
  g_flickr_set_per_page (f, per_page);

  SearchData *sd = g_new (SearchData, 1);
  sd->page = 1 + (ss->skip / per_page);
  sd->offset = ss->skip % per_page;
  sd->ss = ss;

  g_flickr_photos_search (f, ss->text, sd->page, search_cb, sd);
}
