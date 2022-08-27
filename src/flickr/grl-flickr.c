/*
 * Copyright (C) 2010, 2011 Igalia S.L.
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
#include <glib/gi18n-lib.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

#include "grl-flickr.h"
#include "gflickr.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT flickr_log_domain
GRL_LOG_DOMAIN(flickr_log_domain);

#define SEARCH_MAX  500
#define HOTLIST_MAX 200

/* --- Plugin information --- */

#define PUBLIC_SOURCE_ID   "grl-flickr"
#define PUBLIC_SOURCE_NAME "Flickr"
#define PUBLIC_SOURCE_DESC _("A source for browsing and searching Flickr photos")

#define PERSONAL_SOURCE_ID "grl-flickr-%s"
/* "%s" is a full user name, like "John Doe" */
#define PERSONAL_SOURCE_NAME _("%s’s Flickr")
/* "%s" is a full user name, like "John Doe" */
#define PERSONAL_SOURCE_DESC _("A source for browsing and searching %s’s flickr photos")

typedef struct {
  GrlSource *source;
  GrlSourceResultCb callback;
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

static void grl_flickr_source_personal_new (GrlPlugin *plugin,
                                            const gchar *flickr_api_key,
                                            const gchar *flickr_secret,
                                            const gchar *flickr_token,
                                            const gchar *token_secret);

static void grl_flickr_source_finalize (GObject *object);

gboolean grl_flickr_plugin_init (GrlRegistry *registry,
                                 GrlPlugin *plugin,
                                 GList *configs);


static const GList *grl_flickr_source_supported_keys (GrlSource *source);

static void grl_flickr_source_browse (GrlSource *source,
                                      GrlSourceBrowseSpec *bs);

static void grl_flickr_source_resolve (GrlSource *source,
                                       GrlSourceResolveSpec *rs);

static void grl_flickr_source_search (GrlSource *source,
                                      GrlSourceSearchSpec *ss);

/* =================== Flickr Plugin  =============== */

gboolean
grl_flickr_plugin_init (GrlRegistry *registry,
                        GrlPlugin *plugin,
                        GList *configs)
{
  gchar *flickr_key           = NULL;
  gchar *flickr_secret        = NULL;
  gchar *flickr_token         = NULL;
  gchar *flickr_token_secret  = NULL;


  GrlConfig *config;
  gboolean public_source_created = FALSE;

  GRL_LOG_DOMAIN_INIT (flickr_log_domain, "flickr");

  GRL_DEBUG ("flickr_plugin_init");

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

 if (configs == NULL) {
    GRL_INFO ("Configuration not provided! Plugin not loaded");
    return FALSE;
  }

  while (configs) {
    config = GRL_CONFIG (configs->data);

    flickr_key = grl_config_get_api_key (config);
    flickr_token = grl_config_get_api_token (config);
    flickr_token_secret = grl_config_get_api_token_secret (config);
    flickr_secret = grl_config_get_api_secret (config);

    if (!flickr_key || !flickr_secret) {
      GRL_INFO ("Required API key or secret configuration not provdied. "
                " Plugin not loaded");
    } else if (flickr_token && flickr_token_secret) {
      grl_flickr_source_personal_new (plugin,
                                      flickr_key,
                                      flickr_secret,
                                      flickr_token,
                                      flickr_token_secret);
    } else if (public_source_created) {
      GRL_WARNING ("Only one public source can be created");
    } else {
      GrlFlickrSource *source = grl_flickr_source_public_new (flickr_key,
                                                              flickr_secret);
      public_source_created = TRUE;
      grl_registry_register_source (registry,
                                    plugin,
                                    GRL_SOURCE (source),
                                    NULL);
    }

    g_clear_pointer (&flickr_key, g_free);
    g_clear_pointer (&flickr_token, g_free);
    g_clear_pointer (&flickr_secret, g_free);
    g_clear_pointer (&flickr_token_secret, g_free);

    configs = g_list_next (configs);
  }

  return TRUE;
}

GRL_PLUGIN_DEFINE (GRL_MAJOR,
                   GRL_MINOR,
                   FLICKR_PLUGIN_ID,
                   "Flickr",
                   "A plugin for browsing and searching Flickr photos",
                   "Igalia S.L.",
                   VERSION,
                   "LGPL-2.1-or-later",
                   "http://www.igalia.com",
                   grl_flickr_plugin_init,
                   NULL,
                   NULL);

/* ================== Flickr GObject ================ */

G_DEFINE_TYPE_WITH_PRIVATE (GrlFlickrSource, grl_flickr_source, GRL_TYPE_SOURCE)

static GrlFlickrSource *
grl_flickr_source_public_new (const gchar *flickr_api_key,
                              const gchar *flickr_secret)
{
  GrlFlickrSource *source;
  const char *tags[] = {
    "net:internet",
    NULL
  };

  GRL_DEBUG ("grl_flickr_public_source_new");

  source = g_object_new (GRL_FLICKR_SOURCE_TYPE,
                         "source-id", PUBLIC_SOURCE_ID,
                         "source-name", PUBLIC_SOURCE_NAME,
                         "source-desc", PUBLIC_SOURCE_DESC,
                         "supported-media", GRL_SUPPORTED_MEDIA_IMAGE,
                         "source-tags", tags,
                         NULL);
  source->priv->flickr = g_flickr_new (flickr_api_key, flickr_secret,
                                       NULL, NULL);

  return source;
}

static void
grl_flickr_source_personal_new (GrlPlugin *plugin,
                                const gchar *flickr_api_key,
                                const gchar *flickr_secret,
                                const gchar *flickr_token,
                                const gchar *flickr_token_secret)
{
  GFlickr *f;

  GRL_DEBUG ("grl_flickr_personal_source_new");

  f = g_flickr_new (flickr_api_key, flickr_secret,
                    flickr_token, flickr_token_secret);

  g_flickr_auth_checkToken (f, flickr_token, token_info_cb,
                            (gpointer) plugin);
}

static void
grl_flickr_source_class_init (GrlFlickrSourceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

  gobject_class->finalize = grl_flickr_source_finalize;

  source_class->supported_keys = grl_flickr_source_supported_keys;
  source_class->browse = grl_flickr_source_browse;
  source_class->resolve = grl_flickr_source_resolve;
  source_class->search = grl_flickr_source_search;
}

static void
grl_flickr_source_init (GrlFlickrSource *source)
{
  source->priv = grl_flickr_source_get_instance_private (source);

  grl_source_set_auto_split_threshold (GRL_SOURCE (source), SEARCH_MAX);
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
  GrlRegistry *registry;
  gchar *fullname;
  gchar *source_desc;
  gchar *source_id;
  gchar *source_name;
  gchar *username;

  GrlPlugin *plugin = (GrlPlugin *) user_data;

  if (!info) {
    GRL_WARNING ("Wrong token!");
    g_object_unref (f);
    return;
  }

  registry = grl_registry_get_default ();

  username = g_hash_table_lookup (info, "user_username");
  fullname = g_hash_table_lookup (info, "user_fullname");

  /* Set source id */
  source_id = g_strdup_printf (PERSONAL_SOURCE_ID, username);

  source_name = g_strdup_printf (PERSONAL_SOURCE_NAME, fullname);
  source_desc = g_strdup_printf (PERSONAL_SOURCE_DESC, fullname);

  /* Check if source is already registered */
  if (grl_registry_lookup_source (registry, source_id)) {
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
    grl_registry_register_source (registry,
                                  plugin,
                                  GRL_SOURCE (source),
                                  NULL);
  }

  g_free (source_id);
  g_free (source_name);
  g_free (source_desc);
}

static void
update_media (GrlMedia *media, GHashTable *photo)
{
  GrlRelatedKeys *relkeys;
  gchar *image[2] = { NULL };
  gchar *author;
  gchar *date;
  gchar *description;
  gchar *id;
  gchar *small;
  gchar *thumbnail;
  gchar *title;
  gchar *url;
  gint i;

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
    GDateTime *date_time;
    date_time = g_flickr_parse_date (date);
    if (date_time) {
      grl_media_set_creation_date (media, date_time);
      g_date_time_unref (date_time);
    }
  }

  if (description && description[0] != '\0') {
    grl_media_set_description (media, description);
  }

  if (id) {
    grl_media_set_id (media, id);
  }

  if (title && title[0] != '\0') {
    grl_media_set_title (media, title);
  }

  if (url) {
    gchar *content_type;

    grl_media_set_url (media, url);

    content_type = g_content_type_guess (url, NULL, 0, NULL);
    if (content_type) {
      gchar *mime;

      mime = g_content_type_get_mime_type (content_type);
      if (mime) {
        grl_media_set_mime (media, mime);
        g_free (mime);
      }
      g_free (content_type);
    }
    g_free (url);
  }

  small = g_flickr_photo_url_small (NULL, photo);
  image[0] = small;
  image[1] = thumbnail;

  for (i = 0; i < G_N_ELEMENTS (image); i++) {
    if (image[i]) {
      relkeys = grl_related_keys_new_with_keys (GRL_METADATA_KEY_THUMBNAIL,
                                                image[i],
                                                NULL);
      grl_data_add_related_keys (GRL_DATA (media), relkeys);
    }
  }

  g_free (small);
  g_free (thumbnail);
}

static void
update_media_exif (GrlMedia *media, GHashTable *photo)
{
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, photo);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    GrlRelatedKeys *relkeys = NULL;

    /*
     * EXIF tags from: http://www.exif.org/Exif2-2.PDF
     * and values from:
     * http://www.sno.phy.queensu.ca/~phil/exiftool/TagNames/EXIF.html
     */
    if (g_strcmp0 (key, "Model") == 0) {
      relkeys = grl_related_keys_new_with_keys (GRL_METADATA_KEY_CAMERA_MODEL,
                                                value,
                                                NULL);
    } else if (g_strcmp0 (key, "Flash") == 0) {
      gboolean used = g_str_has_prefix (value, "Fired") ||
                      g_str_has_prefix (value, "On,") ||
                      g_str_has_prefix (value, "Auto, Fired");

      relkeys = grl_related_keys_new_with_keys (GRL_METADATA_KEY_FLASH_USED,
                                                used,
                                                NULL);
    } else if (g_strcmp0 (key, "ExposureTime") == 0) {
      /* ExposureTime is in the format "%d/%d" seconds */
      gchar *endptr;
      guint64 num, denom;

      errno = 0;
      num = g_ascii_strtoull (value, &endptr, 10);
      if (errno == ERANGE && (num == G_MAXINT64 || num == G_MININT64))
        continue;

      if (endptr == value || *endptr != '/' || *(endptr +1) == '\0')
        continue;

      errno = 0;
      denom = g_ascii_strtoull (endptr + 1, NULL, 10);
      if ((errno == ERANGE && (denom == G_MAXINT64 || denom == G_MININT64)) ||
          (errno != 0 && denom == 0))
        continue;

      relkeys = grl_related_keys_new_with_keys (GRL_METADATA_KEY_EXPOSURE_TIME,
                                                num / (gdouble) denom,
                                                NULL);
    } else if (g_strcmp0 (key, "ISO") == 0) {
      gdouble iso;

      errno = 0;
      iso = g_ascii_strtod (value, NULL);
      if (errno != ERANGE && (fabs (iso) != HUGE_VAL))
        relkeys = grl_related_keys_new_with_keys (GRL_METADATA_KEY_ISO_SPEED,
                                                  iso,
                                                  NULL);
    } else if (g_strcmp0 (key, "Orientation") == 0) {
      gint degrees;

      if (g_str_match_string ("rotate 90 cw", value, FALSE))
        degrees = 90;
      else if (g_str_match_string ("rotate 180", value, FALSE))
        degrees = 180;
      else if (g_str_match_string ("rotate 270 cw", value, FALSE))
        degrees = 270;
      else
        degrees = 0;

      relkeys = grl_related_keys_new_with_keys (GRL_METADATA_KEY_ORIENTATION,
                                                degrees,
                                                NULL);
    }

    if (relkeys)
      grl_data_add_related_keys (GRL_DATA (media), relkeys);
  }
}

static void
getExif_cb (GFlickr *f, GHashTable *photo, gpointer user_data)
{
  GrlSourceResolveSpec *rs = (GrlSourceResolveSpec *) user_data;

  if (photo)
    update_media_exif (rs->media, photo);

  rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
}

static void
getInfo_cb (GFlickr *f, GHashTable *photo, gpointer user_data)
{
  GrlSourceResolveSpec *rs = (GrlSourceResolveSpec *) user_data;

  if (!photo)
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);

  update_media (rs->media, photo);

  g_flickr_photos_getExif (f,
                           grl_media_get_id (rs->media),
                           getExif_cb,
                           rs);
}

static void
search_cb (GFlickr *f, GList *photolist, gpointer user_data)
{
  GrlMedia *media;
  OperationData *od = (OperationData *) user_data;

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
    media = grl_media_image_new ();
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
  GrlSourceBrowseSpec *bs = (GrlSourceBrowseSpec *) user_data;
  gchar *value;
  gint count, desired_count;

  /* Go to offset element */
  photosets = g_list_nth (photosets,
                          grl_operation_options_get_skip (bs->options));

  /* No more elements can be sent */
  if (!photosets) {
    bs->callback (bs->source,
                  bs->operation_id,
                  NULL,
                  0,
                  bs->user_data,
                  NULL);
    return;
  }

  /* Send data */
  count = g_list_length (photosets);
  desired_count = grl_operation_options_get_count (bs->options);
  if (count > desired_count) {
    count = desired_count;
  }

  while (photosets && count > 0) {
    count--;
    media = grl_media_container_new ();
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
                  bs->operation_id,
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
    if (media_type == NULL) {
      media = grl_media_new ();
    } else if (strcmp (media_type, "photo") == 0) {
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
    g_flickr_photosets_getPhotos (f, od->text, od->page,
                                  photosetsphotos_cb, od);
  } else {
    g_slice_free (OperationData, od);
  }
}

static void
gettags_cb (GFlickr *f, GList *taglist, gpointer user_data)
{
  GrlMedia *media;
  GrlSourceBrowseSpec *bs = (GrlSourceBrowseSpec *) user_data;
  gint count;

  /* Go to offset element */
  taglist = g_list_nth (taglist,
                        grl_operation_options_get_skip (bs->options));

  /* No more elements can be sent */
  if (!taglist) {
    bs->callback (bs->source,
                  bs->operation_id,
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
    media = grl_media_container_new ();
    grl_media_set_id (media, taglist->data);
    grl_media_set_title (media, taglist->data);
    bs->callback (bs->source,
                  bs->operation_id,
                  media,
                  count,
                  bs->user_data,
                  NULL);
    taglist = g_list_next (taglist);
  }
}

/* ================== API Implementation ================ */

static const GList *
grl_flickr_source_supported_keys (GrlSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_AUTHOR,
                                      GRL_METADATA_KEY_CREATION_DATE,
                                      GRL_METADATA_KEY_DESCRIPTION,
                                      GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_MIME,
                                      GRL_METADATA_KEY_THUMBNAIL,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_URL,
                                      NULL);
  }
  return keys;
}

static void
grl_flickr_source_public_browse (GrlSource *source,
                                 GrlSourceBrowseSpec *bs)
{
  GFlickr *f = GRL_FLICKR_SOURCE (source)->priv->flickr;
  const gchar *container_id;
  guint per_page;
  gint request_size;
  guint skip = grl_operation_options_get_skip (bs->options);
  gint count = grl_operation_options_get_count (bs->options);

  container_id = grl_media_get_id (bs->container);

  if (!container_id) {
    /* Get hot tags list. List is limited up to HOTLIST_MAX tags */
    if (skip >= HOTLIST_MAX) {
      bs->callback (bs->source, bs->operation_id,
                    NULL, 0,
                    bs->user_data, NULL);
    } else {
      request_size = CLAMP (skip + count, 1, HOTLIST_MAX);
      g_flickr_tags_getHotList (f, request_size, gettags_cb, bs);
    }
  } else {
    OperationData *od = g_slice_new (OperationData);

    grl_paging_translate (skip,
                          count,
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
    od->count = count;
    od->operation_id = bs->operation_id;
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
grl_flickr_source_personal_browse (GrlSource *source,
                                   GrlSourceBrowseSpec *bs)
{
  GFlickr *f = GRL_FLICKR_SOURCE (source)->priv->flickr;
  OperationData *od;
  const gchar *container_id;
  guint per_page;
  guint skip = grl_operation_options_get_skip (bs->options);
  gint count = grl_operation_options_get_count (bs->options);

  container_id = grl_media_get_id (bs->container);

  if (!container_id) {
    /* Get photoset */
    g_flickr_photosets_getList (f, NULL, photosetslist_cb, bs);
  } else {
    od = g_slice_new (OperationData);

    /* Compute items per page and page offset */
    grl_paging_translate (skip,
                          count,
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
    od->count = count;
    od->operation_id = bs->operation_id;

    g_flickr_photosets_getPhotos (f, container_id, od->page,
                                  photosetsphotos_cb, od);
  }
}

static void
grl_flickr_source_browse (GrlSource *source,
                          GrlSourceBrowseSpec *bs)
{
  if (GRL_FLICKR_SOURCE (source)->priv->user_id) {
    grl_flickr_source_personal_browse (source, bs);
  } else {
    grl_flickr_source_public_browse (source, bs);
  }
}

static void
grl_flickr_source_resolve (GrlSource *source,
                           GrlSourceResolveSpec *rs)
{
  const gchar *id;

  if (!rs->media || (id = grl_media_get_id (rs->media)) == NULL) {
    rs->callback (rs->source, rs->operation_id,
                  rs->media, rs->user_data, NULL);
    return;
  }

  g_flickr_photos_getInfo (GRL_FLICKR_SOURCE (source)->priv->flickr,
                           id,
                           getInfo_cb,
                           rs);
}

static void
grl_flickr_source_search (GrlSource *source,
                          GrlSourceSearchSpec *ss)
{
  GFlickr *f = GRL_FLICKR_SOURCE (source)->priv->flickr;
  guint per_page;
  OperationData *od = g_slice_new (OperationData);
  guint skip = grl_operation_options_get_skip (ss->options);
  gint count = grl_operation_options_get_count (ss->options);

  /* Compute items per page and page offset */
  grl_paging_translate (skip,
                        count,
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
  od->count = count;
  od->operation_id = ss->operation_id;

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
