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

#define ROOT_DIR_FEEDS_INDEX      0
#define ROOT_DIR_CATEGORIES_INDEX 1

#define YOUTUBE_FEEDS_ID        "standard-feeds"
#define YOUTUBE_FEEDS_NAME      "Standard feeds"

#define YOUTUBE_CATEGORIES_ID   "categories"
#define YOUTUBE_CATEGORIES_NAME "Categories"
#define YOUTUBE_CATEGORIES_URL  "http://gdata.youtube.com/schemas/2007/categories.cat"

/* ----- Feeds categories ---- */

#define YOUTUBE_TOP_RATED_ID         (YOUTUBE_FEEDS_ID "/0")
#define YOUTUBE_TOP_RATED_NAME       "Top Rated"

#define YOUTUBE_TOP_FAVS_ID          (YOUTUBE_FEEDS_ID "/1")
#define YOUTUBE_TOP_FAVS_NAME        "Top Favorites"

#define YOUTUBE_MOST_VIEWED_ID       (YOUTUBE_FEEDS_ID "/2")
#define YOUTUBE_MOST_VIEWED_NAME     "Most Viewed"

#define YOUTUBE_MOST_POPULAR_ID      (YOUTUBE_FEEDS_ID "/3")
#define YOUTUBE_MOST_POPULAR_NAME    "Most Popular"

#define YOUTUBE_MOST_RECENT_ID       (YOUTUBE_FEEDS_ID "/4")
#define YOUTUBE_MOST_RECENT_NAME     "Most Recent"

#define YOUTUBE_MOST_DISCUSSED_ID    (YOUTUBE_FEEDS_ID "/5")
#define YOUTUBE_MOST_DISCUSSED_NAME  "Most Discussed"

#define YOUTUBE_MOST_LINKED_ID       (YOUTUBE_FEEDS_ID "/6")
#define YOUTUBE_MOST_LINKED_NAME     "Most Linked"

#define YOUTUBE_MOST_RESPONDED_ID    (YOUTUBE_FEEDS_ID "/7")
#define YOUTUBE_MOST_RESPONDED_NAME  "Most Responded"

#define YOUTUBE_FEATURED_ID          (YOUTUBE_FEEDS_ID "/8")
#define YOUTUBE_FEATURED_NAME        "Recently Featured"

#define YOUTUBE_MOBILE_ID            (YOUTUBE_FEEDS_ID "/9")
#define YOUTUBE_MOBILE_NAME          "Watch On Mobile"

/* --- Other --- */

#define YOUTUBE_MAX_CHUNK       50

#define YOUTUBE_VIDEO_INFO_URL  "http://www.youtube.com/get_video_info?video_id=%s"
#define YOUTUBE_VIDEO_URL       "http://www.youtube.com/get_video?video_id=%s&t=%s"
#define YOUTUBE_CATEGORY_URL    "http://gdata.youtube.com/feeds/api/videos/-/%s?&start-index=%s&max-results=%s"

#define YOUTUBE_VIDEO_MIME      "application/x-shockwave-flash"
#define YOUTUBE_SITE_URL        "www.youtube.com"


#define GRL_CONFIG_KEY_YOUTUBE_CLIENT_ID 100

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
  gchar *id;
  gchar *name;
  guint count;
} CategoryInfo;

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
  guint error_code;
  CategoryInfo *category_info;
} OperationSpec;

typedef struct {
  GDataYouTubeService *service;
  CategoryInfo *category_info;
} CategoryCountCb;

typedef struct {
  AsyncReadCbFunc callback;
  gchar *url;
  gpointer user_data;
} AsyncReadCb;

typedef enum {
  YOUTUBE_MEDIA_TYPE_ROOT,
  YOUTUBE_MEDIA_TYPE_FEEDS,
  YOUTUBE_MEDIA_TYPE_CATEGORIES,
  YOUTUBE_MEDIA_TYPE_FEED,
  YOUTUBE_MEDIA_TYPE_CATEGORY,
  YOUTUBE_MEDIA_TYPE_VIDEO,
} YoutubeMediaType;

#define YOUTUBE_CLIENT_ID "grilo"

static GrlYoutubeSource *grl_youtube_source_new (const gchar *api_key,
						 const gchar *client_id);

gboolean grl_youtube_plugin_init (GrlPluginRegistry *registry,
                                  const GrlPluginInfo *plugin,
                                  GList *configs);

static const GList *grl_youtube_source_supported_keys (GrlMetadataSource *source);

static const GList *grl_youtube_source_slow_keys (GrlMetadataSource *source);

static void grl_youtube_source_search (GrlMediaSource *source,
                                       GrlMediaSourceSearchSpec *ss);

static void grl_youtube_source_browse (GrlMediaSource *source,
                                       GrlMediaSourceBrowseSpec *bs);

static void grl_youtube_source_metadata (GrlMediaSource *source,
                                         GrlMediaSourceMetadataSpec *ms);

static void build_directories (GDataYouTubeService *service);
static void compute_feed_counts (GDataYouTubeService *service);
static void compute_category_counts (GDataYouTubeService *service);

/* ==================== Global Data  ================= */

guint root_dir_size = 2;
CategoryInfo root_dir[] = {
  {YOUTUBE_FEEDS_ID,      YOUTUBE_FEEDS_NAME,      10},
  {YOUTUBE_CATEGORIES_ID, YOUTUBE_CATEGORIES_NAME,  0},
  {NULL, NULL, 0}
};

CategoryInfo feeds_dir[] = {
  {YOUTUBE_TOP_RATED_ID,      YOUTUBE_TOP_RATED_NAME,       0},
  {YOUTUBE_TOP_FAVS_ID,       YOUTUBE_TOP_FAVS_NAME,        0},
  {YOUTUBE_MOST_VIEWED_ID,    YOUTUBE_MOST_VIEWED_NAME,     0},
  {YOUTUBE_MOST_POPULAR_ID,   YOUTUBE_MOST_POPULAR_NAME,    0},
  {YOUTUBE_MOST_RECENT_ID,    YOUTUBE_MOST_RECENT_NAME,     0},
  {YOUTUBE_MOST_DISCUSSED_ID, YOUTUBE_MOST_DISCUSSED_NAME,  0},
  {YOUTUBE_MOST_LINKED_ID,    YOUTUBE_MOST_LINKED_NAME,     0},
  {YOUTUBE_MOST_RESPONDED_ID, YOUTUBE_MOST_RESPONDED_NAME,  0},
  {YOUTUBE_FEATURED_ID,       YOUTUBE_FEATURED_NAME,        0},
  {YOUTUBE_MOBILE_ID,         YOUTUBE_MOBILE_NAME,          0},
  {NULL, NULL, 0}
};

CategoryInfo *categories_dir = NULL;

/* =================== Youtube Plugin  =============== */

gboolean
grl_youtube_plugin_init (GrlPluginRegistry *registry,
                         const GrlPluginInfo *plugin,
                         GList *configs)
{
  const gchar *api_key;
  const gchar *client_id;
  const GrlConfig *config;
  gint config_count;

  g_debug ("youtube_plugin_init");

  if (!configs) {
    g_warning ("Configuration not provided! Cannot configure plugin.");
    return FALSE;
  }

  config_count = g_list_length (configs);
  if (config_count > 1) {
    g_warning ("Provided %d configs, but will only use one", config_count);
  }

  config = GRL_CONFIG (configs->data);
  api_key = grl_config_get_api_key (config);
  if (!api_key) {
    g_warning ("Missing API Key, cannot configure Youtube plugin");
    return FALSE;
  }
  client_id = grl_data_get_string (GRL_DATA (config),
				   GRL_CONFIG_KEY_YOUTUBE_CLIENT_ID);
  if (!client_id) {
    g_warning ("No client id set, using default: %s", YOUTUBE_CLIENT_ID);
    client_id = YOUTUBE_CLIENT_ID;
  }

  /* libgdata needs this */
  if (!g_thread_supported()) {
    g_thread_init (NULL);
  }

  GrlYoutubeSource *source = grl_youtube_source_new (api_key, client_id);
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
grl_youtube_source_new (const gchar *api_key, const gchar *client_id)
{
  g_debug ("grl_youtube_source_new");

  GrlYoutubeSource *source;

  GDataYouTubeService *service =
    gdata_youtube_service_new (api_key, client_id);
  
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

  /* Obtain categories (and chilcounts) only once */
  if (!categories_dir) {
    build_directories (service);
  }

  return source;
}

static void
grl_youtube_source_class_init (GrlYoutubeSourceClass * klass)
{
  GrlMediaSourceClass *source_class = GRL_MEDIA_SOURCE_CLASS (klass);
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);
  source_class->search = grl_youtube_source_search;
  source_class->browse = grl_youtube_source_browse;
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

static void
read_done_cb (GObject *source_object,
              GAsyncResult *res,
              gpointer user_data)
{
  AsyncReadCb *arc = (AsyncReadCb *) user_data;
  GError *vfs_error = NULL;
  gchar *content = NULL;

  g_file_load_contents_finish (G_FILE (source_object),
                               res,
                               &content,
                               NULL,
                               NULL,
                               &vfs_error);
  g_object_unref (source_object);
  if (vfs_error) {
    g_warning ("Failed to open '%s': %s", arc->url, vfs_error->message);
  } else {
    arc->callback (content, arc->user_data);
  }
  g_free (arc->url);
  g_free (arc);
}

static void
read_url_async (const gchar *url,
                AsyncReadCbFunc callback,
                gpointer user_data)
{
  GVfs *vfs;
  GFile *uri;
  AsyncReadCb *arc;

  vfs = g_vfs_get_default ();

  g_debug ("Opening async '%s'", url);

  arc = g_new0 (AsyncReadCb, 1);
  arc->url = g_strdup (url);
  arc->callback = callback;
  arc->user_data = user_data;
  uri = g_vfs_get_file_for_uri (vfs, url);
  g_file_load_contents_async (uri, NULL, read_done_cb, arc);
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
parse_categories (xmlDocPtr doc, xmlNodePtr node, GDataYouTubeService *service)
{
  g_debug ("parse_categories");

  guint total = 0;
  GList *all = NULL, *iter;
  CategoryInfo *cat_info;
  gchar *id;
  guint index = 0;

  while (node) {
    cat_info = g_new (CategoryInfo, 1);
    id = (gchar *) xmlGetProp (node, (xmlChar *) "term");
    cat_info->id = g_strconcat (YOUTUBE_CATEGORIES_ID, "/", id, NULL);
    cat_info->name = (gchar *) xmlGetProp (node, (xmlChar *) "label");
    all = g_list_prepend (all, cat_info);
    g_free (id);
    node = node->next;
    total++;
    g_debug ("  Found category: '%d - %s'", index++, cat_info->name);
  }

  if (all) {
    root_dir[ROOT_DIR_CATEGORIES_INDEX].count = total;
    categories_dir = g_new0 (CategoryInfo, total + 1);
    iter = all;
    do {
      cat_info = (CategoryInfo *) iter->data;
      categories_dir[total - 1].id = cat_info->id ;
      categories_dir[total - 1].name = cat_info->name;
      categories_dir[total - 1].count = 0;
      total--;
      g_free (cat_info);
      iter = g_list_next (iter);
    } while (iter);
    g_list_free (all);

    compute_category_counts (service);
  }
}

static void
build_categories_directory_read_cb (gchar *xmldata, gpointer user_data)
{
  xmlDocPtr doc;
  xmlNodePtr node;

  if (!xmldata) {
    g_critical ("Failed to build category directory (1)");
    return;
  }

  doc = xmlRecoverDoc ((xmlChar *) xmldata);
  if (!doc) {
    g_critical ("Failed to build category directory (2)");
    goto free_resources;
  }

  node = xmlDocGetRootElement (doc);
  if (!node) {
    g_critical ("Failed to build category directory (3)");
    goto free_resources;
  }

  if (xmlStrcmp (node->name, (const xmlChar *) "categories")) {
    g_critical ("Failed to build category directory (4)");
    goto free_resources;
  }

  node = node->xmlChildrenNode;
  if (!node) {
    g_critical ("Failed to build category directory (5)");
    goto free_resources;
  }

  parse_categories (doc, node, GDATA_YOUTUBE_SERVICE (user_data));

 free_resources:
  xmlFreeDoc (doc);
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
  for (gint i=0; i<root_dir[ROOT_DIR_CATEGORIES_INDEX].count; i++) {
    if (!strcmp (categories_dir[i].id, category_id)) {
      return i;
    }
  }
  return -1;
}

static void
item_count_cb (GObject *object, GAsyncResult *result, CategoryCountCb *cc)
{
  g_debug ("item_count_cb");

  GDataFeed *feed;
  GError *error = NULL;
  
  feed = gdata_service_query_finish (GDATA_SERVICE (cc->service),
				     result, &error);
  if (error) {
    g_warning ("Failed to compute count for category '%s': %s",
	       cc->category_info->id, error->message);
    g_error_free (error);
  } else if (feed) {
    cc->category_info->count = gdata_feed_get_total_results (feed);
    g_debug ("Category '%s' - childcount: '%u'", 
	     cc->category_info->id, cc->category_info->count);
  }

  if (feed) {
    g_object_unref (feed);
  }
  g_free (cc);
}

static void
compute_category_counts (GDataYouTubeService *service)
{
  g_debug ("compute_category_counts");

  for (gint i=0; i<root_dir[ROOT_DIR_CATEGORIES_INDEX].count; i++) {
    g_debug ("Computing chilcount for category '%s'", categories_dir[i].id);
    GDataQuery *query = gdata_query_new_with_limits (NULL, 0, 1);
    const gchar *category_term =
      get_category_term_from_id (categories_dir[i].id);
    gdata_query_set_categories (query, category_term);
    CategoryCountCb *cc = g_new (CategoryCountCb, 1);
    cc->service = service;
    cc->category_info = &categories_dir[i];
    gdata_youtube_service_query_videos_async (service,
					      query,
					      NULL, NULL, NULL,
					      (GAsyncReadyCallback) item_count_cb,
					      cc);
    g_object_unref (query);
  }
}

static void
compute_feed_counts (GDataYouTubeService *service)
{
  g_debug ("compute_feed_counts");

  for (gint i=0; i<root_dir[ROOT_DIR_FEEDS_INDEX].count; i++) {
    g_debug ("Computing chilcount for feed '%s'", feeds_dir[i].id);
    gint feed_type = get_feed_type_from_id (feeds_dir[i].id);
    GDataQuery *query = gdata_query_new_with_limits (NULL, 0, 1);
    CategoryCountCb *cc = g_new (CategoryCountCb, 1);
    cc->service = service;
    cc->category_info = &feeds_dir[i];
    gdata_youtube_service_query_standard_feed_async (service,
						     feed_type,
						     query,
						     NULL, NULL, NULL,
						     (GAsyncReadyCallback) item_count_cb,
						     cc);
    g_object_unref (query);
  }
}

static void
process_entry (GDataEntry *entry, guint count, OperationSpec *os)
{
  GrlMedia *media = build_media_from_entry (NULL, entry, os->keys);
  os->callback (os->source,
		os->operation_id,
		media,
		count,
		os->user_data,
		NULL);
}

static void
build_directories (GDataYouTubeService *service)
{
  g_debug ("build_drectories");

  /* Parse category list from Youtube and compute category counts */
  read_url_async (YOUTUBE_CATEGORIES_URL,
                  build_categories_directory_read_cb,
                  service);

  /* Compute feed counts */
  compute_feed_counts (service);
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
	     gpointer user_data)
{
  g_debug ("metadata_cb");

  GError *error = NULL;
  GrlYoutubeSource *source;
  GDataYouTubeVideo *video;
  GDataYouTubeService *service;
  GrlMediaSourceMetadataSpec *ms = (GrlMediaSourceMetadataSpec *) user_data;

  source = GRL_YOUTUBE_SOURCE (ms->source);
  service = GDATA_YOUTUBE_SERVICE (source->service);

  video = gdata_youtube_service_query_single_video_finish (service,
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
search_progress_cb (GDataEntry *entry,
		    guint index,
		    guint count,
		    gpointer user_data)
{
  OperationSpec *os = (OperationSpec *) user_data;
  if (index < count) {
    os->count = count - index - 1;
    process_entry (entry, os->count, os);
  } else {
    g_warning ("Invalid index/count received grom libgdata, ignoring result");
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
  if (!error && feed) {
    g_debug ("Feed info: %s - %u/%u/%u",
	     gdata_feed_get_title (feed),
	     gdata_feed_get_start_index (feed),
	     gdata_feed_get_items_per_page (feed),
	     gdata_feed_get_total_results (feed));
    
    /* If we are browsing a category, update the count for it */
    if (os->category_info) {
      os->category_info->count = gdata_feed_get_total_results (feed);
    }

    /* Should not be necessary, but just in case... */
    if (os->count > 0) {
      os->callback (os->source, os->operation_id, NULL, 0, os->user_data, NULL);
    }
  } else {
    if (!error) {
      error = g_error_new (GRL_ERROR,
			   os->error_code,
			   "Failed to obtain feed from Youtube");
    } else {
      error->code = os->error_code;
    }
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
    if (feed) g_object_unref (feed);
    free_operation_spec (os);
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
set_category_childcount (GDataYouTubeService *service,
			 GrlMediaBox *content,
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
    grl_media_box_set_childcount (content, childcount);
  }
}

static GrlMedia *
produce_container_from_directory (GDataYouTubeService *service,
				  GrlMedia *media,
				  CategoryInfo *dir,
				  guint index)
{
  GrlMedia *content;

  if (!media) {
    /* Create mode */
    content = grl_media_box_new ();
  } else {
    /* Update mode */
    content = media;
  }

  if (!dir) {
    grl_media_set_id (content, NULL);
    grl_media_set_title (content, YOUTUBE_ROOT_NAME);
  } else {
    grl_media_set_id (content, dir[index].id);
    grl_media_set_title (content, dir[index].name);
  }
  grl_media_set_site (content, YOUTUBE_SITE_URL);
  set_category_childcount (service, GRL_MEDIA_BOX (content), dir, index);

  return content;
}

static void
produce_from_directory (CategoryInfo *dir, guint dir_size, OperationSpec *os)
{
  g_debug ("produce_from_directory");

  guint index, remaining;

  if (os->skip >= dir_size) {
    /* No results */
    os->callback (os->source,
		  os->operation_id,
		  NULL,
		  0,
		  os->user_data,
		  NULL);
    free_operation_spec (os);
  } else {
    index = os->skip;
    remaining = MIN (dir_size - os->skip, os->count);

    do {
      GDataYouTubeService *service = GRL_YOUTUBE_SOURCE (os->source)->service;

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
	free_operation_spec (os);
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
  GDataYouTubeService *service;

  feed_type = get_feed_type_from_id (os->container_id);

  if (feed_type < 0) {
    error = g_error_new (GRL_ERROR,
			 GRL_ERROR_BROWSE_FAILED,
			 "Invalid feed id: %s", os->container_id);
    os->callback (os->source,
		  os->operation_id,
		  NULL,
		  0,
		  os->user_data,
		  error);
    g_error_free (error);
    return;
  }

  service = GDATA_YOUTUBE_SERVICE (GRL_YOUTUBE_SOURCE (os->source)->service);
  query = gdata_query_new_with_limits (NULL , os->skip, os->count);
  os->category_info = &feeds_dir[feed_type];
  gdata_youtube_service_query_standard_feed_async (service,
                                                   feed_type,
                                                   query,
                                                   NULL,
                                                   search_progress_cb,
                                                   os,
                                                   (GAsyncReadyCallback) search_cb,
                                                   os);
  g_object_unref (query);
}

static void
produce_from_category (OperationSpec *os)
{
  GError *error = NULL;
  GDataQuery *query;
  GDataYouTubeService *service;
  const gchar *category_term;
  gint category_index;

  category_term = get_category_term_from_id (os->container_id);
  category_index = get_category_index_from_id (os->container_id);

  if (!category_term) {
    error = g_error_new (GRL_ERROR,
			 GRL_ERROR_BROWSE_FAILED,
			 "Invalid category id: %s", os->container_id);
    os->callback (os->source,
		  os->operation_id,
		  NULL,
		  0,
		  os->user_data,
		  error);
    g_error_free (error);
    return;
  }

  service = GDATA_YOUTUBE_SERVICE (GRL_YOUTUBE_SOURCE (os->source)->service);
  query = gdata_query_new_with_limits (NULL , os->skip, os->count);
  os->category_info = &categories_dir[category_index];
  gdata_query_set_categories (query, category_term);
  gdata_youtube_service_query_videos_async (service,
					    query,
					    NULL,
					    search_progress_cb,
					    os,
					    (GAsyncReadyCallback) search_cb,
					    os);
  g_object_unref (query);
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
  GDataQuery *query;

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

  query = gdata_query_new_with_limits (ss->text, ss->skip, ss->count);
  gdata_youtube_service_query_videos_async (GRL_YOUTUBE_SOURCE (source)->service,
					    query,
					    NULL,
					    search_progress_cb,
					    os,
					    (GAsyncReadyCallback) search_cb,
					    os);
  g_object_unref (query);
}

static void
grl_youtube_source_browse (GrlMediaSource *source,
                           GrlMediaSourceBrowseSpec *bs)
{
  OperationSpec *os;
  const gchar *container_id;

  g_debug ("grl_youtube_source_browse: %s", grl_media_get_id (bs->container));

  container_id = grl_media_get_id (bs->container);

  os = g_new0 (OperationSpec, 1);
  os->source = bs->source;
  os->operation_id = bs->browse_id;
  os->container_id = container_id;
  os->keys = bs->keys;
  os->flags = bs->flags;
  os->skip = bs->skip;
  os->count = bs->count;
  os->callback = bs->callback;
  os->user_data = bs->user_data;
  os->error_code = GRL_ERROR_BROWSE_FAILED;

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
      produce_from_directory (categories_dir,
			      root_dir[ROOT_DIR_CATEGORIES_INDEX].count, os);
      break;
    case YOUTUBE_MEDIA_TYPE_FEED:
      produce_from_feed (os);
      break;
    case YOUTUBE_MEDIA_TYPE_CATEGORY:
      produce_from_category (os);
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
grl_youtube_source_metadata (GrlMediaSource *source,
                             GrlMediaSourceMetadataSpec *ms)
{
  YoutubeMediaType media_type;
  const gchar *id;
  GDataYouTubeService *service;
  GError *error = NULL;
  GrlMedia *media = NULL;

  g_debug ("grl_youtube_source_metadata");

  id = grl_media_get_id (ms->media);
  media_type = classify_media_id (id);
  service = GRL_YOUTUBE_SOURCE (source)->service;
  
  switch (media_type) {
  case YOUTUBE_MEDIA_TYPE_ROOT:
    media = produce_container_from_directory (service, ms->media, NULL, 0);
    break;
  case YOUTUBE_MEDIA_TYPE_FEEDS:
    media = produce_container_from_directory (service, ms->media, root_dir, 0);
    break;
  case YOUTUBE_MEDIA_TYPE_CATEGORIES:
    media = produce_container_from_directory (service, ms->media, root_dir, 1);
    break;
  case YOUTUBE_MEDIA_TYPE_FEED:
    {
      gint index = get_feed_type_from_id (id);
      if (index >= 0) {
	media = produce_container_from_directory (service, ms->media, feeds_dir,
						  index);
      } else {
	error = g_error_new (GRL_ERROR,
			     GRL_ERROR_METADATA_FAILED,
			     "Invalid feed id");
      }
    }
    break;
  case YOUTUBE_MEDIA_TYPE_CATEGORY:
    {
      gint index = get_category_index_from_id (id);
      if (index >= 0) {
	media = produce_container_from_directory (service, ms->media,
						  categories_dir, index);
      } else {
	error = g_error_new (GRL_ERROR,
			     GRL_ERROR_METADATA_FAILED,
			     "Invalid category id");
      }
    }
    break;
  case YOUTUBE_MEDIA_TYPE_VIDEO:
  default:
    gdata_youtube_service_query_single_video_async (service,
						    NULL,
						    id,
						    NULL,
						    metadata_cb,
						    ms);
    break;
  }

  if (error) {
    ms->callback (ms->source, ms->media, ms->user_data, error);
    g_error_free (error);
  } else if (media) {
    ms->callback (ms->source, ms->media, ms->user_data, NULL);
  }
}
