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
#include <net/grl-net.h>
#include <gdata/gdata.h>
#include <string.h>

#include "grl-youtube.h"

enum {
  PROP_0,
  PROP_SERVICE,
};

#define GRL_YOUTUBE_SOURCE_GET_PRIVATE(object)            \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                  \
                               GRL_YOUTUBE_SOURCE_TYPE,   \
                               GrlYoutubeSourcePriv))

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT youtube_log_domain
GRL_LOG_DOMAIN_STATIC(youtube_log_domain);

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
#define YOUTUBE_VIDEO_URL       "http://www.youtube.com/get_video?video_id=%s&t=%s&asv="
#define YOUTUBE_CATEGORY_URL    "http://gdata.youtube.com/feeds/api/videos/-/%s?&start-index=%s&max-results=%s"

#define YOUTUBE_VIDEO_MIME      "application/x-shockwave-flash"
#define YOUTUBE_SITE_URL        "www.youtube.com"


/* --- Plugin information --- */

#define PLUGIN_ID   YOUTUBE_PLUGIN_ID

#define SOURCE_ID   "grl-youtube"
#define SOURCE_NAME "Youtube"
#define SOURCE_DESC "A source for browsing and searching Youtube videos"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

/* --- Data types --- */

typedef void (*AsyncReadCbFunc) (gchar *data, gpointer user_data);

typedef void (*BuildMediaFromEntryCbFunc) (GrlMedia *media, gpointer user_data);

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
  guint emitted;
  guint matches;
  guint ref_count;
} OperationSpec;

typedef struct {
  GDataService *service;
  CategoryInfo *category_info;
} CategoryCountCb;

typedef struct {
  AsyncReadCbFunc callback;
  gchar *url;
  gpointer user_data;
} AsyncReadCb;

typedef struct {
  GrlMedia *media;
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
						 const gchar *client_id);

static void grl_youtube_source_set_property (GObject *object,
                                             guint propid,
                                             const GValue *value,
                                             GParamSpec *pspec);
static void grl_youtube_source_finalize (GObject *object);

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

static void build_directories (GDataService *service);
static void compute_feed_counts (GDataService *service);
static void compute_category_counts (GDataService *service);

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

static GrlYoutubeSource *ytsrc = NULL;

/* =================== Youtube Plugin  =============== */

gboolean
grl_youtube_plugin_init (GrlPluginRegistry *registry,
                         const GrlPluginInfo *plugin,
                         GList *configs)
{
  const gchar *api_key;
  GrlConfig *config;
  gint config_count;

  GRL_LOG_DOMAIN_INIT (youtube_log_domain, "youtube");

  GRL_DEBUG ("youtube_plugin_init");

  if (!configs) {
    GRL_WARNING ("Configuration not provided! Cannot configure plugin.");
    return FALSE;
  }

  config_count = g_list_length (configs);
  if (config_count > 1) {
    GRL_WARNING ("Provided %d configs, but will only use one", config_count);
  }

  config = GRL_CONFIG (configs->data);
  api_key = grl_config_get_api_key (config);
  if (!api_key) {
    GRL_WARNING ("Missing API Key, cannot configure Youtube plugin");
    return FALSE;
  }

  /* libgdata needs this */
  if (!g_thread_supported()) {
    g_thread_init (NULL);
  }

  GrlYoutubeSource *source =
    grl_youtube_source_new (api_key, YOUTUBE_CLIENT_ID);

  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source));
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_youtube_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== Youtube GObject ================ */

G_DEFINE_TYPE (GrlYoutubeSource, grl_youtube_source, GRL_TYPE_MEDIA_SOURCE);

static GrlYoutubeSource *
grl_youtube_source_new (const gchar *api_key, const gchar *client_id)
{
  GRL_DEBUG ("grl_youtube_source_new");

  GrlYoutubeSource *source;
  GDataYouTubeService *service;

  service = gdata_youtube_service_new (api_key, client_id);
  if (!service) {
    GRL_WARNING ("Failed to initialize gdata service");
    return NULL;
  }

  /* Use auto-split mode because Youtube fails for queries
     that request more than YOUTUBE_MAX_CHUNK results */
  source = GRL_YOUTUBE_SOURCE (g_object_new (GRL_YOUTUBE_SOURCE_TYPE,
					     "source-id", SOURCE_ID,
					     "source-name", SOURCE_NAME,
					     "source-desc", SOURCE_DESC,
					     "auto-split-threshold",
					     YOUTUBE_MAX_CHUNK,
                                             "yt-service", service,
					     NULL));

  ytsrc = source;

  /* Build browse content hierarchy:
      - Query Youtube for available categories
      - Compute category childcounts
      We only need to do this once */
  if (!categories_dir) {
    build_directories (GDATA_SERVICE (service));
  }

  return source;
}

static void
grl_youtube_source_class_init (GrlYoutubeSourceClass * klass)
{
  GrlMediaSourceClass *source_class = GRL_MEDIA_SOURCE_CLASS (klass);
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  source_class->search = grl_youtube_source_search;
  source_class->browse = grl_youtube_source_browse;
  source_class->metadata = grl_youtube_source_metadata;
  metadata_class->supported_keys = grl_youtube_source_supported_keys;
  metadata_class->slow_keys = grl_youtube_source_slow_keys;
  gobject_class->set_property = grl_youtube_source_set_property;
  gobject_class->finalize = grl_youtube_source_finalize;

  g_object_class_install_property (gobject_class,
                                   PROP_SERVICE,
                                   g_param_spec_object ("yt-service",
                                                        "youtube-service",
                                                        "gdata youtube service object",
                                                        GDATA_TYPE_YOUTUBE_SERVICE,
                                                        G_PARAM_WRITABLE
                                                        | G_PARAM_CONSTRUCT_ONLY
                                                        | G_PARAM_STATIC_NAME));

  g_type_class_add_private (klass, sizeof (GrlYoutubeSourcePriv));
}

static void
grl_youtube_source_init (GrlYoutubeSource *source)
{
  source->priv = GRL_YOUTUBE_SOURCE_GET_PRIVATE (source);
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

  if (self->priv->wc)
    g_object_unref (self->priv->wc);

  if (self->priv->service)
    g_object_unref (self->priv->service);

  G_OBJECT_CLASS (grl_youtube_source_parent_class)->finalize (object);
}

/* ======================= Utilities ==================== */

static OperationSpec *
operation_spec_new ()
{
  GRL_DEBUG ("Allocating new spec");
  OperationSpec *os =  g_slice_new0 (OperationSpec);
  os->ref_count = 1;
  return os;
}

static void
operation_spec_unref (OperationSpec *os)
{
  os->ref_count--;
  if (os->ref_count == 0) {
    g_slice_free (OperationSpec, os);
    GRL_DEBUG ("freeing spec");
  }
}

static void
operation_spec_ref (OperationSpec *os)
{
  GRL_DEBUG ("Reffing spec");
  os->ref_count++;
}

inline static GrlNetWc *
get_wc ()
{
  if (ytsrc && !ytsrc->priv->wc)
    ytsrc->priv->wc = grl_net_wc_new ();

  return ytsrc->priv->wc;
}

static void
read_done_cb (GObject *source_object,
              GAsyncResult *res,
              gpointer user_data)
{
  AsyncReadCb *arc = (AsyncReadCb *) user_data;
  GError *wc_error = NULL;
  gchar *content = NULL;

  grl_net_wc_request_finish (GRL_NET_WC (source_object),
                         res,
                         &content,
                         NULL,
                         &wc_error);
  if (wc_error) {
    GRL_WARNING ("Failed to open '%s': %s", arc->url, wc_error->message);
    arc->callback (NULL, arc->user_data);
  } else {
    arc->callback (content, arc->user_data);
  }
  g_free (arc->url);
  g_slice_free (AsyncReadCb, arc);
}

static void
read_url_async (const gchar *url,
                AsyncReadCbFunc callback,
                gpointer user_data)
{
  AsyncReadCb *arc;

  arc = g_slice_new0 (AsyncReadCb);
  arc->url = g_strdup (url);
  arc->callback = callback;
  arc->user_data = user_data;

  GRL_DEBUG ("Opening async '%s'", url);
  grl_net_wc_request_async (get_wc (),
                        url,
                        NULL,
                        read_done_cb,
                        arc);
}

static void
set_media_url_async_read_cb (gchar *data, gpointer user_data)
{
  SetMediaUrlAsyncReadCb *cb_data = (SetMediaUrlAsyncReadCb *) user_data;
  gchar *url = NULL;
  GMatchInfo *match_info = NULL;
  static GRegex *regex = NULL;

  if (!data) {
    goto done;
  }

  if (regex == NULL) {
    regex = g_regex_new (".*&fmt_url_map=([^&]+)&", G_REGEX_OPTIMIZE, 0, NULL);
  }

  /* Check if we find the url mapping */
  g_regex_match (regex, data, 0, &match_info);
  if (g_match_info_matches (match_info) == TRUE) {
    gchar *url_map_escaped, *url_map;
    gchar **mappings;

    url_map_escaped = g_match_info_fetch (match_info, 1);
    url_map = g_uri_unescape_string (url_map_escaped, NULL);
    g_free (url_map_escaped);

    mappings = g_strsplit (url_map, ",", 0);
    g_free (url_map);

    if (mappings != NULL) {
      /* TODO: We get the URL from the first format available.
       * We should provide the list of available urls or let the user
       * configure preferred formats
       */
      gchar **mapping = g_strsplit (mappings[0], "|", 2);
      url = g_strdup (mapping[1]);
      g_strfreev (mapping);
    }
  } else {
    GRL_DEBUG ("Format array not found, using token workaround");
    gchar *token_start;
    gchar *token_end;
    gchar *token;
    const gchar *video_id;

    token_start = g_strrstr (data, "&token=");
    if (!token_start) {
      goto done;
    }
    token_start += 7;
    token_end = strstr (token_start, "&");
    token = g_strndup (token_start, token_end - token_start);

    video_id = grl_media_get_id (cb_data->media);
    url = g_strdup_printf (YOUTUBE_VIDEO_URL, video_id, token);
    g_free (token);
  }

 done:
  if (url) {
    grl_media_set_url (cb_data->media, url);
    g_free (url);
  }

  cb_data->callback (cb_data->media, cb_data->user_data);

  g_free (cb_data);
}

static void
set_media_url (GrlMedia *media,
	       BuildMediaFromEntryCbFunc callback,
	       gpointer user_data)
{
  const gchar *video_id;
  gchar *video_info_url;
  SetMediaUrlAsyncReadCb *set_media_url_async_read_data;

  /* The procedure to get the video url is:
   * 1) Read the video info URL using the video id (async operation)
   * 2) In the video info page, there should be an array of supported formats
   *    and their corresponding URLs, right now we just use the first one we get.
   *    (see set_media_url_async_read_cb).
   *    TODO: we should be able to provide various urls or at least
   *          select preferred formats via configuration
   *    TODO: we should set mime-type accordingly to the format selected
   * 3) As a workaround in case no format array is found we get the video token
   *    and figure out the url of the video using the video id and the token.
   */

  video_id = grl_media_get_id (media);
  video_info_url = g_strdup_printf (YOUTUBE_VIDEO_INFO_URL, video_id);

  set_media_url_async_read_data = g_new0 (SetMediaUrlAsyncReadCb, 1);
  set_media_url_async_read_data->media = media;
  set_media_url_async_read_data->callback = callback;
  set_media_url_async_read_data->user_data = user_data;

  read_url_async (video_info_url,
		  set_media_url_async_read_cb,
		  set_media_url_async_read_data);

  g_free (video_info_url);
}

static void
build_media_from_entry (GrlMedia *content,
			GDataEntry *entry,
			const GList *keys,
			BuildMediaFromEntryCbFunc callback,
			gpointer user_data)
{
  GDataYouTubeVideo *video;
  GrlMedia *media;
  GList *iter;
  gboolean need_url = FALSE;

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
    if (iter->data == GRL_METADATA_KEY_TITLE) {
      grl_media_set_title (media, gdata_entry_get_title (entry));
    } else if (iter->data == GRL_METADATA_KEY_DESCRIPTION) {
      grl_media_set_description (media,
				 gdata_youtube_video_get_description (video));
    } else if (iter->data == GRL_METADATA_KEY_THUMBNAIL) {
      GList *thumb_list;
      thumb_list = gdata_youtube_video_get_thumbnails (video);
      if (thumb_list) {
        GDataMediaThumbnail *thumbnail;
        thumbnail = GDATA_MEDIA_THUMBNAIL (thumb_list->data);
        grl_media_set_thumbnail (media,
                                 gdata_media_thumbnail_get_uri (thumbnail));
      }
    } else if (iter->data == GRL_METADATA_KEY_DATE) {
      GTimeVal date;
      gchar *date_str;
      gdata_entry_get_published (entry, &date);
      date_str = g_time_val_to_iso8601 (&date);
      grl_media_set_date (media, date_str);
      g_free (date_str);
    } else if (iter->data == GRL_METADATA_KEY_DURATION) {
      grl_media_set_duration (media, gdata_youtube_video_get_duration (video));
    } else if (iter->data == GRL_METADATA_KEY_MIME) {
      grl_media_set_mime (media, YOUTUBE_VIDEO_MIME);
    } else if (iter->data == GRL_METADATA_KEY_SITE) {
      grl_media_set_site (media, gdata_youtube_video_get_player_uri (video));
    } else if (iter->data == GRL_METADATA_KEY_EXTERNAL_URL) {
      grl_media_set_external_url (media, 
				  gdata_youtube_video_get_player_uri (video));
    } else if (iter->data == GRL_METADATA_KEY_RATING) {
      gdouble average;
      gdata_youtube_video_get_rating (video, NULL, NULL, NULL, &average);
      grl_media_set_rating (media, average, 5.00);
    } else if (iter->data == GRL_METADATA_KEY_URL) {
      /* This needs another query and will be resolved asynchronously p Q*/
      need_url = TRUE;
    } else if (iter->data == GRL_METADATA_KEY_EXTERNAL_PLAYER) {
      GDataYouTubeContent *youtube_content;
      youtube_content =
	gdata_youtube_video_look_up_content (video,
					     "application/x-shockwave-flash");
      if (youtube_content != NULL) {
	GDataMediaContent *content = GDATA_MEDIA_CONTENT (youtube_content);
	grl_media_set_external_player (media,
				       gdata_media_content_get_uri (content));
      }
    }
    iter = g_list_next (iter);
  }

  if (need_url) {
    /* URL resolution is async */
    set_media_url (media, callback, user_data);
  } else {
    callback (media, user_data);
  }
}

static void
parse_categories (xmlDocPtr doc, xmlNodePtr node, GDataService *service)
{
  GRL_DEBUG ("parse_categories");

  guint total = 0;
  GList *all = NULL, *iter;
  CategoryInfo *cat_info;
  gchar *id;
  guint index = 0;

  while (node) {
    cat_info = g_slice_new (CategoryInfo);
    id = (gchar *) xmlGetProp (node, (xmlChar *) "term");
    cat_info->id = g_strconcat (YOUTUBE_CATEGORIES_ID, "/", id, NULL);
    cat_info->name = (gchar *) xmlGetProp (node, (xmlChar *) "label");
    all = g_list_prepend (all, cat_info);
    g_free (id);
    node = node->next;
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
      categories_dir[total - 1].name = cat_info->name;
      categories_dir[total - 1].count = 0;
      total--;
      g_slice_free (CategoryInfo, cat_info);
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

  doc = xmlReadMemory (xmldata, strlen (xmldata), NULL, NULL,
                       XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
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

  parse_categories (doc, node, GDATA_SERVICE (user_data));

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
  gint i;

  for (i=0; i<root_dir[ROOT_DIR_CATEGORIES_INDEX].count; i++) {
    if (!strcmp (categories_dir[i].id, category_id)) {
      return i;
    }
  }
  return -1;
}

static void
item_count_cb (GObject *object, GAsyncResult *result, CategoryCountCb *cc)
{
  GRL_DEBUG ("item_count_cb");

  GDataFeed *feed;
  GError *error = NULL;
  
  feed = gdata_service_query_finish (GDATA_SERVICE (cc->service),
				     result, &error);
  if (error) {
    GRL_WARNING ("Failed to compute count for category '%s': %s",
                 cc->category_info->id, error->message);
    g_error_free (error);
  } else if (feed) {
    cc->category_info->count = gdata_feed_get_total_results (feed);
    GRL_DEBUG ("Category '%s' - childcount: '%u'",
               cc->category_info->id, cc->category_info->count);
  }

  if (feed) {
    g_object_unref (feed);
  }
  g_slice_free (CategoryCountCb, cc);
}

static void
compute_category_counts (GDataService *service)
{
  gint i;

  GRL_DEBUG ("compute_category_counts");

  for (i=0; i<root_dir[ROOT_DIR_CATEGORIES_INDEX].count; i++) {
    GRL_DEBUG ("Computing chilcount for category '%s'", categories_dir[i].id);
    GDataQuery *query = gdata_query_new_with_limits (NULL, 0, 1);
    const gchar *category_term =
      get_category_term_from_id (categories_dir[i].id);
    gdata_query_set_categories (query, category_term);
    CategoryCountCb *cc = g_slice_new (CategoryCountCb);
    cc->service = service;
    cc->category_info = &categories_dir[i];
    gdata_youtube_service_query_videos_async (GDATA_YOUTUBE_SERVICE (service),
					      query,
					      NULL, NULL, NULL,
					      (GAsyncReadyCallback) item_count_cb,
					      cc);
    g_object_unref (query);
  }
}

static void
compute_feed_counts (GDataService *service)
{
  gint i;
  GRL_DEBUG ("compute_feed_counts");

  for (i=0; i<root_dir[ROOT_DIR_FEEDS_INDEX].count; i++) {
    GRL_DEBUG ("Computing chilcount for feed '%s'", feeds_dir[i].id);
    gint feed_type = get_feed_type_from_id (feeds_dir[i].id);
    GDataQuery *query = gdata_query_new_with_limits (NULL, 0, 1);
    CategoryCountCb *cc = g_slice_new (CategoryCountCb);
    cc->service = service;
    cc->category_info = &feeds_dir[i];
    gdata_youtube_service_query_standard_feed_async (GDATA_YOUTUBE_SERVICE (service),
						     feed_type,
						     query,
						     NULL, NULL, NULL,
						     (GAsyncReadyCallback) item_count_cb,
						     cc);
    g_object_unref (query);
  }
}

static void
build_media_from_entry_metadata_cb (GrlMedia *media, gpointer user_data)
{
  GrlMediaSourceMetadataSpec *ms = (GrlMediaSourceMetadataSpec *) user_data;
  ms->callback (ms->source, media, ms->user_data, NULL);
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

  if (os->emitted < os->count) {
    remaining = os->count - os->emitted - 1;
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
build_directories (GDataService *service)
{
  GRL_DEBUG ("build_drectories");

  /* Parse category list from Youtube and compute category counts */
  read_url_async (YOUTUBE_CATEGORIES_URL,
                  build_categories_directory_read_cb,
                  service);

  /* Compute feed counts */
  compute_feed_counts (service);
}

static void
metadata_cb (GObject *object,
	     GAsyncResult *result,
	     gpointer user_data)
{
  GRL_DEBUG ("metadata_cb");

  GError *error = NULL;
  GrlYoutubeSource *source;
  GDataEntry *video;
  GDataService *service;
  GrlMediaSourceMetadataSpec *ms = (GrlMediaSourceMetadataSpec *) user_data;

  source = GRL_YOUTUBE_SOURCE (ms->source);
  service = GDATA_SERVICE (source->priv->service);

#ifdef GDATA_API_SUBJECT_TO_CHANGE
  video = gdata_service_query_single_entry_finish (service, result, &error);
#else
  video =
    GDATA_ENTRY (gdata_youtube_service_query_single_video_finish
                   (GDATA_YOUTUBE_SERVICE (service), result, &error));
#endif
  if (error) {
    error->code = GRL_CORE_ERROR_METADATA_FAILED;
    ms->callback (ms->source, ms->media, ms->user_data, error);
    g_error_free (error);
  } else {
    build_media_from_entry (ms->media, video, ms->keys,
			    build_media_from_entry_metadata_cb, ms);
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
    /* Keep track of the items we got here. Due to the asynchronous
     * nature of build_media_from_entry(), when search_cb is invoked
     * we have to check if we got as many results as we requested or
     * not, and handle that situation properly */
    os->matches++;
    build_media_from_entry (NULL, entry, os->keys,
			    build_media_from_entry_search_cb, os);
  } else {
    GRL_WARNING ("Invalid index/count received grom libgdata, ignoring result");
  }

  /* The entry will be freed when freeing the feed in search_cb */
}

static void
search_cb (GObject *object, GAsyncResult *result, OperationSpec *os)
{
  GRL_DEBUG ("search_cb");

  GDataFeed *feed;
  GError *error = NULL;
  gboolean need_extra_unref = FALSE;
  GrlYoutubeSource *source = GRL_YOUTUBE_SOURCE (os->source);

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
      error = g_error_new (GRL_CORE_ERROR,
			   os->error_code,
			   "Failed to obtain feed from Youtube");
    } else {
      error->code = os->error_code;
    }
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
    need_extra_unref = TRUE;
  }

  if (feed)
    g_object_unref (feed);

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
produce_container_from_directory (GDataService *service,
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
  GRL_DEBUG ("produce_from_directory");

  guint index, remaining;

  /* Youtube's first index is 1, but the directories start at 0 */
  os->skip--;

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
			 "Invalid feed id: %s", os->container_id);
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

  service = GRL_YOUTUBE_SOURCE (os->source)->priv->service;
  query = gdata_query_new_with_limits (NULL , os->skip, os->count);
  os->category_info = &feeds_dir[feed_type];
  gdata_youtube_service_query_standard_feed_async (GDATA_YOUTUBE_SERVICE (service),
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
  GDataService *service;
  const gchar *category_term;
  gint category_index;

  category_term = get_category_term_from_id (os->container_id);
  category_index = get_category_index_from_id (os->container_id);

  if (!category_term) {
    error = g_error_new (GRL_CORE_ERROR,
			 GRL_CORE_ERROR_BROWSE_FAILED,
			 "Invalid category id: %s", os->container_id);
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
  query = gdata_query_new_with_limits (NULL , os->skip, os->count);
  os->category_info = &categories_dir[category_index];
  gdata_query_set_categories (query, category_term);
  gdata_youtube_service_query_videos_async (GDATA_YOUTUBE_SERVICE (service),
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
				      GRL_METADATA_KEY_EXTERNAL_URL,
                                      GRL_METADATA_KEY_DESCRIPTION,
                                      GRL_METADATA_KEY_DURATION,
                                      GRL_METADATA_KEY_DATE,
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

  GRL_DEBUG ("grl_youtube_source_search (%u, %u)", ss->skip, ss->count);

  os = operation_spec_new ();
  os->source = source;
  os->operation_id = ss->search_id;
  os->keys = ss->keys;
  os->skip = ss->skip + 1;
  os->count = ss->count;
  os->callback = ss->callback;
  os->user_data = ss->user_data;
  os->error_code = GRL_CORE_ERROR_SEARCH_FAILED;

  /* Look for OPERATION_SPEC_REF_RATIONALE for details */
  operation_spec_ref (os);

  query = gdata_query_new_with_limits (ss->text, os->skip, os->count);
  gdata_youtube_service_query_videos_async (GDATA_YOUTUBE_SERVICE (GRL_YOUTUBE_SOURCE (source)->priv->service),
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

  GRL_DEBUG ("grl_youtube_source_browse: %s", grl_media_get_id (bs->container));

  container_id = grl_media_get_id (bs->container);

  os = operation_spec_new ();
  os->source = bs->source;
  os->operation_id = bs->browse_id;
  os->container_id = container_id;
  os->keys = bs->keys;
  os->flags = bs->flags;
  os->skip = bs->skip + 1;
  os->count = bs->count;
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
  GDataService *service;
  GError *error = NULL;
  GrlMedia *media = NULL;

  GRL_DEBUG ("grl_youtube_source_metadata");

  id = grl_media_get_id (ms->media);
  media_type = classify_media_id (id);
  service = GRL_YOUTUBE_SOURCE (source)->priv->service;

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
	error = g_error_new (GRL_CORE_ERROR,
			     GRL_CORE_ERROR_METADATA_FAILED,
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
	error = g_error_new (GRL_CORE_ERROR,
			     GRL_CORE_ERROR_METADATA_FAILED,
			     "Invalid category id");
      }
    }
    break;
  case YOUTUBE_MEDIA_TYPE_VIDEO:
  default:
#ifdef GDATA_API_SUBJECT_TO_CHANGE
    {
      gchar *entryid = g_strconcat ("tag:youtube.com,2008:video:", id, NULL);
      gdata_service_query_single_entry_async (service,
                                              entryid,
                                              NULL,
                                              GDATA_TYPE_YOUTUBE_VIDEO,
                                              NULL,
                                              metadata_cb,
                                              ms);
      g_free (entryid);
    }
#else
    gdata_youtube_service_query_single_video_async (GDATA_YOUTUBE_SERVICE (service),
                                                    NULL,
                                                    id,
                                                    NULL,
                                                    metadata_cb,
                                                    ms);
#endif
    break;
  }

  if (error) {
    ms->callback (ms->source, ms->media, ms->user_data, error);
    g_error_free (error);
  } else if (media) {
    ms->callback (ms->source, ms->media, ms->user_data, NULL);
  }
}
