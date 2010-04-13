/*
 * Copyright (C) 2010 Igalia S.L.
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
#include <gio/gio.h>
#include <gdata/gdata.h>
#include <string.h>

#include "grl-youtube.h"

/* --------- Logging  -------- */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "grl-youtube"

/* ----- Root categories ---- */

#define YOUTUBE_ROOT_NAME       "Youtube"

#define YOUTUBE_FEEDS_ID        "standard-feeds"
#define YOUTUBE_FEEDS_NAME      "Standard feeds"

#define YOUTUBE_CATEGORIES_ID   "categories"
#define YOUTUBE_CATEGORIES_NAME "Categories"
#define YOUTUBE_CATEGORIES_URL  "http://gdata.youtube.com/schemas/2007/categories.cat"

/* ----- Feeds categories ---- */

#define YOUTUBE_VIEWED_ID       (YOUTUBE_FEEDS_ID "/most-viewed")
#define YOUTUBE_VIEWED_NAME     "Most viewed"

#define YOUTUBE_RATED_ID        (YOUTUBE_FEEDS_ID "/most-rated")
#define YOUTUBE_RATED_NAME      "Most rated"

#define YOUTUBE_FAVS_ID         (YOUTUBE_FEEDS_ID "/top-favs")
#define YOUTUBE_FAVS_NAME       "Top favorites"

#define YOUTUBE_RECENT_ID       (YOUTUBE_FEEDS_ID "/most-recent")
#define YOUTUBE_RECENT_NAME     "Most recent"

#define YOUTUBE_DISCUSSED_ID    (YOUTUBE_FEEDS_ID "/most-discussed")
#define YOUTUBE_DISCUSSED_NAME  "Most discussed"

#define YOUTUBE_FEATURED_ID     (YOUTUBE_FEEDS_ID "/featured")
#define YOUTUBE_FEATURED_NAME   "Featured"

#define YOUTUBE_MOBILE_ID       (YOUTUBE_FEEDS_ID "/mobile")
#define YOUTUBE_MOBILE_NAME     "Watch on mobile"

/* --- Other --- */

#define YOUTUBE_MAX_CHUNK   50

#define YOUTUBE_VIDEO_INFO_URL  "http://www.youtube.com/get_video_info?video_id=%s"
#define YOUTUBE_VIDEO_URL       "http://www.youtube.com/get_video?video_id=%s&t=%s"

#define YOUTUBE_VIDEO_MIME  "application/x-shockwave-flash"
#define YOUTUBE_SITE_URL    "www.youtube.com"

/* --- Plugin information --- */

#define PLUGIN_ID   "grl-youtube"
#define PLUGIN_NAME "Youtube"
#define PLUGIN_DESC "A plugin for browsing and searching Youtube videos"

#define SOURCE_ID   "grl-youtube"
#define SOURCE_NAME "Youtube"
#define SOURCE_DESC "A source for browsing and searching Youtube videos"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

/* --- Data types --- */

typedef void (*AsyncReadCbFunc) (gchar *data, gpointer user_data);

typedef struct {
  GrlMediaSource *source;
  guint operation_id;
  const gchar *container_id;
  GList *keys;
  GrlMetadataResolutionFlags flags;
  guint skip;
  guint count;
  GrlMediaSourceResultCb callback;
  gpointer user_data;
  const GDataYouTubeService *service;
  guint error_code;
  GDataQuery *query;
} OperationSpec;

typedef enum {
  YOUTUBE_MEDIA_TYPE_ROOT,
  YOUTUBE_MEDIA_TYPE_FEEDS,
  YOUTUBE_MEDIA_TYPE_CATEGORIES,
  YOUTUBE_MEDIA_TYPE_FEED,
  YOUTUBE_MEDIA_TYPE_CATEGORY,
  YOUTUBE_MEDIA_TYPE_VIDEO,
} YoutubeMediaType;

#define YOUTUBE_DEVELOPER_KEY "AI39si4EfscPllSfUy1IwexMf__kntTL_G5dfSr2iUEVN45RHGq92Aq0lX25OlnOkG6KTN-4soVAkAf67fWYXuHfVADZYr7S1A"
#define YOUTUBE_CLIENT_ID "test-client"


static GrlYoutubeSource *grl_youtube_source_new (void);

gboolean grl_youtube_plugin_init (GrlPluginRegistry *registry,
                                  const GrlPluginInfo *plugin,
                                  GList *configs);

static const GList *grl_youtube_source_supported_keys (GrlMetadataSource *source);

static const GList *grl_youtube_source_slow_keys (GrlMetadataSource *source);

static void grl_youtube_source_search (GrlMediaSource *source,
                                       GrlMediaSourceSearchSpec *ss);

static void grl_youtube_source_metadata (GrlMediaSource *source,
                                         GrlMediaSourceMetadataSpec *ms);

/* ==================== Global Data  ================= */


/* =================== Youtube Plugin  =============== */

gboolean
grl_youtube_plugin_init (GrlPluginRegistry *registry,
                         const GrlPluginInfo *plugin,
                         GList *config)
{
  g_debug ("youtube_plugin_init\n");

  /* libgdata needs this */
  if (!g_thread_supported()) {
    g_thread_init (NULL);
  }

  GrlYoutubeSource *source = grl_youtube_source_new ();
  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source));
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_youtube_plugin_init,
                     NULL,
                     PLUGIN_ID,
                     PLUGIN_NAME,
                     PLUGIN_DESC,
                     PACKAGE_VERSION,
                     AUTHOR,
                     LICENSE,
                     SITE);

/* ================== Youtube GObject ================ */

static GrlYoutubeSource *
grl_youtube_source_new (void)
{
  g_debug ("grl_youtube_source_new");

  GrlYoutubeSource *source;

  GDataYouTubeService *service =
    gdata_youtube_service_new (YOUTUBE_DEVELOPER_KEY, YOUTUBE_CLIENT_ID);
  
  if (!service) {
    g_warning ("Failed to connect to Youtube");
    return NULL;
  }

  source = GRL_YOUTUBE_SOURCE (g_object_new (GRL_YOUTUBE_SOURCE_TYPE,
					     "source-id", SOURCE_ID,
					     "source-name", SOURCE_NAME,
					     "source-desc", SOURCE_DESC,
					     "auto-split-threshold",
					     YOUTUBE_MAX_CHUNK,
					     NULL));
  source->service = service;

  return source;
}

static void
grl_youtube_source_class_init (GrlYoutubeSourceClass * klass)
{
  GrlMediaSourceClass *source_class = GRL_MEDIA_SOURCE_CLASS (klass);
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);
  source_class->search = grl_youtube_source_search;
  source_class->metadata = grl_youtube_source_metadata;
  metadata_class->supported_keys = grl_youtube_source_supported_keys;
  metadata_class->slow_keys = grl_youtube_source_slow_keys;
}

static void
grl_youtube_source_init (GrlYoutubeSource *source)
{
}

G_DEFINE_TYPE (GrlYoutubeSource, grl_youtube_source, GRL_TYPE_MEDIA_SOURCE);

/* ======================= Utilities ==================== */

static void
free_operation_spec (OperationSpec *os)
{
  if (os->query) {
    g_object_unref (os->query);
  }
  g_free (os);
}

static gchar *
read_url (const gchar *url)
{
  GVfs *vfs;
  GFile *uri;
  GError *vfs_error = NULL;
  gchar *content = NULL;

  vfs = g_vfs_get_default ();

  g_debug ("Opening '%s'", url);
  uri = g_vfs_get_file_for_uri (vfs, url);
  g_file_load_contents (uri, NULL, &content, NULL, NULL, &vfs_error);
  g_object_unref (uri);
  if (vfs_error) {
    g_warning ("Failed reading '%s': %s", url, vfs_error->message);
    return NULL;
  } else {
    return content;
  }
}

static gchar *
get_video_url (const gchar *id)
{
  gchar *token_start;
  gchar *token_end;
  gchar *token;
  gchar *video_info_url;
  gchar *data;
  gchar *url;

  video_info_url = g_strdup_printf (YOUTUBE_VIDEO_INFO_URL, id);
  data = read_url (video_info_url);
  if (!data) {
    g_free (video_info_url);
    return NULL;
  }

  token_start = g_strrstr (data, "&token=");
  if (!token_start) {
    g_free (video_info_url);
    return NULL;
  }
  token_start += 7;
  token_end = strstr (token_start, "&");
  token = g_strndup (token_start, token_end - token_start);

  url = g_strdup_printf (YOUTUBE_VIDEO_URL, id, token);

  g_free (video_info_url);
  g_free (token);

  return url;
}

static GrlMedia *
build_media_from_entry (GrlMedia *content,
			GDataEntry *entry,
			const GList *keys)
{
  GDataYouTubeVideo *video;
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
    grl_media_set_id (media, gdata_youtube_video_get_video_id (video));
  }

  iter = (GList *) keys;
  while (iter) {
    GrlKeyID key_id = POINTER_TO_GRLKEYID (iter->data);
    switch (key_id) {
    case GRL_METADATA_KEY_TITLE:
      grl_media_set_title (media, gdata_entry_get_title (entry));
      break;
    case GRL_METADATA_KEY_DESCRIPTION:
      grl_media_set_description (media, gdata_entry_get_summary (entry));
      break;
    case GRL_METADATA_KEY_THUMBNAIL:
      {
	GList *thumb_list;
	thumb_list = gdata_youtube_video_get_thumbnails (video);
	if (thumb_list) {
	  GDataMediaThumbnail *thumbnail;
	  thumbnail = GDATA_MEDIA_THUMBNAIL (thumb_list->data);
	  grl_media_set_thumbnail (media,
				   gdata_media_thumbnail_get_uri (thumbnail));
	}
      }
      break;
    case GRL_METADATA_KEY_DATE:
      {
	GTimeVal date;
	gchar *date_str;
	gdata_entry_get_published (entry, &date);
	date_str = g_time_val_to_iso8601 (&date);
	grl_media_set_date (media, date_str);
	g_free (date_str);
      }
      break;
    case GRL_METADATA_KEY_DURATION:
      grl_media_set_duration (media, gdata_youtube_video_get_duration (video));
      break;
    case GRL_METADATA_KEY_MIME:
      grl_media_set_mime (media, YOUTUBE_VIDEO_MIME);
      break;
    case GRL_METADATA_KEY_SITE:
      grl_media_set_site (media, YOUTUBE_SITE_URL);
      break;
    case GRL_METADATA_KEY_RATING:
      {
	gdouble average;
	gdata_youtube_video_get_rating (video, NULL, NULL, NULL, &average);
	grl_media_set_rating (media, average, 5.00);
      }
      break;
    case GRL_METADATA_KEY_URL:
      {
	gchar *url = get_video_url (gdata_youtube_video_get_video_id (video));
	if (url) {
	  grl_media_set_url (media, url);
	  g_free (url);
	} else {
	  GDataYouTubeContent *youtube_content;
	  youtube_content =
	    gdata_youtube_video_look_up_content (video,
						 "application/x-shockwave-flash");
	  if (youtube_content != NULL) {
	    GDataMediaContent *content = GDATA_MEDIA_CONTENT (youtube_content);
	    grl_media_set_url (media,
			       gdata_media_content_get_uri (content));
	  }
	}
      }
      break;
    default:
      break;
    }
    iter = g_list_next (iter);
  }

  return media;
}

static void
process_feed (GDataFeed *feed, OperationSpec *os)
{
  GList *entries;

  /* Send results to client */
  entries = gdata_feed_get_entries (feed);
  while (entries) {
    GrlMedia *media =
      build_media_from_entry (NULL, GDATA_ENTRY (entries->data), os->keys);
    os->callback (os->source,
		  os->operation_id,
		  media,
		  --(os->count),
		  os->user_data,
		  NULL);
    entries = g_list_next (entries);
  }

  /* Send last result if not yet sent */
  if (os->count > 0) {
    os->callback (os->source,
		  os->operation_id,
		  NULL,
		  0,
		  os->user_data,
		  NULL);
  }

  free_operation_spec (os);
}

static void
process_metadata (GDataYouTubeVideo *video, GrlMediaSourceMetadataSpec *ms)
{
  build_media_from_entry (ms->media, GDATA_ENTRY (video), ms->keys);
  ms->callback (ms->source, ms->media, ms->user_data, NULL);
}

static void
metadata_cb (GObject *object,
	     GAsyncResult *result,
	     GrlMediaSourceMetadataSpec *ms)
{
  g_debug ("metadata_cb");

  GError *error = NULL;
  GrlYoutubeSource *source = GRL_YOUTUBE_SOURCE (ms->source);

  GDataYouTubeVideo *video =
    gdata_youtube_service_query_single_video_finish (GDATA_YOUTUBE_SERVICE (source->service),
						     result,
						     &error);
  if (error) {
    error->code = GRL_ERROR_METADATA_FAILED;
    ms->callback (ms->source, ms->media, ms->user_data, error);
    g_error_free (error);
  } else {
    process_metadata (video, ms);
  }

  if (video) {
    g_object_unref (video);
  }
}

static void
search_cb (GObject *object, GAsyncResult *result, OperationSpec *os)
{
  g_debug ("search_cb");

  GDataFeed *feed;
  GError *error = NULL;
  GrlYoutubeSource *source = GRL_YOUTUBE_SOURCE (os->source);
  
  feed = gdata_service_query_finish (GDATA_SERVICE (source->service),
				     result, &error);
  if (error) {
    error->code = os->error_code;
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
    if (feed) {
      g_object_unref (feed);
    }
  } else {
    g_debug ("Feed info: %s - %u/%u/%u",
	     gdata_feed_get_title (feed),
	     gdata_feed_get_start_index (feed),
	     gdata_feed_get_items_per_page (feed),
	     gdata_feed_get_total_results (feed));

    process_feed (feed, os);
  }
}

/* ================== API Implementation ================ */

static const GList *
grl_youtube_source_supported_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_URL,
                                      GRL_METADATA_KEY_DESCRIPTION,
                                      GRL_METADATA_KEY_DURATION,
                                      GRL_METADATA_KEY_DATE,
                                      GRL_METADATA_KEY_THUMBNAIL,
                                      GRL_METADATA_KEY_MIME,
                                      GRL_METADATA_KEY_CHILDCOUNT,
                                      GRL_METADATA_KEY_SITE,
                                      GRL_METADATA_KEY_RATING,
                                      NULL);
  }
  return keys;
}

static const GList *
grl_youtube_source_slow_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    /* childcount may or may not be slow depending on the category,
       so we handle it as a non-slow key and then we decide if we
       resolve or not depending on the category and the flags set */
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_URL,
                                      NULL);
  }
  return keys;
}

static void
grl_youtube_source_search (GrlMediaSource *source,
                           GrlMediaSourceSearchSpec *ss)
{
  OperationSpec *os;

  g_debug ("grl_youtube_source_search %u", ss->count);

  os = g_new0 (OperationSpec, 1);
  os->source = source;
  os->operation_id = ss->search_id;
  os->keys = ss->keys;
  os->skip = ss->skip;
  os->count = ss->count;
  os->callback = ss->callback;
  os->user_data = ss->user_data;
  os->error_code = GRL_ERROR_SEARCH_FAILED;
  os->query = gdata_query_new_with_limits (ss->text, ss->skip, ss->count);

  gdata_youtube_service_query_videos_async (GRL_YOUTUBE_SOURCE (source)->service,
					    os->query,
					    NULL,
					    NULL,
					    NULL,
					    (GAsyncReadyCallback) search_cb,
					    os);
}

static void
grl_youtube_source_metadata (GrlMediaSource *source,
                             GrlMediaSourceMetadataSpec *ms)
{
  g_debug ("grl_youtube_source_metadata");
  
  gdata_youtube_service_query_single_video_async (GRL_YOUTUBE_SOURCE (source)->service,
						  NULL,
						  grl_media_get_id (ms->media),
						  NULL,
						  (GAsyncReadyCallback) metadata_cb,
						  ms);
}
