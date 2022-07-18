/*
 * Copyright (C) 2010, 2011 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
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

#include <grilo.h>
#include <glib/gi18n-lib.h>
#include <net/grl-net.h>
#include <gdata/gdata.h>
#include <totem-pl-parser.h>
#include <string.h>

#include "grl-youtube.h"

enum {
  PROP_0,
  PROP_SERVICE,
};

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT youtube_log_domain
GRL_LOG_DOMAIN_STATIC(youtube_log_domain);

/* ----- Root categories ---- */

#define YOUTUBE_ROOT_NAME       "YouTube"

#define ROOT_DIR_FEEDS_INDEX      0
#define ROOT_DIR_CATEGORIES_INDEX 1

#define YOUTUBE_FEEDS_ID        "standard-feeds"
#define YOUTUBE_FEEDS_NAME      N_("Standard feeds")

#define YOUTUBE_CATEGORIES_ID   "categories"
#define YOUTUBE_CATEGORIES_NAME N_("Categories")
#define YOUTUBE_CATEGORIES_URL  "https://gdata.youtube.com/schemas/2007/categories.cat"

/* ----- Feeds categories ---- */

#define YOUTUBE_TOP_RATED_ID         (YOUTUBE_FEEDS_ID "/0")
#define YOUTUBE_TOP_RATED_NAME       N_("Top Rated")

/* --- Other --- */

#define YOUTUBE_MAX_CHUNK       50

#define YOUTUBE_VIDEO_INFO_URL  "https://www.youtube.com/get_video_info?video_id=%s"
#define YOUTUBE_VIDEO_URL       "https://www.youtube.com/get_video?video_id=%s&t=%s&asv="
#define YOUTUBE_CATEGORY_URL    "https://gdata.youtube.com/feeds/api/videos/-/%s?&start-index=%s&max-results=%s"
#define YOUTUBE_WATCH_URL       "https://www.youtube.com/watch?v="

#define YOUTUBE_VIDEO_MIME      "application/x-shockwave-flash"
#define YOUTUBE_SITE_URL        "www.youtube.com"


/* --- Plugin information --- */

#define SOURCE_ID   "grl-youtube"
#define SOURCE_NAME "YouTube"
#define SOURCE_DESC _("A source for browsing and searching YouTube videos")

/* --- Data types --- */

typedef void (*AsyncReadCbFunc) (gchar *data, gpointer user_data);

typedef void (*BuildMediaFromEntryCbFunc) (GrlMedia *media, gpointer user_data);

typedef struct {
  const gchar *id;
  const gchar *name;
  guint count;
} CategoryInfo;

typedef struct {
  GrlSource *source;
  GCancellable *cancellable;
  guint operation_id;
  const gchar *container_id;
  GList *keys;
  GrlResolutionFlags flags;
  guint skip;
  guint count;
  GrlSourceResultCb callback;
  gpointer user_data;
  guint error_code;
  CategoryInfo *category_info;
  guint emitted;
  guint matches;
  gint ref_count;
} OperationSpec;

typedef struct {
  GrlSource *source;
  GSourceFunc callback;
  gpointer user_data;
} BuildCategorySpec;

typedef struct {
  AsyncReadCbFunc callback;
  gchar *url;
  gpointer user_data;
} AsyncReadCb;

typedef struct {
  GrlMedia *media;
  GCancellable *cancellable;
  BuildMediaFromEntryCbFunc callback;
  gpointer user_data;
} SetMediaUrlAsyncReadCb;

typedef enum {
  YOUTUBE_MEDIA_TYPE_ROOT,
  YOUTUBE_MEDIA_TYPE_FEEDS,
  YOUTUBE_MEDIA_TYPE_CATEGORIES,
  YOUTUBE_MEDIA_TYPE_FEED,
  YOUTUBE_MEDIA_TYPE_CATEGORY,
  YOUTUBE_MEDIA_TYPE_VIDEO,
} YoutubeMediaType;

struct _GrlYoutubeSourcePriv {
  GDataService *service;

  GrlNetWc *wc;
};

#define YOUTUBE_CLIENT_ID "grilo"

static GrlYoutubeSource *grl_youtube_source_new (const gchar *api_key,
						 const gchar *client_id,
						 const gchar *format);

static void grl_youtube_source_set_property (GObject *object,
                                             guint propid,
                                             const GValue *value,
                                             GParamSpec *pspec);
static void grl_youtube_source_finalize (GObject *object);

gboolean grl_youtube_plugin_init (GrlRegistry *registry,
                                  GrlPlugin *plugin,
                                  GList *configs);

static const GList *grl_youtube_source_supported_keys (GrlSource *source);

static const GList *grl_youtube_source_slow_keys (GrlSource *source);

static void grl_youtube_source_search (GrlSource *source,
                                       GrlSourceSearchSpec *ss);

static void grl_youtube_source_browse (GrlSource *source,
                                       GrlSourceBrowseSpec *bs);

static void grl_youtube_source_resolve (GrlSource *source,
                                        GrlSourceResolveSpec *rs);

static gboolean grl_youtube_test_media_from_uri (GrlSource *source,
						 const gchar *uri);

static void grl_youtube_get_media_from_uri (GrlSource *source,
					    GrlSourceMediaFromUriSpec *mfus);

static void grl_youtube_source_cancel (GrlSource *source,
                                       guint operation_id);

static void produce_from_directory (CategoryInfo *dir, guint dir_size, OperationSpec *os);

/* ==================== Global Data  ================= */

guint root_dir_size = 2;
CategoryInfo root_dir[] = {
  {YOUTUBE_FEEDS_ID,      YOUTUBE_FEEDS_NAME,      1},
  {YOUTUBE_CATEGORIES_ID, YOUTUBE_CATEGORIES_NAME, -1},
  {NULL, NULL, 0}
};

CategoryInfo feeds_dir[] = {
  {YOUTUBE_TOP_RATED_ID,      YOUTUBE_TOP_RATED_NAME,       -1},
  {NULL, NULL, 0}
};

CategoryInfo *categories_dir = NULL;

static GrlYoutubeSource *ytsrc = NULL;

/* =================== YouTube Plugin  =============== */

gboolean
grl_youtube_plugin_init (GrlRegistry *registry,
                         GrlPlugin *plugin,
                         GList *configs)
{
  gchar *api_key;
  gchar *format;
  GrlConfig *config;
  gint config_count;
  GrlYoutubeSource *source;

  GRL_LOG_DOMAIN_INIT (youtube_log_domain, "youtube");

  GRL_DEBUG ("youtube_plugin_init");

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  if (!configs) {
    GRL_INFO ("Configuration not provided! Plugin not loaded");
    return FALSE;
  }

  config_count = g_list_length (configs);
  if (config_count > 1) {
    GRL_INFO ("Provided %d configs, but will only use one", config_count);
  }

  config = GRL_CONFIG (configs->data);
  api_key = grl_config_get_api_key (config);
  if (!api_key) {
    GRL_INFO ("Missing API Key, cannot load plugin");
    return FALSE;
  }
  format = grl_config_get_string (config, "format");

  source = grl_youtube_source_new (api_key, YOUTUBE_CLIENT_ID, format);

  grl_registry_register_source (registry,
                                plugin,
                                GRL_SOURCE (source),
                                NULL);

  g_free (api_key);
  g_free (format);

  return TRUE;
}

GRL_PLUGIN_DEFINE (GRL_MAJOR,
                   GRL_MINOR,
                   YOUTUBE_PLUGIN_ID,
                   "YouTube",
                   "A plugin for browsing and searching YouTube videos",
                   "Igalia",
                   VERSION,
                   "LGPL-2.1-or-later",
                   "http://www.igalia.com",
                   grl_youtube_plugin_init,
                   NULL,
                   NULL);

/* ================== YouTube GObject ================ */

G_DEFINE_TYPE_WITH_PRIVATE (GrlYoutubeSource, grl_youtube_source, GRL_TYPE_SOURCE)

static GrlYoutubeSource *
grl_youtube_source_new (const gchar *api_key, const gchar *client_id, const gchar *format)
{
  GrlYoutubeSource *source;
  GDataYouTubeService *service;
  GIcon *icon;
  GFile *file;
  const char *tags[] = {
    "net:internet",
    NULL
  };

  GRL_DEBUG ("grl_youtube_source_new");

  service = gdata_youtube_service_new (api_key, NULL);
  if (!service) {
    GRL_WARNING ("Failed to initialize gdata service");
    return NULL;
  }

  file = g_file_new_for_uri ("resource:///org/gnome/grilo/plugins/youtube/channel-youtube.svg");
  icon = g_file_icon_new (file);
  g_object_unref (file);

  /* Use auto-split mode because YouTube fails for queries
     that request more than YOUTUBE_MAX_CHUNK results */
  source = GRL_YOUTUBE_SOURCE (g_object_new (GRL_YOUTUBE_SOURCE_TYPE,
					     "source-id", SOURCE_ID,
					     "source-name", SOURCE_NAME,
					     "source-desc", SOURCE_DESC,
					     "auto-split-threshold",
					     YOUTUBE_MAX_CHUNK,
                                             "yt-service", service,
                                             "supported-media", GRL_SUPPORTED_MEDIA_VIDEO,
                                             "source-icon", icon,
                                             "source-tags", tags,
					     NULL));

  g_object_unref (icon);
  ytsrc = source;
  g_object_add_weak_pointer (G_OBJECT (source), (gpointer *) &ytsrc);

  return source;
}

static void
grl_youtube_source_class_init (GrlYoutubeSourceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

  gobject_class->set_property = grl_youtube_source_set_property;
  gobject_class->finalize = grl_youtube_source_finalize;

  source_class->supported_keys = grl_youtube_source_supported_keys;
  source_class->slow_keys = grl_youtube_source_slow_keys;
  source_class->cancel = grl_youtube_source_cancel;

  source_class->search = grl_youtube_source_search;
  source_class->browse = grl_youtube_source_browse;
  source_class->resolve = grl_youtube_source_resolve;
  source_class->test_media_from_uri = grl_youtube_test_media_from_uri;
  source_class->media_from_uri = grl_youtube_get_media_from_uri;

  g_object_class_install_property (gobject_class,
                                   PROP_SERVICE,
                                   g_param_spec_object ("yt-service",
                                                        "youtube-service",
                                                        "gdata youtube service object",
                                                        GDATA_TYPE_YOUTUBE_SERVICE,
                                                        G_PARAM_WRITABLE
                                                        | G_PARAM_CONSTRUCT_ONLY
                                                        | G_PARAM_STATIC_NAME));
}

static void
grl_youtube_source_init (GrlYoutubeSource *source)
{
  source->priv = grl_youtube_source_get_instance_private (source);
}

static void
grl_youtube_source_set_property (GObject *object,
                                 guint propid,
                                 const GValue *value,
                                 GParamSpec *pspec)

{
  switch (propid) {
  case PROP_SERVICE: {
    GrlYoutubeSource *self;
    self = GRL_YOUTUBE_SOURCE (object);
    self->priv->service = g_value_get_object (value);
    break;
  }
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, propid, pspec);
  }
}

static void
grl_youtube_source_finalize (GObject *object)
{
  GrlYoutubeSource *self;

  self = GRL_YOUTUBE_SOURCE (object);

  g_clear_object (&self->priv->wc);
  g_clear_object (&self->priv->service);

  G_OBJECT_CLASS (grl_youtube_source_parent_class)->finalize (object);
}

/* ======================= Utilities ==================== */

static void
entry_parsed_cb (TotemPlParser *parser,
                 const char    *uri,
                 GHashTable    *metadata,
                 GrlMedia      *media)
{
  grl_media_set_url (media, uri);
}

static void
release_operation_data (guint operation_id)
{
  GCancellable *cancellable = grl_operation_get_data (operation_id);

  g_clear_object (&cancellable);

  grl_operation_set_data (operation_id, NULL);
}

static OperationSpec *
operation_spec_new (void)
{
  OperationSpec *os;

  GRL_DEBUG ("Allocating new spec");

  os =  g_slice_new0 (OperationSpec);
  g_atomic_int_set (&os->ref_count, 1);

  return os;
}

static void
operation_spec_unref (OperationSpec *os)
{
  if (g_atomic_int_dec_and_test (&os->ref_count)) {
    g_clear_object (&os->cancellable);
    g_slice_free (OperationSpec, os);
    GRL_DEBUG ("freeing spec");
  }
}

static void
operation_spec_ref (OperationSpec *os)
{
  GRL_DEBUG ("Reffing spec");
  g_atomic_int_inc (&os->ref_count);
}

static void
build_media_from_entry (GrlYoutubeSource *source,
                        GrlMedia *content,
                        GDataEntry *entry,
                        GCancellable *cancellable,
                        const GList *keys,
                        BuildMediaFromEntryCbFunc callback,
                        gpointer user_data)
{
  GDataYouTubeVideo *video;
  GDataMediaThumbnail *thumbnail;
  GrlMedia *media;
  GList *iter;

  if (!content) {
    media = grl_media_video_new ();
  } else {
    media = content;
  }

  video = GDATA_YOUTUBE_VIDEO (entry);

  /* Make sure we set the media id in any case */
  if (!grl_media_get_id (media)) {
    grl_media_set_id (media, gdata_entry_get_id (entry));
  }

  iter = (GList *) keys;
  while (iter) {
    GrlKeyID key = GRLPOINTER_TO_KEYID (iter->data);
    if (key == GRL_METADATA_KEY_TITLE) {
      grl_media_set_title (media, gdata_entry_get_title (entry));
    } else if (key == GRL_METADATA_KEY_DESCRIPTION) {
      grl_media_set_description (media,
				 gdata_youtube_video_get_description (video));
    } else if (key == GRL_METADATA_KEY_THUMBNAIL) {
      GList *thumb_list;
      thumb_list = gdata_youtube_video_get_thumbnails (video);
      while (thumb_list) {
        thumbnail = GDATA_MEDIA_THUMBNAIL (thumb_list->data);
        grl_media_add_thumbnail (media,
                                 gdata_media_thumbnail_get_uri (thumbnail));
        thumb_list = g_list_next (thumb_list);
      }
    } else if (key == GRL_METADATA_KEY_PUBLICATION_DATE) {
      GTimeVal date;
      gint64 published = gdata_entry_get_published (entry);
      date.tv_sec = (glong) published;
      date.tv_usec = 0;
      if (date.tv_sec != 0 || date.tv_usec != 0) {
        GDateTime *date_time;
        date_time = g_date_time_new_from_timeval_utc (&date);
        grl_media_set_publication_date (media, date_time);
        g_date_time_unref (date_time);
      }
    } else if (key == GRL_METADATA_KEY_DURATION) {
      grl_media_set_duration (media, gdata_youtube_video_get_duration (video));
    } else if (key == GRL_METADATA_KEY_MIME) {
      grl_media_set_mime (media, YOUTUBE_VIDEO_MIME);
    } else if (key == GRL_METADATA_KEY_SITE) {
      grl_media_set_site (media, gdata_youtube_video_get_player_uri (video));
    } else if (key == GRL_METADATA_KEY_EXTERNAL_URL) {
      grl_media_set_external_url (media,
				  gdata_youtube_video_get_player_uri (video));
    } else if (key == GRL_METADATA_KEY_RATING) {
      gdouble average;
      gdata_youtube_video_get_rating (video, NULL, NULL, NULL, &average);
      grl_media_set_rating (media, average, 5.00);
    } else if (key == GRL_METADATA_KEY_URL) {
      TotemPlParser *parser;
      TotemPlParserResult res;

      parser = totem_pl_parser_new ();
      g_signal_connect (parser, "entry-parsed",
                        G_CALLBACK (entry_parsed_cb), media);
      res = totem_pl_parser_parse (parser,
                                   (char *) gdata_youtube_video_get_player_uri (video),
                                   FALSE);
      if (res != TOTEM_PL_PARSER_RESULT_SUCCESS)
	GRL_WARNING ("Failed to get video URL. totem-pl-parser error '%d'", res);
      g_clear_object (&parser);
    } else if (key == GRL_METADATA_KEY_EXTERNAL_PLAYER) {
      const gchar *uri = gdata_youtube_video_get_player_uri (video);
      grl_media_set_external_player (media, uri);
    }
    iter = g_list_next (iter);
  }

  callback (media, user_data);
}

static void
build_categories_directory_read_cb (GObject *source_object,
                                    GAsyncResult *result,
                                    gpointer user_data)
{
  GDataYouTubeService *service;
  BuildCategorySpec *bcs;
  GDataAPPCategories *app_categories = NULL;
  GList *categories = NULL;  /*<unowned GDataCategory>*/
  GError *error = NULL;
  guint total = 0;
  GList *all = NULL, *iter;
  CategoryInfo *cat_info;
  guint index = 0;

  GRL_DEBUG (G_STRFUNC);

  service = GDATA_YOUTUBE_SERVICE (source_object);
  bcs = user_data;

  app_categories = gdata_youtube_service_get_categories_finish (service,
                                                                result,
                                                                &error);

  if (error != NULL) {
    g_error_free (error);
    goto done;
  }

  categories = gdata_app_categories_get_categories (app_categories);

  for (; categories != NULL; categories = categories->next) {
    GDataCategory *category = GDATA_CATEGORY (categories->data);

    cat_info = g_slice_new (CategoryInfo);
    cat_info->id = g_strconcat (YOUTUBE_CATEGORIES_ID, "/",
                                gdata_category_get_term (category), NULL);
    cat_info->name = g_strdup (gdata_category_get_label (category));
    all = g_list_prepend (all, cat_info);
    total++;
    GRL_DEBUG ("  Found category: '%d - %s'", index++, cat_info->name);
  }

  if (all) {
    root_dir[ROOT_DIR_CATEGORIES_INDEX].count = total;
    categories_dir = g_new0 (CategoryInfo, total + 1);
    iter = all;
    do {
      cat_info = (CategoryInfo *) iter->data;
      categories_dir[total - 1].id = cat_info->id ;
      categories_dir[total - 1].name = (gchar *) g_dgettext (GETTEXT_PACKAGE,
                                                             cat_info->name);
      categories_dir[total - 1].count = -1;
      total--;
      g_slice_free (CategoryInfo, cat_info);
      iter = g_list_next (iter);
    } while (iter);
    g_list_free (all);
  }

done:
  bcs->callback (bcs);
  g_slice_free (BuildCategorySpec, bcs);
}

static gint
get_feed_type_from_id (const gchar *feed_id)
{
  gchar *tmp;
  gchar *test;
  gint feed_type;

  tmp = g_strrstr (feed_id, "/");
  if (!tmp) {
    return -1;
  }
  tmp++;

  feed_type = strtol (tmp, &test, 10);
  if (*test != '\0') {
    return -1;
  }

  return feed_type;
}

static const gchar *
get_category_term_from_id (const gchar *category_id)
{
  gchar *term;
  term = g_strrstr (category_id, "/");
  if (!term) {
    return NULL;
  }
  return ++term;
}

static gint
get_category_index_from_id (const gchar *category_id)
{
  guint i;

  for (i=0; i<root_dir[ROOT_DIR_CATEGORIES_INDEX].count; i++) {
    if (!strcmp (categories_dir[i].id, category_id)) {
      return i;
    }
  }
  return -1;
}

static void
build_media_from_entry_resolve_cb (GrlMedia *media, gpointer user_data)
{
  GrlSourceResolveSpec *rs = (GrlSourceResolveSpec *) user_data;
  release_operation_data (rs->operation_id);
  rs->callback (rs->source, rs->operation_id, media, rs->user_data, NULL);
}

static void
build_media_from_entry_search_cb (GrlMedia *media, gpointer user_data)
{
  /*
   * TODO: Async resolution of URL messes (or could mess) with the sorting,
   * If we want to ensure a particular sorting or implement sorting
   * mechanisms we should add code to handle that here so we emit items in
   * the right order and not just when we got the URL resolved (would
   * damage response time though).
   */
  OperationSpec *os = (OperationSpec *) user_data;
  guint remaining;

  if (g_cancellable_is_cancelled (os->cancellable)) {
    GRL_DEBUG ("%s: cancelled", __FUNCTION__);
    return;
  }

  if (os->emitted < os->count) {
    remaining = os->count - os->emitted - 1;
    if (remaining == 0) {
      release_operation_data (os->operation_id);
    }
    os->callback (os->source,
		  os->operation_id,
		  media,
		  remaining,
		  os->user_data,
		  NULL);
    if (remaining == 0) {
      GRL_DEBUG ("Unreffing spec in build_media_from_entry_search_cb");
      operation_spec_unref (os);
    } else {
      os->emitted++;
    }
  }
}

static void
build_category_directory (BuildCategorySpec *bcs)
{
  GrlYoutubeSource *source;
  GDataYouTubeService *service;

  GRL_DEBUG (__FUNCTION__);

  source = GRL_YOUTUBE_SOURCE (bcs->source);
  service = GDATA_YOUTUBE_SERVICE (source->priv->service);
  gdata_youtube_service_get_categories_async (service, NULL,
                                              build_categories_directory_read_cb,
                                              bcs);
}

static void
resolve_cb (GObject *object,
            GAsyncResult *result,
            gpointer user_data)
{
  GRL_DEBUG (__FUNCTION__);

  GError *error = NULL;
  GrlYoutubeSource *source;
  GDataEntry *video;
  GDataService *service;
  GrlSourceResolveSpec *rs = (GrlSourceResolveSpec *) user_data;

  source = GRL_YOUTUBE_SOURCE (rs->source);
  service = GDATA_SERVICE (source->priv->service);

  video = gdata_service_query_single_entry_finish (service, result, &error);

  if (error) {
    release_operation_data (rs->operation_id);
    error->code = GRL_CORE_ERROR_RESOLVE_FAILED;
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);
    g_error_free (error);
  } else {
    build_media_from_entry (GRL_YOUTUBE_SOURCE (rs->source),
                            rs->media,
                            video,
                            grl_operation_get_data (rs->operation_id),
                            rs->keys,
			    build_media_from_entry_resolve_cb,
                            rs);
  }

  g_clear_object (&video);
}

static void
search_progress_cb (GDataEntry *entry,
		    guint index,
		    guint count,
		    gpointer user_data)
{
  OperationSpec *os = (OperationSpec *) user_data;

  /* Check if operation has been cancelled */
  if (g_cancellable_is_cancelled (os->cancellable)) {
    GRL_DEBUG ("%s: cancelled (%u, %u)", __FUNCTION__, index, count);
    build_media_from_entry_search_cb (NULL, os);
    return;
  }

  if (index < count) {
    /* Keep track of the items we got here. Due to the asynchronous
     * nature of build_media_from_entry(), when search_cb is invoked
     * we have to check if we got as many results as we requested or
     * not, and handle that situation properly */
    os->matches++;
    build_media_from_entry (GRL_YOUTUBE_SOURCE (os->source),
                            NULL,
                            entry,
                            os->cancellable,
                            os->keys,
                            build_media_from_entry_search_cb,
                            os);
  } else {
    GRL_WARNING ("Invalid index/count received grom libgdata, ignoring result");
  }

  /* The entry will be freed when freeing the feed in search_cb */
}

static void
search_cb (GObject *object, GAsyncResult *result, OperationSpec *os)
{
  GDataFeed *feed;
  GError *error = NULL;
  gboolean need_extra_unref = FALSE;
  GrlYoutubeSource *source = GRL_YOUTUBE_SOURCE (os->source);

  GRL_DEBUG ("search_cb");

  /* Check if operation was cancelled */
  if (g_cancellable_is_cancelled (os->cancellable)) {
    GRL_DEBUG ("Search operation has been cancelled");
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, NULL);
    operation_spec_unref (os);
    /* Look for OPERATION_SPEC_REF_RATIONALE for details on the reason for this
     * extra unref */
    operation_spec_unref (os);
    return;
  }

  feed = gdata_service_query_finish (source->priv->service, result, &error);
  if (!error && feed) {
    /* If we are browsing a category, update the count for it */
    if (os->category_info) {
      os->category_info->count = gdata_feed_get_total_results (feed);
    }

    /* Check if we got as many results as we requested */
    if (os->matches < os->count) {
      os->count = os->matches;
      /* In case we are resolving URLs asynchronously, from now on
       * results will be sent with appropriate remaining, but it can
       * also be the case that we have sent all the results already
       * and the last one was sent with remaining>0, in that case
       * we should send a finishing message now. */
      if (os->emitted == os->count) {
	GRL_DEBUG ("sending finishing message");
	os->callback (os->source, os->operation_id,
		      NULL, 0, os->user_data, NULL);
	need_extra_unref = TRUE;
      }
    }
  } else {
    if (!error) {
      error = g_error_new_literal (GRL_CORE_ERROR,
                                   os->error_code,
                                   _("Failed to get feed"));
    } else {
      error->code = os->error_code;
    }
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
    need_extra_unref = TRUE;
  }

  g_clear_object (&feed);

  GRL_DEBUG ("Unreffing spec in search_cb");
  operation_spec_unref (os);
  if (need_extra_unref) {
    /* We did not free the spec in the emission callback, do it here */
    GRL_DEBUG ("need extra spec unref in search_cb");
    operation_spec_unref (os);
  }
}

static gboolean
is_category_container (const gchar *container_id)
{
  return g_str_has_prefix (container_id, YOUTUBE_CATEGORIES_ID "/");
}

static gboolean
is_feeds_container (const gchar *container_id)
{
  return g_str_has_prefix (container_id, YOUTUBE_FEEDS_ID "/");
}

static YoutubeMediaType
classify_media_id (const gchar *media_id)
{
  if (!media_id) {
    return YOUTUBE_MEDIA_TYPE_ROOT;
  } else if (!strcmp (media_id, YOUTUBE_FEEDS_ID)) {
    return YOUTUBE_MEDIA_TYPE_FEEDS;
  } else if (!strcmp (media_id, YOUTUBE_CATEGORIES_ID)) {
    return YOUTUBE_MEDIA_TYPE_CATEGORIES;
  } else if (is_category_container (media_id)) {
    return YOUTUBE_MEDIA_TYPE_CATEGORY;
  } else if (is_feeds_container (media_id)) {
    return YOUTUBE_MEDIA_TYPE_FEED;
  } else {
    return YOUTUBE_MEDIA_TYPE_VIDEO;
  }
}

static void
set_category_childcount (GDataService *service,
			 GrlMedia *content,
                         CategoryInfo *dir,
                         guint index)
{
  gint childcount;
  gboolean set_childcount = TRUE;
  const gchar *container_id;

  container_id = grl_media_get_id (GRL_MEDIA (content));

  if (dir == NULL) {
    /* Special case: we want childcount of root category */
    childcount = root_dir_size;
  } else if (!strcmp (dir[index].id, YOUTUBE_FEEDS_ID)) {
    childcount = root_dir[ROOT_DIR_FEEDS_INDEX].count;
  } else if (!strcmp (dir[index].id, YOUTUBE_CATEGORIES_ID)) {
    childcount = root_dir[ROOT_DIR_CATEGORIES_INDEX].count;
  } else if (is_feeds_container (container_id)) {
    gint feed_index = get_feed_type_from_id (container_id);
    if (feed_index >= 0) {
      childcount = feeds_dir[feed_index].count;
    } else {
      set_childcount = FALSE;
    }
  } else if (is_category_container (container_id)) {
    gint cat_index = get_category_index_from_id (container_id);
    if (cat_index >= 0) {
      childcount = categories_dir[cat_index].count;
    } else {
      set_childcount = FALSE;
    }
  } else {
    set_childcount = FALSE;
  }

  if (set_childcount) {
    grl_media_set_childcount (content, childcount);
  }
}

static GrlMedia *
produce_container_from_directory (GDataService *service,
				  GrlMedia *media,
				  CategoryInfo *dir,
				  guint index)
{
  GrlMedia *content;

  if (!media) {
    /* Create mode */
    content = grl_media_container_new ();
  } else {
    /* Update mode */
    content = media;
  }

  if (!dir) {
    grl_media_set_id (content, NULL);
    grl_media_set_title (content, YOUTUBE_ROOT_NAME);
  } else {
    grl_media_set_id (content, dir[index].id);
    grl_media_set_title (content, g_dgettext (GETTEXT_PACKAGE, dir[index].name));
  }
  grl_media_set_site (content, YOUTUBE_SITE_URL);
  set_category_childcount (service, content, dir, index);

  return content;
}

static void
produce_from_directory (CategoryInfo *dir, guint dir_size, OperationSpec *os)
{
  guint index, remaining;

  GRL_DEBUG ("produce_from_directory");

  if (os->skip >= dir_size) {
    /* No results */
    os->callback (os->source,
		  os->operation_id,
		  NULL,
		  0,
		  os->user_data,
		  NULL);
    operation_spec_unref (os);
  } else {
    index = os->skip;
    remaining = MIN (dir_size - os->skip, os->count);

    do {
      GDataService *service = GRL_YOUTUBE_SOURCE (os->source)->priv->service;

      GrlMedia *content =
	produce_container_from_directory (service, NULL, dir, index);

      remaining--;
      index++;

      os->callback (os->source,
		    os->operation_id,
		    content,
		    remaining,
		    os->user_data,
		    NULL);

      if (remaining == 0) {
	operation_spec_unref (os);
      }
    } while (remaining > 0);
  }
}

static void
produce_from_feed (OperationSpec *os)
{
  GError *error = NULL;
  gint feed_type;
  GDataQuery *query;
  GDataService *service;

  feed_type = get_feed_type_from_id (os->container_id);

  if (feed_type < 0) {
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_BROWSE_FAILED,
                         _("Invalid feed identifier %s"),
                         os->container_id);
    os->callback (os->source,
		  os->operation_id,
		  NULL,
		  0,
		  os->user_data,
		  error);
    g_error_free (error);
    operation_spec_unref (os);
    return;
  }
  /* OPERATION_SPEC_REF_RATIONALE
   * Depending on wether the URL has been requested, metadata resolution
   * for each item in the result set may or may not be asynchronous.
   * We cannot free the spec in search_cb because that may be called
   * before the asynchronous URL resolution is finished, and we cannot
   * do it in build_media_from_entry_search_cb either, because in the
   * synchronous case (when we do not request URL) search_cb will
   * be invoked after it.
   * Thus, the solution is to increase the reference count here and
   * have both places unreffing the spec, that way, no matter which
   * is invoked last, the spec will be freed only once. */
  operation_spec_ref (os);

  os->cancellable = g_cancellable_new ();
  grl_operation_set_data (os->operation_id, g_object_ref (os->cancellable));

  service = GRL_YOUTUBE_SOURCE (os->source)->priv->service;

  /* Index in GData starts at 1 */
  query = GDATA_QUERY (gdata_youtube_query_new (NULL));
  gdata_query_set_start_index (query, os->skip + 1);
  gdata_query_set_max_results (query, os->count);
  os->category_info = &feeds_dir[feed_type];

  gdata_youtube_service_query_standard_feed_async (GDATA_YOUTUBE_SERVICE (service),
                                                   feed_type,
                                                   query,
                                                   os->cancellable,
                                                   search_progress_cb,
                                                   os,
                                                   NULL,
                                                   (GAsyncReadyCallback) search_cb,
                                                   os);

  g_object_unref (query);
}

static void
produce_from_category (OperationSpec *os)
{
  GError *error = NULL;
  GDataQuery *query;
  GDataService *service;
  const gchar *category_term;
  gint category_index;

  category_term = get_category_term_from_id (os->container_id);
  category_index = get_category_index_from_id (os->container_id);

  if (!category_term) {
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_BROWSE_FAILED,
                         _("Invalid category identifier %s"),
                         os->container_id);
    os->callback (os->source,
		  os->operation_id,
		  NULL,
		  0,
		  os->user_data,
		  error);
    g_error_free (error);
    operation_spec_unref (os);
    return;
  }

  /* Look for OPERATION_SPEC_REF_RATIONALE for details */
  operation_spec_ref (os);

  service = GRL_YOUTUBE_SOURCE (os->source)->priv->service;

  /* Index in GData starts at 1 */
  query = GDATA_QUERY (gdata_youtube_query_new (NULL));
  gdata_query_set_start_index (query, os->skip + 1);
  gdata_query_set_max_results (query, os->count);
  os->category_info = &categories_dir[category_index];
  gdata_query_set_categories (query, category_term);

  gdata_youtube_service_query_videos_async (GDATA_YOUTUBE_SERVICE (service),
                                            query,
                                            NULL,
                                            search_progress_cb,
                                            os,
                                            NULL,
                                            (GAsyncReadyCallback) search_cb,
                                            os);

  g_object_unref (query);
}

static gchar *
get_video_id_from_url (const gchar *url)
{
  gchar *marker, *end, *video_id;

  if (url == NULL) {
    return NULL;
  }

  marker = strstr (url, YOUTUBE_WATCH_URL);
  if (!marker) {
    return NULL;
  }

  marker += strlen (YOUTUBE_WATCH_URL);

  end = marker;
  while (*end != '\0' && *end != '&') {
    end++;
  }

  video_id = g_strndup (marker, end - marker);

  return video_id;
}

static void
build_media_from_entry_media_from_uri_cb (GrlMedia *media, gpointer user_data)
{
  GrlSourceMediaFromUriSpec *mfus = (GrlSourceMediaFromUriSpec *) user_data;

  release_operation_data (mfus->operation_id);
  mfus->callback (mfus->source, mfus->operation_id,
                  media, mfus->user_data, NULL);
}

static void
media_from_uri_cb (GObject *object, GAsyncResult *result, gpointer user_data)
{
  GError *error = NULL;
  GrlYoutubeSource *source;
  GDataEntry *video;
  GDataService *service;
  GrlSourceMediaFromUriSpec *mfus = (GrlSourceMediaFromUriSpec *) user_data;

  source = GRL_YOUTUBE_SOURCE (mfus->source);
  service = GDATA_SERVICE (source->priv->service);

  video = gdata_service_query_single_entry_finish (service, result, &error);

  if (error) {
    error->code = GRL_CORE_ERROR_MEDIA_FROM_URI_FAILED;
    release_operation_data (mfus->operation_id);
    mfus->callback (mfus->source, mfus->operation_id, NULL, mfus->user_data, error);
    g_error_free (error);
  } else {
    build_media_from_entry (GRL_YOUTUBE_SOURCE (mfus->source),
                            NULL,
                            video,
                            grl_operation_get_data (mfus->operation_id),
                            mfus->keys,
			    build_media_from_entry_media_from_uri_cb,
			    mfus);
  }

  g_clear_object (&video);
}

static gboolean
produce_from_category_cb (BuildCategorySpec *spec)
{
  produce_from_directory (categories_dir,
                          root_dir[ROOT_DIR_CATEGORIES_INDEX].count,
                          spec->user_data);
  return FALSE;
}

static gboolean
produce_container_from_category_cb (BuildCategorySpec *spec)
{
  GError *error = NULL;
  GrlMedia *media = NULL;

  GrlSourceResolveSpec *rs = (GrlSourceResolveSpec *) spec->user_data;
  GDataService *service = GRL_YOUTUBE_SOURCE (rs->source)->priv->service;
  const gchar *id = grl_media_get_id (rs->media);
  gint index = get_category_index_from_id (id);
  if (index >= 0) {
    media = produce_container_from_directory (service,
                                              rs->media,
                                              categories_dir,
                                              index);
  } else {
    media = rs->media;
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_RESOLVE_FAILED,
                         _("Invalid category identifier %s"),
                         id);
  }

  rs->callback (rs->source, rs->operation_id, media, rs->user_data, error);
  g_clear_error (&error);

  return FALSE;
}

/* ================== API Implementation ================ */

static const GList *
grl_youtube_source_supported_keys (GrlSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_URL,
                                      GRL_METADATA_KEY_EXTERNAL_URL,
                                      GRL_METADATA_KEY_DESCRIPTION,
                                      GRL_METADATA_KEY_DURATION,
                                      GRL_METADATA_KEY_PUBLICATION_DATE,
                                      GRL_METADATA_KEY_THUMBNAIL,
                                      GRL_METADATA_KEY_MIME,
                                      GRL_METADATA_KEY_CHILDCOUNT,
                                      GRL_METADATA_KEY_SITE,
                                      GRL_METADATA_KEY_RATING,
                                      GRL_METADATA_KEY_EXTERNAL_PLAYER,
                                      NULL);
  }
  return keys;
}

static const GList *
grl_youtube_source_slow_keys (GrlSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_URL,
                                      NULL);
  }
  return keys;
}

static void
grl_youtube_source_search (GrlSource *source,
                           GrlSourceSearchSpec *ss)
{
  OperationSpec *os;
  GDataQuery *query;

  GRL_DEBUG ("%s (%u, %d)",
             __FUNCTION__,
             grl_operation_options_get_skip (ss->options),
             grl_operation_options_get_count (ss->options));

  os = operation_spec_new ();
  os->source = source;
  os->cancellable = g_cancellable_new ();
  os->operation_id = ss->operation_id;
  os->keys = ss->keys;
  os->skip = grl_operation_options_get_skip (ss->options);
  os->count = grl_operation_options_get_count (ss->options);
  os->callback = ss->callback;
  os->user_data = ss->user_data;
  os->error_code = GRL_CORE_ERROR_SEARCH_FAILED;

  /* Look for OPERATION_SPEC_REF_RATIONALE for details */
  operation_spec_ref (os);

  grl_operation_set_data (ss->operation_id, g_object_ref (os->cancellable));

  /* Index in GData starts at 1 */
  query = GDATA_QUERY (gdata_youtube_query_new (ss->text));
  gdata_query_set_start_index (query, os->skip + 1);
  gdata_query_set_max_results (query, os->count);

  gdata_youtube_service_query_videos_async (GDATA_YOUTUBE_SERVICE (GRL_YOUTUBE_SOURCE (source)->priv->service),
                                            query,
                                            os->cancellable,
                                            search_progress_cb,
                                            os,
                                            NULL,
                                            (GAsyncReadyCallback) search_cb,
                                            os);

  g_object_unref (query);
}

static void
grl_youtube_source_browse (GrlSource *source,
                           GrlSourceBrowseSpec *bs)
{
  BuildCategorySpec *bcs;
  OperationSpec *os;
  const gchar *container_id;

  GRL_DEBUG ("%s: %s (%u, %d)",
             __FUNCTION__,
             grl_media_get_id (bs->container),
             grl_operation_options_get_skip (bs->options),
             grl_operation_options_get_count (bs->options));

  container_id = grl_media_get_id (bs->container);

  os = operation_spec_new ();
  os->source = bs->source;
  os->operation_id = bs->operation_id;
  os->container_id = container_id;
  os->keys = bs->keys;
  os->flags = grl_operation_options_get_resolution_flags (bs->options);
  os->skip = grl_operation_options_get_skip (bs->options);
  os->count = grl_operation_options_get_count (bs->options);
  os->callback = bs->callback;
  os->user_data = bs->user_data;
  os->error_code = GRL_CORE_ERROR_BROWSE_FAILED;

  switch (classify_media_id (container_id))
    {
    case YOUTUBE_MEDIA_TYPE_ROOT:
      produce_from_directory (root_dir, root_dir_size, os);
      break;
    case YOUTUBE_MEDIA_TYPE_FEEDS:
      produce_from_directory (feeds_dir,
			      root_dir[ROOT_DIR_FEEDS_INDEX].count, os);
      break;
    case YOUTUBE_MEDIA_TYPE_CATEGORIES:
      if (!categories_dir) {
        bcs = g_slice_new0 (BuildCategorySpec);
        bcs->source = bs->source;
        bcs->callback = (GSourceFunc) produce_from_category_cb;
        bcs->user_data = os;
        build_category_directory (bcs);
      } else {
        produce_from_directory (categories_dir,
                                root_dir[ROOT_DIR_CATEGORIES_INDEX].count,
                                os);
      }
      break;
    case YOUTUBE_MEDIA_TYPE_FEED:
      produce_from_feed (os);
      break;
    case YOUTUBE_MEDIA_TYPE_CATEGORY:
      produce_from_category (os);
      break;
    case YOUTUBE_MEDIA_TYPE_VIDEO:
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
grl_youtube_source_resolve (GrlSource *source,
                            GrlSourceResolveSpec *rs)
{
  BuildCategorySpec *bcs;
  YoutubeMediaType media_type;
  const gchar *id;
  GCancellable *cancellable;
  GDataService *service;
  GError *error = NULL;
  GrlMedia *media = NULL;

  GRL_DEBUG (__FUNCTION__);

  id = grl_media_get_id (rs->media);
  media_type = classify_media_id (id);
  service = GRL_YOUTUBE_SOURCE (source)->priv->service;

  switch (media_type) {
  case YOUTUBE_MEDIA_TYPE_ROOT:
    media = produce_container_from_directory (service, rs->media, NULL, 0);
    break;
  case YOUTUBE_MEDIA_TYPE_FEEDS:
    media = produce_container_from_directory (service, rs->media, root_dir, 0);
    break;
  case YOUTUBE_MEDIA_TYPE_CATEGORIES:
    media = produce_container_from_directory (service, rs->media, root_dir, 1);
    break;
  case YOUTUBE_MEDIA_TYPE_FEED:
    {
      gint index = get_feed_type_from_id (id);
      if (index >= 0) {
        media = produce_container_from_directory (service, rs->media, feeds_dir,
                                                  index);
      } else {
        error = g_error_new (GRL_CORE_ERROR,
                             GRL_CORE_ERROR_RESOLVE_FAILED,
                             _("Invalid feed identifier %s"),
                             id);
      }
    }
    break;
  case YOUTUBE_MEDIA_TYPE_CATEGORY:
    {
      if (!categories_dir) {
        bcs = g_slice_new0 (BuildCategorySpec);
        bcs->source = source;
        bcs->callback = (GSourceFunc) produce_container_from_category_cb;
        bcs->user_data = rs;
        build_category_directory (bcs);
      } else {
        gint index = get_category_index_from_id (id);
        if (index >= 0) {
          media = produce_container_from_directory (service, rs->media,
                                                    categories_dir, index);
        } else {
          error = g_error_new (GRL_CORE_ERROR,
                               GRL_CORE_ERROR_RESOLVE_FAILED,
                               _("Invalid category identifier %s"),
                               id);
        }
      }
    }
    break;
  case YOUTUBE_MEDIA_TYPE_VIDEO:
  default:
    cancellable = g_cancellable_new ();
    grl_operation_set_data (rs->operation_id, cancellable);
    gchar *entryid = g_strconcat ("tag:youtube.com,2008:video:", id, NULL);

      gdata_service_query_single_entry_async (service,
                                              NULL,
                                              entryid,
                                              NULL,
                                              GDATA_TYPE_YOUTUBE_VIDEO,
                                              cancellable,
                                              resolve_cb,
                                              rs);

      g_free (entryid);
      break;
  }

  if (error) {
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);
    g_error_free (error);
  } else if (media) {
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
  }
}

static gboolean
grl_youtube_test_media_from_uri (GrlSource *source, const gchar *uri)
{
  gchar *video_id;
  gboolean ok;

  GRL_DEBUG (__FUNCTION__);

  video_id = get_video_id_from_url (uri);
  ok = (video_id != NULL);
  g_free (video_id);
  return ok;
}

static void
grl_youtube_get_media_from_uri (GrlSource *source,
                                GrlSourceMediaFromUriSpec *mfus)
{
  gchar *video_id;
  GError *error;
  GCancellable *cancellable;
  GDataService *service;
  gchar *entry_id;

  GRL_DEBUG (__FUNCTION__);

  video_id = get_video_id_from_url (mfus->uri);
  if (video_id == NULL) {
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_MEDIA_FROM_URI_FAILED,
                         _("Cannot get media from %s"),
                         mfus->uri);
    mfus->callback (source, mfus->operation_id, NULL, mfus->user_data, error);
    g_error_free (error);
    return;
  }

  service = GRL_YOUTUBE_SOURCE (source)->priv->service;

  cancellable = g_cancellable_new ();
  grl_operation_set_data (mfus->operation_id, cancellable);
  entry_id = g_strconcat ("tag:youtube.com,2008:video:", video_id, NULL);

  gdata_service_query_single_entry_async (service,
                                          NULL,
                                          entry_id,
                                          NULL,
                                          GDATA_TYPE_YOUTUBE_VIDEO,
                                          cancellable,
                                          media_from_uri_cb,
                                          mfus);

  g_free (entry_id);
}

static void
grl_youtube_source_cancel (GrlSource *source,
                           guint operation_id)
{
  GCancellable *cancellable = NULL;
  gpointer data;

  GRL_DEBUG (__FUNCTION__);

  data = grl_operation_get_data (operation_id);

  if (data) {
    cancellable = G_CANCELLABLE (data);
  }

  if (cancellable) {
    g_cancellable_cancel (cancellable);
  }
}
