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

#define SOURCE_ID   "grl-flickr"
#define SOURCE_NAME "Flickr"
#define SOURCE_DESC "A source for browsing and searching Flickr photos"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

typedef struct {
  GrlMediaSource *source;
  GrlMediaSourceResultCb callback;
  gchar *tags;
  gchar *text;
  gint offset;
  gint page;
  gpointer user_data;
  guint count;
  guint search_id;
} SearchData;

struct _GrlFlickrSourcePrivate {
  GFlickr *flickr;
};

static GrlFlickrSource *grl_flickr_source_new (void);

gboolean grl_flickr_plugin_init (GrlPluginRegistry *registry,
				 const GrlPluginInfo *plugin,
                                 GList *configs);

static const GList *grl_flickr_source_supported_keys (GrlMetadataSource *source);

static void grl_flickr_source_browse (GrlMediaSource *source,
                                      GrlMediaSourceBrowseSpec *bs);

static void grl_flickr_source_metadata (GrlMediaSource *source,
                                        GrlMediaSourceMetadataSpec *ss);

static void grl_flickr_source_search (GrlMediaSource *source,
                                      GrlMediaSourceSearchSpec *ss);

/* =================== Flickr Plugin  =============== */

gboolean
grl_flickr_plugin_init (GrlPluginRegistry *registry,
                        const GrlPluginInfo *plugin,
                        GList *configs)
{
  const gchar *flickr_key;
  const gchar *flickr_secret;
  const gchar *flickr_token;
  const GrlConfig *config;
  gint config_count;

  g_debug ("flickr_plugin_init\n");

  if (!configs) {
    g_warning ("Missing configuration");
    return FALSE;
  }

  config_count = g_list_length (configs);
  if (config_count > 1) {
    g_warning ("Provided %d configs, but will only use one", config_count);
  }

  config = GRL_CONFIG (configs->data);

  flickr_key = grl_config_get_api_key (config);
  flickr_token = grl_config_get_api_token (config);
  flickr_secret = grl_config_get_api_secret (config);

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
                     PLUGIN_ID);

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

  source_class->browse = grl_flickr_source_browse;
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
update_media (GrlMedia *media, GHashTable *photo)
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
    grl_media_set_author (media, author);
  }

  if (date) {
    grl_media_set_date (media, date);
  }

  if (description) {
    grl_media_set_description (media, description);
  }

  if (id) {
    grl_media_set_id (media, id);
  }

  if (thumbnail) {
    grl_media_set_thumbnail (media, thumbnail);
    g_free (thumbnail);
  }

  if (title) {
    grl_media_set_title (media, title);
  }

  if (url) {
    grl_media_set_url (media, url);
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
  GrlMedia *media;
  SearchData *sd = (SearchData *) user_data;
  gchar *media_type;

  /* Go to offset element */
  photolist = g_list_nth (photolist, sd->offset);

  /* No more elements can be sent */
  if (!photolist) {
    sd->callback (sd->source,
                  sd->search_id,
                  NULL,
                  0,
                  sd->user_data,
                  NULL);
    g_slice_free (SearchData, sd);
    return;
  }

  while (photolist && sd->count) {
    media_type = g_hash_table_lookup (photolist->data, "photo_media");
    if (strcmp (media_type, "photo") == 0) {
      media = grl_media_image_new ();
    } else {
      media = grl_media_video_new ();
    }
    update_media (media, photolist->data);
    sd->callback (sd->source,
                  sd->search_id,
                  media,
                  sd->count == 1? 0: -1,
                  sd->user_data,
                  NULL);
    photolist = g_list_next (photolist);
    sd->count--;
  }

  /* Get more elements */
  if (sd->count) {
    sd->offset = 0;
    sd->page++;
    g_flickr_photos_search (f, sd->text, sd->tags, sd->page, search_cb, sd);
  } else {
    g_slice_free (SearchData, sd);
  }
}

static void
gettags_cb (GFlickr *f, GList *taglist, gpointer user_data)
{
  GrlMedia *media;
  GrlMediaSourceBrowseSpec *bs = (GrlMediaSourceBrowseSpec *) user_data;
  gint count;

  /* Go to offset element */
  taglist = g_list_nth (taglist, bs->skip);

  /* No more elements can be sent */
  if (!taglist) {
    bs->callback (bs->source,
                  bs->browse_id,
                  NULL,
                  0,
                  bs->user_data,
                  NULL);
    return;
  }

  /* Send data */
  count = g_list_length (taglist);
  while (taglist) {
    count--;
    media = grl_media_box_new ();
    grl_media_set_id (media, taglist->data);
    grl_media_set_title (media, taglist->data);
    bs->callback (bs->source,
                  bs->browse_id,
                  media,
                  count,
                  bs->user_data,
                  NULL);
    taglist = g_list_next (taglist);
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
grl_flickr_source_browse (GrlMediaSource *source,
                          GrlMediaSourceBrowseSpec *bs)
{
  GFlickr *f = GRL_FLICKR_SOURCE (source)->priv->flickr;
  const gchar *container_id;
  gint per_page;
  gint request_size;

  container_id = grl_media_get_id (bs->container);

  if (!container_id) {
    /* Get hot tags list. List is limited up to 200 tags */
    if (bs->skip >= 200) {
      bs->callback (bs->source, bs->browse_id, NULL, 0, bs->user_data, NULL);
    } else {
      request_size = bs->skip + CLAMP (bs->count, 0, 200);
      g_flickr_tags_getHotList (f, request_size, gettags_cb, bs);
    }
  } else {
    per_page = CLAMP (1 + bs->skip + bs->count, 0, 100);
    g_flickr_set_per_page (f, per_page);

    SearchData *sd = g_slice_new (SearchData);
    sd->source = bs->source;
    sd->callback = bs->callback;
    sd->tags = (gchar *) container_id;
    sd->text = NULL;
    sd->page = 1 + (bs->skip / per_page);
    sd->offset = bs->skip % per_page;
    sd->user_data = bs->user_data;
    sd->count = bs->count;
    sd->search_id = bs->browse_id;
    g_flickr_photos_search (f, NULL, sd->tags, sd->page, search_cb, sd);
  }
}

static void
grl_flickr_source_metadata (GrlMediaSource *source,
                            GrlMediaSourceMetadataSpec *ms)
{
  const gchar *id;

  if (!ms->media || (id = grl_media_get_id (ms->media)) == NULL) {
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

  SearchData *sd = g_slice_new (SearchData);
  sd->source = ss->source;
  sd->callback = ss->callback;
  sd->tags = NULL;
  sd->text = ss->text;
  sd->page = 1 + (ss->skip / per_page);
  sd->offset = ss->skip % per_page;
  sd->user_data = ss->user_data;
  sd->count = ss->count;
  sd->search_id = ss->search_id;
  g_flickr_photos_search (f, ss->text, NULL, sd->page, search_cb, sd);
}
