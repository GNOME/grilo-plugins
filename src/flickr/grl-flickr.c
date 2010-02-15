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
#include <flickcurl.h>
#include <string.h>

#include "grl-flickr.h"
#include "gflickr.h"

#define GRL_FLICKR_SOURCE_GET_PRIVATE(object)                           \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                                \
                               GRL_FLICKR_SOURCE_TYPE,                  \
                               GrlFlickrSourcePrivate))

typedef struct {
  GrlMediaSourceSearchSpec *ss;
  GrlContentMedia *media;
  gint remaining;
} SearchData;

/* --------- Logging  -------- */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "grl-flickr"

/* ----- Security tokens ---- */

#define FLICKR_KEY    "fa037bee8120a921b34f8209d715a2fa"
#define FLICKR_SECRET "9f6523b9c52e3317"
#define FLICKR_FROB   "416-357-743"
#define FLICKR_TOKEN  "72157623286932154-c90318d470e96a29"

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

static GrlFlickrSource *grl_flickr_source_new (void);

gboolean grl_flickr_plugin_init (GrlPluginRegistry *registry,
				 const GrlPluginInfo *plugin);

static const GList *grl_flickr_source_supported_keys (GrlMetadataSource *source);

static void grl_flickr_source_metadata (GrlMediaSource *source,
                                        GrlMediaSourceMetadataSpec *ss);

static void grl_flickr_source_search (GrlMediaSource *source,
                                      GrlMediaSourceSearchSpec *ss);

/* =================== Flickr Plugin  =============== */

gboolean
grl_flickr_plugin_init (GrlPluginRegistry *registry,
                        const GrlPluginInfo *plugin)
{
  g_debug ("flickr_plugin_init\n");

  GrlFlickrSource *source = grl_flickr_source_new ();
  if (source) {
    grl_plugin_registry_register_source (registry,
                                         plugin,
                                         GRL_MEDIA_PLUGIN (source));
    return TRUE;
  } else {
    return FALSE;
  }
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

  if (flickcurl_init ()) {
    g_warning ("Unable to initialize Flickcurl");
    return NULL;
  }

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
}

static void
grl_flickr_source_init (GrlFlickrSource *source)
{
  source->priv = GRL_FLICKR_SOURCE_GET_PRIVATE (source);

  if (!g_thread_supported ()) {
    g_thread_init (NULL);
  }
}

G_DEFINE_TYPE (GrlFlickrSource, grl_flickr_source, GRL_TYPE_MEDIA_SOURCE);

/* ======================= Utilities ==================== */

static void
update_media (GrlContentMedia *media, flickcurl_photo *fc_photo)
{
  if (fc_photo->uri) {
    grl_content_media_set_url (media, fc_photo->uri);
  }
  if (fc_photo->fields[PHOTO_FIELD_owner_realname].string) {
    grl_content_media_set_author (media,
                                  fc_photo->fields[PHOTO_FIELD_owner_realname].string);
  }
  if (fc_photo->fields[PHOTO_FIELD_title].string) {
    grl_content_media_set_title (media,
                                 fc_photo->fields[PHOTO_FIELD_title].string);
  }
  if (fc_photo->fields[PHOTO_FIELD_description].string) {
    grl_content_media_set_description (media,
                                       fc_photo->fields[PHOTO_FIELD_description].string);
  }
  if (fc_photo->fields[PHOTO_FIELD_dates_taken].string) {
    grl_content_media_set_date (media,
                                fc_photo->fields[PHOTO_FIELD_dates_taken].string);
  }
}

static GrlContentMedia *
get_content_image (flickcurl_photo *fc_photo)
{
  GrlContentMedia *media;

  if (strcmp (fc_photo->media_type, "photo") == 0) {
    media = grl_content_image_new ();
  } else {
    media = grl_content_video_new ();
  }

  grl_content_media_set_id (media, fc_photo->id);
  update_media (media, fc_photo);

  return media;
}

static gboolean
search_cb (gpointer data)
{
  SearchData *search_data = (SearchData *) data;

  search_data->ss->callback(search_data->ss->source,
                            search_data->ss->search_id,
                            search_data->media,
                            search_data->remaining,
                            search_data->ss->user_data,
                            NULL);

  g_free (data);

  return FALSE;
}

/* Make get_url TRUE if url has been requested.
 * Make get_others TRUE if other (supported) keys has been requested
 */
static void
check_keys (GList *keys, gboolean *get_url, gboolean *get_others)
{
  GList *iter;
  GrlKeyID key_id;
  gboolean others = get_others? FALSE: TRUE;
  gboolean url = get_url? FALSE: TRUE;

  iter = keys;
  while (iter && (!url || !others)) {
    key_id = POINTER_TO_GRLKEYID (iter->data);
    if (key_id == GRL_METADATA_KEY_AUTHOR ||
        key_id == GRL_METADATA_KEY_DESCRIPTION ||
        key_id == GRL_METADATA_KEY_DATE) {
      others = TRUE;
    } else if (key_id == GRL_METADATA_KEY_URL) {
      url = TRUE;
    }
    iter = g_list_next (iter);
  }

  if (get_url) {
    *get_url = url;
  }

  if (get_others) {
    *get_others = others;
  }
}

#if 0
static gpointer
grl_flickr_source_metadata_main (gpointer data)
{
  GrlMediaSourceMetadataSpec *ms = (GrlMediaSourceMetadataSpec *) data;
  const gchar *id;
  flickcurl *fc = NULL;
  flickcurl_photo *photo;
  flickcurl_size **photo_sizes;
  gboolean get_other_keys;
  gboolean get_slow_url;
  gchar *url;
  gint s;
  gint width, height;

  if (!ms->media || (id = grl_content_media_get_id (ms->media)) == NULL) {
    g_idle_add (metadata_cb, ms);
    return NULL;
  }

  check_keys (ms->keys, &get_slow_url, &get_other_keys);

  if (ms->flags & GRL_RESOLVE_FAST_ONLY && get_other_keys) {
    get_slow_url = FALSE;
  }

  fc = flickcurl_new ();
  flickcurl_set_api_key (fc, FLICKR_KEY);
  flickcurl_set_auth_token (fc, FLICKR_TOKEN);
  flickcurl_set_shared_secret (fc, FLICKR_SECRET);

  if (get_slow_url) {
    photo_sizes = flickcurl_photos_getSizes (fc, id);
    if (photo_sizes) {
      url = photo_sizes[0]->source;
      width = photo_sizes[0]->width;
      height = photo_sizes[0]->height;

      /* Look for "Original" size */
      s = 0;
      while (photo_sizes[s]) {
        if (strcmp (photo_sizes[s]->label, "Original") == 0) {
          url = photo_sizes[s]->source;
          width = photo_sizes[s]->width;
          height = photo_sizes[s]->height;
          break;
        }
        s++;
      }

      /* Update media */
      grl_content_media_set_url (ms->media, url);
      if (GRL_IS_CONTENT_IMAGE (ms->media)) {
        grl_content_image_set_size (GRL_CONTENT_IMAGE (ms->media),
                                    width,
                                    height);
      } else if (GRL_IS_CONTENT_VIDEO (ms->media)) {
        grl_content_video_set_size (GRL_CONTENT_VIDEO (ms->media),
                                    width,
                                    height);
      }
      flickcurl_free_sizes (photo_sizes);
    }
  }

  photo = flickcurl_photos_getInfo (fc, id);
  if (photo) {
    update_media (ms->media, photo);
  }

  flickcurl_free_photo (photo);

  flickcurl_free (fc);

  g_idle_add (metadata_cb, ms);

  return NULL;
}
#endif

static gpointer
grl_flickr_source_search_main (gpointer data)
{
  GrlContentMedia *media;
  GrlMediaSourceSearchSpec *ss = (GrlMediaSourceSearchSpec *) data;
  SearchData *search_data;
  char *url;
  flickcurl *fc = NULL;
  flickcurl_photo *photo;
  flickcurl_photos_list *result;
  flickcurl_photos_list_params lparams;
  flickcurl_search_params sparams;
  flickcurl_size **photo_sizes;
  gboolean get_slow_keys = FALSE;
  gboolean get_slow_url = FALSE;
  int height;
  int i, s;
  int offset_in_page;
  int per_page;
  int width;

  fc = flickcurl_new ();
  flickcurl_set_api_key (fc, FLICKR_KEY);
  flickcurl_set_auth_token (fc, FLICKR_TOKEN);
  flickcurl_set_shared_secret (fc, FLICKR_SECRET);

  flickcurl_search_params_init (&sparams);
  flickcurl_photos_list_params_init (&lparams);
  sparams.text = ss->text;

  /* Compute page offset */
  per_page = 1 + ss->skip + ss->count;
  lparams.per_page = per_page > 100? 100: per_page;
  lparams.page = 1 + (ss->skip/lparams.per_page);
  offset_in_page = 1 + (ss->skip%lparams.per_page);

  /* Check if we need need to ask for complete information for each photo */
  if (!(ss->flags & GRL_RESOLVE_FAST_ONLY)) {
    /* Check if some "slow" key is requested */
    check_keys (ss->keys, &get_slow_url, &get_slow_keys);
  }

  for (;;) {
    result = flickcurl_photos_search_params (fc, &sparams, &lparams);
    /* No (more) results */
    if (!result || result->photos_count == 0) {
      g_debug ("No (more) results");
      search_data = g_new (SearchData, 1);
      search_data->ss = ss;
      search_data->media = NULL;
      search_data->remaining = 0;
      g_idle_add (search_cb, search_data);
      break;
    }
    for (i = offset_in_page; i < result->photos_count && ss->count > 0; i++) {
      /* As we are not computing whether there are enough photos to satisfy user
         requirement, use -1 in remaining elements (i.e., "unknown"), and use 0
         no more elements are/can be sent */
      media = get_content_image (result->photos[i]);
      if (get_slow_url) {
        photo_sizes = flickcurl_photos_getSizes (fc, result->photos[i]->id);
        if (photo_sizes) {
          url = photo_sizes[0]->source;
          width = photo_sizes[0]->width;
          height = photo_sizes[0]->height;

          /* Look for "Original" size */
          s = 0;
          while (photo_sizes[s]) {
            if (strcmp (photo_sizes[s]->label, "Original") == 0) {
              url = photo_sizes[s]->source;
              width = photo_sizes[s]->width;
              height = photo_sizes[s]->height;
              break;
            }
            s++;
          }

          /* Update media */
          grl_content_media_set_url (media, url);
          if (GRL_IS_CONTENT_IMAGE (media)) {
            grl_content_image_set_size (GRL_CONTENT_IMAGE (media),
                                        width,
                                        height);
          } else if (GRL_IS_CONTENT_VIDEO (media)) {
            grl_content_video_set_size (GRL_CONTENT_VIDEO (media),
                                        width,
                                        height);
          }
          flickcurl_free_sizes (photo_sizes);
        }
      }

      if (get_slow_keys) {
        photo = flickcurl_photos_getInfo (fc, result->photos[i]->id);
        if (photo) {
          update_media (media, photo);
        }

        flickcurl_free_photo (photo);
      }

      search_data = g_new (SearchData, 1);
      search_data->ss = ss;
      search_data->media = media;
      search_data->remaining = ss->count == 1? 0: -1;
      g_idle_add (search_cb, search_data);

      ss->count--;
    }
    /* Sent all requested photos */
    if (ss->count == 0) {
      g_debug ("All results sent");
      break;
    }
    flickcurl_free_photos_list (result);
    offset_in_page = 0;
    lparams.page++;
  }
  /* Free last results */
  flickcurl_free_photos_list (result);

  flickcurl_free (fc);

  return NULL;
}

static void
getInfo_cb (gpointer f, GHashTable *photo, gpointer user_data)
{
  GrlMediaSourceMetadataSpec *ms = (GrlMediaSourceMetadataSpec *) user_data;
  gchar *author;
  gchar *date;
  gchar *description;
  gchar *thumbnail;
  gchar *title;
  gchar *url;

  if (photo) {
    author = g_hash_table_lookup (photo, "owner_realname");
    title = g_hash_table_lookup (photo, "title");
    description = g_hash_table_lookup (photo, "description");
    date = g_hash_table_lookup (photo, "dates_taken");
    url = g_flickr_photo_url_original (f, photo);
    thumbnail = g_flickr_photo_url_thumbnail (f, photo);

    if (author) {
      grl_content_media_set_author (ms->media, author);
    }

    if (title) {
      grl_content_media_set_title (ms->media, title);
    }

    if (description) {
      grl_content_media_set_description (ms->media, description);
    }

    if (date) {
      grl_content_media_set_date (ms->media, date);
    }

    if (url) {
      grl_content_media_set_url (ms->media, url);
    }

    if (thumbnail) {
      grl_content_media_set_thumbnail (ms->media, thumbnail);
    }
  }

  ms->callback (ms->source, ms->media, ms->user_data, NULL);
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

  if (!ms->media || (id = grl_content_media_get_id (ms->media)) == NULL) {
    ms->callback (ms->source, ms->media, ms->user_data, NULL);
    return;
  }

  g_flickr_photos_getInfo (NULL, atol (id), getInfo_cb, ms);
}

static void
grl_flickr_source_search (GrlMediaSource *source,
                          GrlMediaSourceSearchSpec *ss)
{
  if (!g_thread_create (grl_flickr_source_search_main,
                        ss,
                        FALSE,
                        NULL)) {
    g_critical ("Unable to create thread");
  }
}
