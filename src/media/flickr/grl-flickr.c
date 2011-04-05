/*
 * Copyright (C) 2010 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
 *
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

#define GRL_LOG_DOMAIN_DEFAULT flickr_log_domain
GRL_LOG_DOMAIN(flickr_log_domain);

#define SEARCH_MAX  500
#define HOTLIST_MAX 200

/* --- Plugin information --- */

#define PLUGIN_ID   FLICKR_PLUGIN_ID

#define PUBLIC_SOURCE_ID   "grl-flickr"
#define PUBLIC_SOURCE_NAME "Flickr"
#define PUBLIC_SOURCE_DESC "A source for browsing and searching Flickr photos"

#define PERSONAL_SOURCE_ID "grl-flickr-%s"
#define PERSONAL_SOURCE_NAME "%s's Flickr"
#define PERSONAL_SOURCE_DESC "A source for browsing and searching %s' flickr photos"

typedef struct {
  GrlMediaSource *source;
  GrlMediaSourceResultCb callback;
  gchar *user_id;
  gchar *tags;
  gchar *text;
  guint offset;
  guint page;
  gpointer user_data;
  guint count;
  guint operation_id;
} OperationData;

struct _GrlFlickrSourcePrivate {
  GFlickr *flickr;
  gchar *user_id;
};

static void token_info_cb (GFlickr *f,
                           GHashTable *info,
                           gpointer user_data);

static GrlFlickrSource *grl_flickr_source_public_new (const gchar *flickr_api_key,
                                                      const gchar *flickr_secret);

static void grl_flickr_source_personal_new (const GrlPluginInfo *plugin,
                                            const gchar *flickr_api_key,
                                            const gchar *flickr_secret,
                                            const gchar *flickr_token);

static void grl_flickr_source_finalize (GObject *object);

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
  gchar *flickr_key;
  gchar *flickr_secret;
  gchar *flickr_token;
  GrlConfig *config;
  gboolean public_source_created = FALSE;

  GRL_LOG_DOMAIN_INIT (flickr_log_domain, "flickr");

  GRL_DEBUG ("flickr_plugin_init");

  if (!configs) {
    GRL_WARNING ("Missing configuration");
    return FALSE;
  }

  while (configs) {
    config = GRL_CONFIG (configs->data);

    flickr_key = grl_config_get_api_key (config);
    flickr_token = grl_config_get_api_token (config);
    flickr_secret = grl_config_get_api_secret (config);

    if (!flickr_key || !flickr_secret) {
      GRL_WARNING ("Required configuration keys not set up");
    } else if (flickr_token) {
      grl_flickr_source_personal_new (plugin,
                                      flickr_key,
                                      flickr_secret,
                                      flickr_token);
    } else if (public_source_created) {
      GRL_WARNING ("Only one public source can be created");
    } else {
      GrlFlickrSource *source = grl_flickr_source_public_new (flickr_key, flickr_secret);
      public_source_created = TRUE;
      grl_plugin_registry_register_source (registry,
                                           plugin,
                                           GRL_MEDIA_PLUGIN (source),
                                           NULL);
    }

    if (flickr_key != NULL)
      g_free (flickr_key);
    if (flickr_token != NULL)
      g_free (flickr_token);
    if (flickr_secret != NULL)
      g_free (flickr_secret);

    configs = g_list_next (configs);
  }

  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_flickr_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== Flickr GObject ================ */

G_DEFINE_TYPE (GrlFlickrSource, grl_flickr_source, GRL_TYPE_MEDIA_SOURCE);

static GrlFlickrSource *
grl_flickr_source_public_new (const gchar *flickr_api_key,
                              const gchar *flickr_secret)
{
  GrlFlickrSource *source;

  GRL_DEBUG ("grl_flickr_source_new");

  source = g_object_new (GRL_FLICKR_SOURCE_TYPE,
                         "source-id", PUBLIC_SOURCE_ID,
                         "source-name", PUBLIC_SOURCE_NAME,
                         "source-desc", PUBLIC_SOURCE_DESC,
                         NULL);
  source->priv->flickr = g_flickr_new (flickr_api_key, flickr_secret, NULL);

  return source;
}

static void
grl_flickr_source_personal_new (const GrlPluginInfo *plugin,
                                const gchar *flickr_api_key,
                                const gchar *flickr_secret,
                                const gchar *flickr_token)
{
  GFlickr *f;

  f = g_flickr_new (flickr_api_key, flickr_secret, flickr_token);
  g_flickr_auth_checkToken (f, flickr_token, token_info_cb, (gpointer) plugin);
}

static void
grl_flickr_source_class_init (GrlFlickrSourceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GrlMediaSourceClass *source_class = GRL_MEDIA_SOURCE_CLASS (klass);
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);

  gobject_class->finalize = grl_flickr_source_finalize;
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

  grl_media_source_set_auto_split_threshold (GRL_MEDIA_SOURCE (source),
                                             SEARCH_MAX);
}

static void
grl_flickr_source_finalize (GObject *object)
{
  GrlFlickrSource *source;

  GRL_DEBUG ("grl_flickr_source_finalize");

  source = GRL_FLICKR_SOURCE (object);
  g_free (source->priv->user_id);
  g_object_unref (source->priv->flickr);

  G_OBJECT_CLASS (grl_flickr_source_parent_class)->finalize (object);
}

/* ======================= Utilities ==================== */

static void
token_info_cb (GFlickr *f,
               GHashTable *info,
               gpointer user_data)
{
  GrlFlickrSource *source;
  GrlPluginInfo *plugin = (GrlPluginInfo *) user_data;
  GrlPluginRegistry *registry;
  gchar *fullname;
  gchar *source_desc;
  gchar *source_id;
  gchar *source_name;
  gchar *username;

  if (!info) {
    GRL_WARNING ("Wrong token!");
    g_object_unref (f);
    return;
  }

  registry = grl_plugin_registry_get_default ();

  username = g_hash_table_lookup (info, "user_username");
  fullname = g_hash_table_lookup (info, "user_fullname");

  source_id = g_strdup_printf (PERSONAL_SOURCE_ID, username);
  source_name = g_strdup_printf (PERSONAL_SOURCE_NAME, fullname);
  source_desc = g_strdup_printf (PERSONAL_SOURCE_DESC, fullname);

  /* Check if source is already registered */
  if (grl_plugin_registry_lookup_source (registry, source_id)) {
    GRL_DEBUG ("A source with id '%s' is already registered. Skipping...",
               source_id);
    g_object_unref (f);
  } else {
    source = g_object_new (GRL_FLICKR_SOURCE_TYPE,
                           "source-id", source_id,
                           "source-name", source_name,
                           "source-desc", source_desc,
                           NULL);
    source->priv->flickr = f;
    source->priv->user_id = g_strdup (g_hash_table_lookup (info, "user_nsid"));
    grl_plugin_registry_register_source (registry,
                                         plugin,
                                         GRL_MEDIA_PLUGIN (source),
                                         NULL);
  }

  g_free (source_id);
  g_free (source_name);
  g_free (source_desc);
}

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
    if (!url) {
      url = g_flickr_photo_url_largest (NULL, photo);
    }
  }

  if (author) {
    grl_media_set_author (media, author);
  }

  if (date) {
    grl_media_set_date (media, date);
  }

  if (description && description[0] != '\0') {
    grl_media_set_description (media, description);
  }

  if (id) {
    grl_media_set_id (media, id);
  }

  if (thumbnail) {
    grl_media_set_thumbnail (media, thumbnail);
    g_free (thumbnail);
  }

  if (title && title[0] != '\0') {
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

  ms->callback (ms->source, ms->metadata_id, ms->media, ms->user_data, NULL);
}

static void
search_cb (GFlickr *f, GList *photolist, gpointer user_data)
{
  GrlMedia *media;
  OperationData *od = (OperationData *) user_data;
  gchar *media_type;

  /* Go to offset element */
  photolist = g_list_nth (photolist, od->offset);

  /* No more elements can be sent */
  if (!photolist) {
    od->callback (od->source,
                  od->operation_id,
                  NULL,
                  0,
                  od->user_data,
                  NULL);
    g_slice_free (OperationData, od);
    return;
  }

  while (photolist && od->count) {
    media_type = g_hash_table_lookup (photolist->data, "photo_media");
    if (strcmp (media_type, "photo") == 0) {
      media = grl_media_image_new ();
    } else {
      media = grl_media_video_new ();
    }
    update_media (media, photolist->data);
    od->callback (od->source,
                  od->operation_id,
                  media,
                  od->count == 1? 0: -1,
                  od->user_data,
                  NULL);
    photolist = g_list_next (photolist);
    od->count--;
  }

  /* Get more elements */
  if (od->count) {
    od->offset = 0;
    od->page++;
    g_flickr_photos_search (f,
                            od->user_id,
                            od->text,
                            od->tags,
                            od->page,
                            search_cb,
                            od);
  } else {
    g_slice_free (OperationData, od);
  }
}

static void
photosetslist_cb (GFlickr *f, GList *photosets, gpointer user_data)
{
  GrlMedia *media;
  GrlMediaSourceBrowseSpec *bs = (GrlMediaSourceBrowseSpec *) user_data;
  gchar *value;
  gint count;

  /* Go to offset element */
  photosets = g_list_nth (photosets, bs->skip);

  /* No more elements can be sent */
  if (!photosets) {
    bs->callback (bs->source,
                  bs->browse_id,
                  NULL,
                  0,
                  bs->user_data,
                  NULL);
    return;
  }

  /* Send data */
  count = g_list_length (photosets);
  if (count > bs->count) {
    count = bs->count;
  }

  while (photosets && count > 0) {
    count--;
    media = grl_media_box_new ();
    grl_media_set_id (media,
                      g_hash_table_lookup (photosets->data,
                                           "photoset_id"));
    value = g_hash_table_lookup (photosets->data, "title");
    if (value && value[0] != '\0') {
      grl_media_set_title (media, value);
    }
    value = g_hash_table_lookup (photosets->data, "description");
    if (value && value[0] != '\0') {
      grl_media_set_description (media, value);
    }

    bs->callback (bs->source,
                  bs->browse_id,
                  media,
                  count,
                  bs->user_data,
                  NULL);
    photosets = g_list_next (photosets);
  }
}

static void
photosetsphotos_cb (GFlickr *f, GList *photolist, gpointer user_data)
{
  GrlMedia *media;
  OperationData *od = (OperationData *) user_data;
  gchar *media_type;

  /* Go to offset element */
  photolist = g_list_nth (photolist, od->offset);

  /* No more elements can be sent */
  if (!photolist) {
    od->callback (od->source,
                  od->operation_id,
                  NULL,
                  0,
                  od->user_data,
                  NULL);
    return;
  }

  while (photolist && od->count) {
    media_type = g_hash_table_lookup (photolist->data, "photo_media");
    if (strcmp (media_type, "photo") == 0) {
      media = grl_media_image_new ();
    } else {
      media = grl_media_video_new ();
    }

    update_media (media, photolist->data);
    od->callback (od->source,
                  od->operation_id,
                  media,
                  od->count == 1? 0: -1,
                  od->user_data,
                  NULL);
    photolist = g_list_next (photolist);
    od->count--;
  }

  /* Get more elements */
  if (od->count) {
    od->offset = 0;
    od->page++;
    g_flickr_photosets_getPhotos (f, od->text, od->page, photosetsphotos_cb, od);
  } else {
    g_slice_free (OperationData, od);
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
grl_flickr_source_public_browse (GrlMediaSource *source,
                                 GrlMediaSourceBrowseSpec *bs)
{
  GFlickr *f = GRL_FLICKR_SOURCE (source)->priv->flickr;
  const gchar *container_id;
  guint per_page;
  gint request_size;

  container_id = grl_media_get_id (bs->container);

  if (!container_id) {
    /* Get hot tags list. List is limited up to HOTLIST_MAX tags */
    if (bs->skip >= HOTLIST_MAX) {
      bs->callback (bs->source, bs->browse_id, NULL, 0, bs->user_data, NULL);
    } else {
      request_size = CLAMP (bs->skip + bs->count, 1, HOTLIST_MAX);
      g_flickr_tags_getHotList (f, request_size, gettags_cb, bs);
    }
  } else {
    OperationData *od = g_slice_new (OperationData);

    grl_paging_translate (bs->skip,
                          bs->count,
                          SEARCH_MAX,
                          &per_page,
                          &(od->page),
                          &(od->offset));
    g_flickr_set_per_page (f, per_page);

    od->source = bs->source;
    od->callback = bs->callback;
    od->user_id = GRL_FLICKR_SOURCE (source)->priv->user_id;
    od->tags = (gchar *) container_id;
    od->text = NULL;
    od->user_data = bs->user_data;
    od->count = bs->count;
    od->operation_id = bs->browse_id;
    g_flickr_photos_search (f,
                            od->user_id,
                            NULL,
                            od->tags,
                            od->page,
                            search_cb,
                            od);
  }
}

static void
grl_flickr_source_personal_browse (GrlMediaSource *source,
                                   GrlMediaSourceBrowseSpec *bs)
{
  GFlickr *f = GRL_FLICKR_SOURCE (source)->priv->flickr;
  OperationData *od;
  const gchar *container_id;
  guint per_page;

  container_id = grl_media_get_id (bs->container);

  if (!container_id) {
    /* Get photoset */
    g_flickr_photosets_getList (f, NULL, photosetslist_cb, bs);
  } else {
    od = g_slice_new (OperationData);

    /* Compute items per page and page offset */
    grl_paging_translate (bs->skip,
                          bs->count,
                          SEARCH_MAX,
                          &per_page,
                          &(od->page),
                          &(od->offset));
    g_flickr_set_per_page (f, per_page);
    od->source = bs->source;
    od->callback = bs->callback;
    od->tags = NULL;
    od->text = (gchar *) container_id;
    od->user_data = bs->user_data;
    od->count = bs->count;
    od->operation_id = bs->browse_id;

    g_flickr_photosets_getPhotos (f, container_id, od->page, photosetsphotos_cb, od);
  }
}

static void
grl_flickr_source_browse (GrlMediaSource *source,
                          GrlMediaSourceBrowseSpec *bs)
{
  if (GRL_FLICKR_SOURCE (source)->priv->user_id) {
    grl_flickr_source_personal_browse (source, bs);
  } else {
    grl_flickr_source_public_browse (source, bs);
  }
}

static void
grl_flickr_source_metadata (GrlMediaSource *source,
                            GrlMediaSourceMetadataSpec *ms)
{
  const gchar *id;

  if (!ms->media || (id = grl_media_get_id (ms->media)) == NULL) {
    ms->callback (ms->source, ms->metadata_id, ms->media, ms->user_data, NULL);
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
  guint per_page;
  OperationData *od = g_slice_new (OperationData);

  /* Compute items per page and page offset */
  grl_paging_translate (ss->skip,
                        ss->count,
                        SEARCH_MAX,
                        &per_page,
                        &(od->page),
                        &(od->offset));
  g_flickr_set_per_page (f, per_page);

  od->source = ss->source;
  od->callback = ss->callback;
  od->user_id = GRL_FLICKR_SOURCE (source)->priv->user_id;
  od->tags = NULL;
  od->text = ss->text;
  od->user_data = ss->user_data;
  od->count = ss->count;
  od->operation_id = ss->search_id;

  if (od->user_id || od->text) {
    g_flickr_photos_search (f,
                            od->user_id,
                            ss->text,
                            NULL,
                            od->page,
                            search_cb,
                            od);
  } else {
    g_flickr_photos_getRecent (f,
                               od->page,
                               search_cb,
                               od);
  }
}
