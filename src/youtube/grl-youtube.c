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
#include <libgnomevfs/gnome-vfs.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <string.h>
#include <stdlib.h>

#include "util/gnomevfs.h"
#include "grl-youtube.h"

/* --------- Logging  -------- */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "grl-youtube"

/* ----- Root categories ---- */

#define YOUTUBE_ROOT_NAME       "Youtube"

#define YOUTUBE_FEEDS_ID        "standard-feeds"
#define YOUTUBE_FEEDS_NAME      "Standard feeds"
#define YOUTUBE_FEEDS_URL       NULL

#define YOUTUBE_CATEGORIES_ID   "categories"
#define YOUTUBE_CATEGORIES_NAME "Categories"
#define YOUTUBE_CATEGORIES_URL  "http://gdata.youtube.com/schemas/2007/categories.cat"

/* ----- Feeds categories ---- */

#define YOUTUBE_VIEWED_ID       (YOUTUBE_FEEDS_ID "/most-viewed")
#define YOUTUBE_VIEWED_NAME     "Most viewed"
#define YOUTUBE_VIEWED_URL      "http://gdata.youtube.com/feeds/standardfeeds/most_viewed?start-index=%d&max-results=%d"

#define YOUTUBE_RATED_ID        (YOUTUBE_FEEDS_ID "/most-rated")
#define YOUTUBE_RATED_NAME      "Most rated"
#define YOUTUBE_RATED_URL       "http://gdata.youtube.com/feeds/standardfeeds/top_rated?start-index=%d&max-results=%d"

#define YOUTUBE_FAVS_ID         (YOUTUBE_FEEDS_ID "/top-favs")
#define YOUTUBE_FAVS_NAME       "Top favorites"
#define YOUTUBE_FAVS_URL        "http://gdata.youtube.com/feeds/standardfeeds/top_favorites?start-index=%d&max-results=%d"

#define YOUTUBE_RECENT_ID       (YOUTUBE_FEEDS_ID "/most-recent")
#define YOUTUBE_RECENT_NAME     "Most recent"
#define YOUTUBE_RECENT_URL      "http://gdata.youtube.com/feeds/standardfeeds/most_recent?start-index=%d&max-results=%d"

#define YOUTUBE_DISCUSSED_ID    (YOUTUBE_FEEDS_ID "/most-discussed")
#define YOUTUBE_DISCUSSED_NAME  "Most discussed"
#define YOUTUBE_DISCUSSED_URL   "http://gdata.youtube.com/feeds/standardfeeds/most_discussed?start-index=%d&max-results=%d"

#define YOUTUBE_FEATURED_ID     (YOUTUBE_FEEDS_ID "/featured")
#define YOUTUBE_FEATURED_NAME   "Featured"
#define YOUTUBE_FEATURED_URL    "http://gdata.youtube.com/feeds/standardfeeds/recently_featured?start-index=%d&max-results=%d"

#define YOUTUBE_MOBILE_ID       (YOUTUBE_FEEDS_ID "/mobile")
#define YOUTUBE_MOBILE_NAME     "Watch on mobile"
#define YOUTUBE_MOBILE_URL      "http://gdata.youtube.com/feeds/standardfeeds/watch_on_mobile?start-index=%d&max-results=%d"

/* ----- Other Youtube URLs ---- */

#define YOUTUBE_VIDEO_INFO_URL  "http://www.youtube.com/get_video_info?video_id=%s"
#define YOUTUBE_VIDEO_URL       "http://www.youtube.com/get_video?video_id=%s&t=%s"
#define YOUTUBE_SEARCH_URL      "http://gdata.youtube.com/feeds/api/videos?vq=%s&start-index=%s&max-results=%s"
#define YOUTUBE_CATEGORY_URL    "http://gdata.youtube.com/feeds/api/videos/-/%s?&start-index=%s&max-results=%s"

/* --- Youtube parsing hints ---- */

#define YOUTUBE_TOTAL_RESULTS_START "<openSearch:totalResults>"
#define YOUTUBE_TOTAL_RESULTS_END   "</openSearch:totalResults>"

/* --- Other --- */

#define YOUTUBE_MAX_CHUNK   50
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
  gchar *id;
  gchar *title;
  gchar *published;
  gchar *author;
  gchar *thumbnail;
  gchar *description;
  gchar *duration;
  gchar *rating;
  gboolean restricted;
} Entry;

typedef struct {
  GrlMediaSource *source;
  guint operation_id;
  const gchar *container_id;
  GList *keys;
  GrlMetadataResolutionFlags flags;
  guint skip;
  guint count;
  gboolean chained_chunk;
  gchar *query_url;
  gboolean cancelled;
  GrlMediaSourceResultCb callback;
  gpointer user_data;
} OperationSpec;

typedef struct {
  GrlMetadataSource *source;
  const GList *keys;
  GrlMediaSourceMetadataCb callback;
  gpointer user_data;
} MetadataOperationSpec;

typedef struct {
  OperationSpec *os;
  xmlNodePtr node;
  xmlDocPtr doc;
  guint count;
} ParseEntriesIdle;

typedef struct {
  gchar *id;
  gchar *name;
  gchar *url;
} CategoryInfo;

typedef struct {
  AsyncReadCbFunc callback;
  gchar *url;
  gpointer user_data;
} AsyncReadCb;

typedef struct {
  OperationSpec *os;
  CategoryInfo *directory;
  guint index;
  guint remaining;
  gboolean set_childcount;
} ProduceFromDirectoryIdle;

typedef enum {
  YOUTUBE_MEDIA_TYPE_ROOT,
  YOUTUBE_MEDIA_TYPE_FEEDS,
  YOUTUBE_MEDIA_TYPE_CATEGORIES,
  YOUTUBE_MEDIA_TYPE_FEED,
  YOUTUBE_MEDIA_TYPE_CATEGORY,
  YOUTUBE_MEDIA_TYPE_VIDEO,
} YoutubeMediaType;

static GrlYoutubeSource *grl_youtube_source_new (void);

gboolean grl_youtube_plugin_init (GrlPluginRegistry *registry,
                                  const GrlPluginInfo *plugin,
                                  GList *configs);

static const GList *grl_youtube_source_supported_keys (GrlMetadataSource *source);

static const GList *grl_youtube_source_slow_keys (GrlMetadataSource *source);

static void grl_youtube_source_metadata (GrlMediaSource *source,
                                         GrlMediaSourceMetadataSpec *ms);

static void grl_youtube_source_search (GrlMediaSource *source,
                                       GrlMediaSourceSearchSpec *ss);

static void grl_youtube_source_browse (GrlMediaSource *source,
                                       GrlMediaSourceBrowseSpec *bs);

static void grl_youtube_source_cancel (GrlMediaSource *source,
                                       guint operation_id);

static gchar *read_url (const gchar *url);

static void build_categories_directory (void);

static void produce_next_video_chunk (OperationSpec *os);

/* ==================== Global Data  ================= */

guint root_dir_size = 2;
CategoryInfo root_dir[] = {
  {YOUTUBE_FEEDS_ID, YOUTUBE_FEEDS_NAME, YOUTUBE_FEEDS_URL},
  {YOUTUBE_CATEGORIES_ID, YOUTUBE_CATEGORIES_NAME, YOUTUBE_CATEGORIES_URL},
  {NULL, NULL, NULL}
};

guint feeds_dir_size = 7;
CategoryInfo feeds_dir[] = {
  {YOUTUBE_VIEWED_ID, YOUTUBE_VIEWED_NAME, YOUTUBE_VIEWED_URL},
  {YOUTUBE_RATED_ID, YOUTUBE_RATED_NAME, YOUTUBE_RATED_URL},
  {YOUTUBE_FAVS_ID, YOUTUBE_FAVS_NAME, YOUTUBE_FAVS_URL},
  {YOUTUBE_RECENT_ID, YOUTUBE_RECENT_NAME, YOUTUBE_RECENT_URL},
  {YOUTUBE_DISCUSSED_ID, YOUTUBE_DISCUSSED_NAME, YOUTUBE_DISCUSSED_URL},
  {YOUTUBE_FEATURED_ID, YOUTUBE_FEATURED_NAME, YOUTUBE_FEATURED_URL},
  {YOUTUBE_MOBILE_ID, YOUTUBE_MOBILE_NAME, YOUTUBE_MOBILE_URL},
  {NULL, NULL, NULL}
};

guint categories_dir_size = 0;
CategoryInfo *categories_dir = NULL;

/* =================== Youtube Plugin  =============== */

gboolean
grl_youtube_plugin_init (GrlPluginRegistry *registry,
                         const GrlPluginInfo *plugin,
                         GList *config)
{
  g_debug ("youtube_plugin_init\n");

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
  return g_object_new (GRL_YOUTUBE_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       NULL);
}

static void
grl_youtube_source_class_init (GrlYoutubeSourceClass * klass)
{
  GrlMediaSourceClass *source_class = GRL_MEDIA_SOURCE_CLASS (klass);
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);
  source_class->browse = grl_youtube_source_browse;
  source_class->search = grl_youtube_source_search;
  source_class->cancel = grl_youtube_source_cancel;
  source_class->metadata = grl_youtube_source_metadata;
  metadata_class->supported_keys = grl_youtube_source_supported_keys;
  metadata_class->slow_keys = grl_youtube_source_slow_keys;
}

static void
grl_youtube_source_init (GrlYoutubeSource *source)
{
  if (!categories_dir) {
    build_categories_directory ();
  }
}

G_DEFINE_TYPE (GrlYoutubeSource, grl_youtube_source, GRL_TYPE_MEDIA_SOURCE);

/* ======================= Utilities ==================== */

static void
print_entry (Entry *entry)
{
  g_print ("Entry Information:\n");
  g_print ("            ID: %s (%d)\n", entry->id, entry->restricted);
  g_print ("         Title: %s\n", entry->title);
  g_print ("          Date: %s\n", entry->published);
  g_print ("        Author: %s\n", entry->author);
  g_print ("      Duration: %s\n", entry->duration);
  g_print ("     Thumbnail: %s\n", entry->thumbnail);
  g_print ("        Rating: %s\n", entry->rating);
  g_print ("   Description: %s\n", entry->description);
}

static void
free_entry (Entry *entry)
{
  g_free (entry->id);
  g_free (entry->title);
  g_free (entry->published);
  g_free (entry->author);
  g_free (entry->duration);
  g_free (entry->thumbnail);
  g_free (entry->description);
  g_free (entry->rating);
  g_free (entry);
}

static void
free_operation_spec (OperationSpec *os)
{
  g_free (os->query_url);
  g_free (os);
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

static void
read_done_cb (gchar *content,
              gpointer user_data)
{
  AsyncReadCb *arc = (AsyncReadCb *) user_data;

  if (!content) {
    g_warning ("Failed to open '%s'", arc->url);
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
  AsyncReadCb *arc;

  g_debug ("Opening async '%s'", url);

  arc = g_new0 (AsyncReadCb, 1);
  arc->url = g_strdup (url);
  arc->callback = callback;
  arc->user_data = user_data;

  grl_plugins_gnome_vfs_read_url_async (url, read_done_cb, arc);
}

static gchar *
read_url (const gchar *url)
{
  gchar *content = NULL;
  GnomeVFSResult result;

  g_debug ("Opening '%s'", url);
  result = gnome_vfs_read_entire_file (url, NULL, &content);
  if (result != GNOME_VFS_OK) {
    g_warning ("Failed reading '%s': %s", url,
               gnome_vfs_result_to_string (result));
    g_free (content);
    return NULL;
  } else {
    return content;
  }
}

static GrlMedia *
build_media_from_entry (GrlMedia *content,
			const Entry *entry,
			const GList *keys)
{
  GrlMedia *media;
  gchar *url;
  GList *iter;

  if (!content) {
    media = grl_media_video_new ();
  } else {
    media = content;
  }

  iter = (GList *) keys;
  while (iter) {
    GrlKeyID key_id = POINTER_TO_GRLKEYID (iter->data);
    switch (key_id) {
    case GRL_METADATA_KEY_ID:
      grl_media_set_id (media, entry->id);
      break;
    case GRL_METADATA_KEY_TITLE:
      grl_media_set_title (media, entry->title);
      break;
    case GRL_METADATA_KEY_AUTHOR:
      grl_media_set_author (media, entry->author);
      break;
    case GRL_METADATA_KEY_DESCRIPTION:
      grl_media_set_description (media, entry->description);
      break;
    case GRL_METADATA_KEY_THUMBNAIL:
      grl_media_set_thumbnail (media, entry->thumbnail);
      break;
    case GRL_METADATA_KEY_DATE:
      grl_media_set_date (media, entry->published);
      break;
    case GRL_METADATA_KEY_DURATION:
      grl_media_set_duration (media, atoi (entry->duration));
      break;
    case GRL_METADATA_KEY_MIME:
      grl_media_set_mime (media, YOUTUBE_VIDEO_MIME);
      break;
    case GRL_METADATA_KEY_SITE:
      grl_media_set_site (media, YOUTUBE_SITE_URL);
      break;
    case GRL_METADATA_KEY_RATING:
      grl_media_set_rating (media, entry->rating, "5.00");
      break;
    case GRL_METADATA_KEY_URL:
      if (!entry->restricted) {
	url = get_video_url (entry->id);
	if (url) {
	  grl_media_set_url (media, url);
	}
	g_free (url);
      }
      break;
    default:
      break;
    }
    iter = g_list_next (iter);
  }

  return media;
}

static gchar *
parse_id (xmlDocPtr doc, xmlNodePtr id)
{
  gchar *value, *video_id;
  value = (gchar *) xmlNodeListGetString (doc, id->xmlChildrenNode, 1);
  video_id = g_strdup (g_strrstr (value , "/") + 1);
  g_free (value);
  return video_id;
}

static gchar *
parse_author (xmlDocPtr doc, xmlNodePtr author)
{
  xmlNodePtr node;
  node = author->xmlChildrenNode;
  while (node) {
    if (!xmlStrcmp (node->name, (const xmlChar *) "name")) {
      return (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    }
    node = node->next;
  }
  return NULL;
}

static void
parse_media_group (xmlDocPtr doc, xmlNodePtr media, Entry *entry)
{
  xmlNodePtr node;
  xmlNs *ns;

  node = media->xmlChildrenNode;
  while (node) {
    ns = node->ns;
    if (!xmlStrcmp (node->name, (const xmlChar *) "description")) {
      entry->description =
	(gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    } else if (!xmlStrcmp (node->name, (const xmlChar *) "thumbnail") &&
	       !entry->thumbnail) {
      entry->thumbnail =
	(gchar *) xmlGetProp (node, (xmlChar *) "url");
    } else if (!xmlStrcmp (node->name, (const xmlChar *) "duration")) {
      entry->duration =
	(gchar *) xmlGetProp (node, (xmlChar *) "seconds");
    }
    node = node->next;
  }
}

static void
parse_app_control (xmlDocPtr doc, xmlNodePtr media, Entry *entry)
{
  xmlNodePtr node;
  xmlChar *value;

  node = media->xmlChildrenNode;
  while (node) {
    if (!xmlStrcmp (node->name, (const xmlChar *) "state")) {
      value = xmlGetProp (node, (xmlChar *) "name");
      if (!xmlStrcmp (value, (xmlChar *) "restricted")) {
	entry->restricted = TRUE;
      }
      g_free ((gchar *) value);
    }
    node = node->next;
  }
}

static void
parse_entry (xmlDocPtr doc, xmlNodePtr entry, Entry *data)
{
  xmlNodePtr node;
  xmlNs *ns;

  node = entry->xmlChildrenNode;
  while (node) {
    ns = node->ns;
    if (!xmlStrcmp (node->name, (const xmlChar *) "id")) {
      data->id = parse_id (doc, node);
    } else if (!xmlStrcmp (node->name, (const xmlChar *) "published")) {
      data->published =
	(gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    } else if (!xmlStrcmp (node->name, (const xmlChar *) "title")) {
      data->title =
	(gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    } else if (!xmlStrcmp (node->name, (const xmlChar *) "author")) {
      data->author = parse_author (doc, node);
    } else if (ns && !xmlStrcmp (ns->prefix, (const xmlChar *) "media") &&
	       !xmlStrcmp (node->name, (const xmlChar *) "group")) {
      parse_media_group (doc, node, data);
    } else if (ns && !xmlStrcmp (ns->prefix, (const xmlChar *) "app") &&
	       !xmlStrcmp (node->name, (const xmlChar *) "control")) {
      parse_app_control (doc, node, data);
    } else if (ns && !xmlStrcmp (ns->prefix, (const xmlChar *) "gd") &&
	       !xmlStrcmp (node->name, (const xmlChar *) "rating")) {
      data->rating = (gchar *) xmlGetProp (node, (xmlChar *) "average");
    }

    node = node->next;
  }
}

static gboolean
parse_entries_idle (gpointer user_data)
{
  ParseEntriesIdle *pei = (ParseEntriesIdle *) user_data;
  gboolean parse_more = FALSE;

  g_debug ("parse_entries_idle");

  /* Do not bother if operation was cancelled */
  if (!pei->os->cancelled) {
    while (pei->node && xmlStrcmp (pei->node->name,
				   (const xmlChar *) "entry")) {
      pei->node = pei->node->next;
    }

    if (pei->node) {
      Entry *entry = g_new0 (Entry, 1);
      parse_entry (pei->doc, pei->node, entry);
      if (0) print_entry (entry);
      GrlMedia *media =
	build_media_from_entry (NULL, entry, pei->os->keys);
      free_entry (entry);

      pei->os->skip++;
      pei->os->count--;
      pei->count++;
      pei->os->callback (pei->os->source,
			 pei->os->operation_id,
			 media,
			 pei->os->count,
			 pei->os->user_data,
			 NULL);

      parse_more = TRUE;
      pei->node = pei->node->next;
    }
  }

  if (!parse_more) {
    xmlFreeDoc (pei->doc);
    /* Did we split the query in chunks? if so, query next chunk now */
    if (!pei->os->cancelled && pei->os->count > 0 && pei->count > 0) {
      /* We can go for the next chunk: operation not cancelled,
	 still more results were requested and last chunk returned
	 valid results */
      produce_next_video_chunk (pei->os);
    } else {
      /* The operation is finished */
      if (pei->os->cancelled && pei->os->count > 0) {
	/* Operation cancelled before finalization. We stop the
	   operation now */
	g_debug ("Operation was cancelled, stopping next chunk");
	pei->os->callback (pei->os->source,
			   pei->os->operation_id,
			   NULL,
			   0,
			   pei->os->user_data,
			   NULL);
      } else if (pei->count == 0) {
	/* This can only happen in youtube's totalResults is wrong
	   (actually greater than it should be). If that's the case
	   there are not more results and we must finish the operation now */
	g_warning ("Wrong totalResults from Youtube "		\
		   "using NULL media to finish operation");
	pei->os->callback (pei->os->source,
			   pei->os->operation_id,
			   NULL,
			   0,
			   pei->os->user_data,
			   NULL);
      }
      free_operation_spec (pei->os);
    }
    g_free (pei);
  }

  return parse_more;
}

static void
parse_feed (OperationSpec *os, const gchar *str, GError **error)
{
  xmlDocPtr doc;
  xmlNodePtr node;
  guint total_results;
  xmlNs *ns;

  doc = xmlRecoverDoc ((xmlChar *) str);
  if (!doc) {
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_BROWSE_FAILED,
			  "Failed to parse Youtube's response");
    goto free_resources;
  }

  node = xmlDocGetRootElement (doc);
  if (!node) {
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_BROWSE_FAILED,
			  "Empty response from Youtube");
    goto free_resources;
  }

  if (xmlStrcmp (node->name, (const xmlChar *) "feed")) {
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_BROWSE_FAILED,
			  "Unexpected response from Youtube: no feed");
    goto free_resources;
  }

  node = node->xmlChildrenNode;
  if (!node) {
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_BROWSE_FAILED,
			  "Unexpected response from Youtube: empty feed");
    goto free_resources;
  }

  /* Compute # of items to send only when we execute the
     first chunk of the query (os->chained_chunk == FALSE) */
  if (!os->chained_chunk) {
    os->chained_chunk = TRUE;
    total_results = 0;
    while (node && !total_results) {
      if (!xmlStrcmp (node->name, (const xmlChar *) "entry")) {
	break;
      } else {
	ns = node->ns;
	if (ns && !xmlStrcmp (ns->prefix, (xmlChar *) "openSearch")) {
	  if (!xmlStrcmp (node->name, (const xmlChar *) "totalResults")) {
	    gchar *total_results_str =
	      (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
	    total_results = atoi (total_results_str);
	    g_free (total_results_str);
	  }
	}
      }
      node = node->next;
    }

    if (total_results >= os->skip + os->count) {
      /* Ok, we can send all the elements requested, no problem */
    } else if (total_results > os->skip) {
      /* We cannot send all, there aren't so many: adjust os->count
	 so it represents the total available */
      os->count = total_results - os->skip;
    } else {
      /* No results to send */
      os->callback (os->source,
                    os->operation_id,
                    NULL,
                    0,
                    os->user_data,
                    NULL);
      goto free_resources;
    }
  }

  /* Now go for the entries */
  ParseEntriesIdle *pei = g_new0 (ParseEntriesIdle, 1);
  pei->os = os;
  pei->node = node;
  pei->doc = doc;
  g_idle_add (parse_entries_idle, pei);
  return;

 free_resources:
  xmlFreeDoc (doc);
  return;
}

static GrlMedia *
parse_metadata_entry (GrlMediaSourceMetadataSpec *os,
		      xmlDocPtr doc,
		      xmlNodePtr node,
		      GError **error)
{
  xmlNs *ns;
  guint total_results = 0;
  GrlMedia *media = NULL;

  /* First checkout search information looking for totalResults */
  while (node && !total_results) {
    if (!xmlStrcmp (node->name, (const xmlChar *) "entry")) {
      break;
    } else {
      ns = node->ns;
      if (ns && !xmlStrcmp (ns->prefix, (xmlChar *) "openSearch")) {
	if (!xmlStrcmp (node->name, (const xmlChar *) "totalResults")) {
	  gchar *total_results_str =
	    (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
	  total_results = atoi (total_results_str);
	  g_free (total_results_str);
	}
      }
    }
    node = node->next;
  }

  if (total_results <= 0) {
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_MEDIA_NOT_FOUND,
			  "Could not find requested media");
    return NULL;
  }

  while (node && xmlStrcmp (node->name, (const xmlChar *) "entry")) {
    node = node->next;
  }
  if (node) {
    const gchar *target_id = grl_media_get_id (os->media);
    Entry *entry = g_new0 (Entry, 1);
    parse_entry (doc, node, entry);
    if (0) print_entry (entry);
    if (!strcmp (entry->id, target_id)) {
      build_media_from_entry (os->media, entry, os->keys);
    } else {
      /* FIXME: The query using the video id at target text resulted in various
	 matches and the one we were looking for was not the first one */
      g_warning ("Metadata query did not match expected target");
    }
    free_entry (entry);
  }

  return media;
}

static void
parse_metadata_feed (GrlMediaSourceMetadataSpec *os,
		     const gchar *str,
		     GError **error)
{
  xmlDocPtr doc;
  xmlNodePtr node;

  doc = xmlRecoverDoc ((xmlChar *) str);
  if (!doc) {
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_METADATA_FAILED,
			  "Failed to parse Youtube's response");
    goto free_resources;
  }

  node = xmlDocGetRootElement (doc);
  if (!node) {
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_METADATA_FAILED,
			  "Empty response from Youtube");
    goto free_resources;
  }

  if (xmlStrcmp (node->name, (const xmlChar *) "feed")) {
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_METADATA_FAILED,
			  "Unexpected response from Youtube: no feed");
    goto free_resources;
  }

  node = node->xmlChildrenNode;
  if (!node) {
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_METADATA_FAILED,
			  "Unexpected response from Youtube: empty feed");
    goto free_resources;
  }

  parse_metadata_entry (os, doc, node, error);

 free_resources:
  xmlFreeDoc (doc);
}

static void
parse_categories (xmlDocPtr doc, xmlNodePtr node)
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
    cat_info->url = g_strdup_printf (YOUTUBE_CATEGORY_URL,
				     id, "%d", "%d");
    all = g_list_prepend (all, cat_info);
    g_free (id);
    node = node->next;
    total++;
    g_debug ("  Found category: '%d - %s'", index++, cat_info->name);
  }

  if (all) {
    categories_dir_size = total;
    categories_dir = g_new0 (CategoryInfo, total + 1);
    iter = all;
    do {
      cat_info = (CategoryInfo *) iter->data;
      categories_dir[total - 1].id = cat_info->id ;
      categories_dir[total - 1].name = cat_info->name;
      categories_dir[total - 1].url = cat_info->url;
      total--;
      g_free (cat_info);
      iter = g_list_next (iter);
    } while (iter);
    g_list_free (all);
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

  parse_categories (doc, node);

 free_resources:
  xmlFreeDoc (doc);
  return;
}

static void
build_categories_directory (void)
{
  read_url_async (YOUTUBE_CATEGORIES_URL,
                  build_categories_directory_read_cb,
                  NULL);
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

static const gchar *
get_container_url (const gchar *container_id)
{
  g_debug ("get_container_url (%s)", container_id);

  guint index;
  CategoryInfo *directory;

  if (is_category_container (container_id)) {
    directory = categories_dir;
  } else if (is_feeds_container (container_id)) {
    directory = feeds_dir;
  } else {
    g_warning ("Cannot get URL for container id '%s'", container_id);
    return NULL;
  }

  if (directory == NULL) {
    g_critical ("Required directory is not initialized");
    return NULL;
  }

  index = 0;
  while (directory[index].id &&
	 strcmp (container_id, directory[index].id)) {
    index++;
  }
  if (directory[index].id) {
    return directory[index].url;
  } else {
    return NULL;
  }
}

static void
set_category_childcount (GrlMediaBox *content,
                         CategoryInfo *dir,
                         guint index)
{
  gint childcount;
  gboolean set_childcount = TRUE;

  if (dir == NULL) {
    /* Special case: we want childcount of root category */
    childcount = root_dir_size;
  } else if (!strcmp (dir[index].id, YOUTUBE_FEEDS_ID)) {
    childcount = feeds_dir_size;
  } else if (!strcmp (dir[index].id, YOUTUBE_CATEGORIES_ID)) {
    childcount = categories_dir_size;
  } else {
    const gchar *_url = get_container_url (dir[index].id);
    if (_url) {
      gchar *url = g_strdup_printf (_url, 1, 0);
      gchar  *xmldata = read_url (url);
      g_free (url);
      if (!xmldata) {
	g_warning ("Failed to connect to Youtube to get category childcount");
	return;
      }
      gchar *start = strstr (xmldata, YOUTUBE_TOTAL_RESULTS_START) +
	strlen (YOUTUBE_TOTAL_RESULTS_START);
      gchar *end = strstr (start, YOUTUBE_TOTAL_RESULTS_END);
      gchar *childcount_str = g_strndup (start, end - start);
      childcount = strtol (childcount_str, (char **) NULL, 10);
    } else {
      set_childcount = FALSE;
    }
  }

  if (set_childcount) {
    grl_media_box_set_childcount (content, childcount);
  }
}

static GrlMedia *
produce_container_from_directory (GrlMedia *media,
				  CategoryInfo *dir,
				  guint index,
				  gboolean set_childcount)
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
  if (set_childcount) {
    set_category_childcount (GRL_MEDIA_BOX (content), dir, index);
  }

  return content;
}

static GrlMedia *
produce_container_from_directory_by_id (GrlMedia *media,
					CategoryInfo *dir,
					const gchar *id,
					gboolean set_childcount)
{
  GrlMedia *content;
  guint index = 0;

  while (dir[index].id && strcmp (dir[index].id, id)) index++;
  if (dir[index].id) {
    content = produce_container_from_directory (media, dir,
						index, set_childcount);
  } else {
    /* If don't have that entry, return the media as it was
       provided (if media was NULL, we return NULL) */
    content = media;
  }

  return content;
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

static gboolean
produce_from_directory_idle (gpointer user_data)
{
  GrlMedia *content;
  ProduceFromDirectoryIdle *pfdi = (ProduceFromDirectoryIdle *) user_data;

  if (!pfdi->os->cancelled) {
    content = produce_container_from_directory (NULL,
						pfdi->directory,
						pfdi->index,
						pfdi->set_childcount);
    pfdi->remaining--;
    pfdi->index++;
  } else {
    content = NULL;
    pfdi->remaining = 0;
  }

  pfdi->os->callback (pfdi->os->source,
		      pfdi->os->operation_id,
		      content,
		      pfdi->remaining,
		      pfdi->os->user_data,
		      NULL);

  if (pfdi->remaining == 0) {
    free_operation_spec (pfdi->os);
    g_free (pfdi);
    return FALSE;
  }

  return TRUE;
}

static void
produce_from_directory (CategoryInfo *dir, guint dir_size, OperationSpec *os)
{
  g_debug ("produce_from_directory");

  guint index, remaining;
  gboolean set_childcount;
  YoutubeMediaType media_type;

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
    /* Do not compute childcount when it is expensive and user requested
       GRL_RESOLVE_FAST_ONLY */
    media_type = classify_media_id (os->container_id);
    if ((os->flags & GRL_RESOLVE_FAST_ONLY) &&
	(media_type == YOUTUBE_MEDIA_TYPE_CATEGORIES ||
	 media_type == YOUTUBE_MEDIA_TYPE_FEEDS)) {
      set_childcount = FALSE;
    } else {
      set_childcount =
	(g_list_find (os->keys,
		      GRLKEYID_TO_POINTER (GRL_METADATA_KEY_CHILDCOUNT)) != NULL);
    }
    index = os->skip;
    remaining = MIN (dir_size - os->skip, os->count);

    /* We use the idle loop because computing the childcount is blocking
       and it may be called for every entry in the directory */
    ProduceFromDirectoryIdle *pfdi = g_new0 (ProduceFromDirectoryIdle, 1);
    pfdi->os = os;
    pfdi->directory = dir;
    pfdi->index = index;
    pfdi->remaining = remaining;
    pfdi->set_childcount = set_childcount;
    g_idle_add (produce_from_directory_idle, pfdi);
  }
}

static void
produce_next_video_chunk_read_cb (gchar *xmldata, gpointer user_data)
{
  GError *error = NULL;
  OperationSpec *os = (OperationSpec *) user_data;

  if (!xmldata) {
    error = g_error_new (GRL_ERROR,
			 GRL_ERROR_BROWSE_FAILED,
			 "Failed to read from Youtube");
  } else {
    parse_feed (os, xmldata, &error);
    g_free (xmldata);
  }

  if (error) {
    os->callback (os->source,
		  os->operation_id,
		  NULL,
		  0,
		  os->user_data,
		  error);
    g_error_free (error);
    free_operation_spec (os);
  }
}

static void
produce_next_video_chunk (OperationSpec *os)
{
  gchar *url;

  g_debug ("produce_next_video_chunk");

  if (os->count > YOUTUBE_MAX_CHUNK) {
    url = g_strdup_printf (os->query_url, os->skip + 1, YOUTUBE_MAX_CHUNK);
  } else {
    url = g_strdup_printf (os->query_url, os->skip + 1, os->count);
  }
  read_url_async (url, produce_next_video_chunk_read_cb, os);
  g_free (url);
}

static void
produce_videos_from_container (OperationSpec *os, GError **error)
{
  const gchar *_url;

  _url = get_container_url (os->container_id);
  if (!_url) {
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_BROWSE_FAILED,
			  "Invalid container-id: '%s'",
			  os->container_id);
  } else {
    os->query_url = g_strdup (_url);
    produce_next_video_chunk (os);
  }
}

static void
produce_videos_from_search (const gchar *text,
			    OperationSpec *os,
			    GError **error)
{
  gchar *url;
  url = g_strdup_printf (YOUTUBE_SEARCH_URL, text, "%d", "%d");
  os->query_url = url;
  produce_next_video_chunk (os);
}

static void
metadata_read_cb (gchar *xmldata, gpointer user_data)
{
  GError *error = NULL;
  GrlMediaSourceMetadataSpec *ms = (GrlMediaSourceMetadataSpec *) user_data;

  if (!xmldata) {
    error = g_error_new (GRL_ERROR,
			 GRL_ERROR_METADATA_FAILED,
			 "Failed to read from Youtube");
  } else {
    parse_metadata_feed (ms, xmldata, &error);
    g_free (xmldata);
  }

  if (error) {
    ms->callback (ms->source, ms->media, ms->user_data, error);
    g_error_free (error);
  } else {
    ms->callback (ms->source, ms->media, ms->user_data, NULL);
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
                                      GRL_METADATA_KEY_AUTHOR,
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
grl_youtube_source_browse (GrlMediaSource *source,
                           GrlMediaSourceBrowseSpec *bs)
{
  OperationSpec *os;
  GError *error = NULL;
  const gchar *container_id;

  g_debug ("grl_youtube_source_browse");

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

  grl_media_source_set_operation_data (source, os->operation_id, os);

  if (!container_id) {
    produce_from_directory (root_dir, root_dir_size, os);
  } else if (!strcmp (container_id, YOUTUBE_FEEDS_ID)) {
    produce_from_directory (feeds_dir, feeds_dir_size, os);
  } else if (!strcmp (container_id, YOUTUBE_CATEGORIES_ID)) {
    produce_from_directory (categories_dir, categories_dir_size, os);
  } else {
    produce_videos_from_container (os, &error);
    if (error) {
      os->callback (os->source, os->operation_id, NULL,
		    0, os->user_data, error);
      g_error_free (error);
      free_operation_spec (os);
    }
  }
}

static void
grl_youtube_source_search (GrlMediaSource *source,
                           GrlMediaSourceSearchSpec *ss)
{
  OperationSpec *os;
  GError *error = NULL;

  g_debug ("grl_youtube_source_search");

  os = g_new0 (OperationSpec, 1);
  os->source = source;
  os->operation_id = ss->search_id;
  os->keys = ss->keys;
  os->skip = ss->skip;
  os->count = ss->count;
  os->callback = ss->callback;
  os->user_data = ss->user_data;

  grl_media_source_set_operation_data (source, os->operation_id, os);
  produce_videos_from_search (ss->text, os, &error);

  if (error) {
    error->code = GRL_ERROR_SEARCH_FAILED;
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
    g_free (os->query_url);
    g_free (os);
  }
}

static void
grl_youtube_source_metadata (GrlMediaSource *source,
                             GrlMediaSourceMetadataSpec *ms)
{
  gchar *url;
  GError *error = NULL;
  GrlMedia *media = NULL;
  YoutubeMediaType media_type;
  gboolean set_childcount;
  const gchar *id;

  g_debug ("grl_youtube_source_metadata");

  id = grl_media_get_id (ms->media);
  media_type = classify_media_id (id);

  /* Do not compute childcount for expensive categories
     if user requested that */
  if ((ms->flags & GRL_RESOLVE_FAST_ONLY) &&
      (media_type == YOUTUBE_MEDIA_TYPE_CATEGORIES ||
       media_type == YOUTUBE_MEDIA_TYPE_FEEDS)) {
    set_childcount = FALSE;
  } else {
    set_childcount =
      (g_list_find (ms->keys,
		    GRLKEYID_TO_POINTER (GRL_METADATA_KEY_CHILDCOUNT)) != NULL);
  }

  switch (media_type) {
  case YOUTUBE_MEDIA_TYPE_ROOT:
    media = produce_container_from_directory (ms->media, NULL,
					      0, set_childcount);
    break;
  case YOUTUBE_MEDIA_TYPE_FEEDS:
    media = produce_container_from_directory (ms->media, root_dir,
					      0, set_childcount);
    break;
  case YOUTUBE_MEDIA_TYPE_CATEGORIES:
    media = produce_container_from_directory (ms->media, root_dir,
					      1, set_childcount);
    break;
  case YOUTUBE_MEDIA_TYPE_FEED:
    media = produce_container_from_directory_by_id (ms->media,
						    feeds_dir,
						    id,
						    set_childcount);
    break;
  case YOUTUBE_MEDIA_TYPE_CATEGORY:
    media = produce_container_from_directory_by_id (ms->media,
						    categories_dir,
						    id,
						    set_childcount);
    break;
  case YOUTUBE_MEDIA_TYPE_VIDEO:
  default:
    {
      /* For metadata retrieval we just search by text using the video ID */
      /* This case is async, we will emit results in the read callback */
      url = g_strdup_printf (YOUTUBE_SEARCH_URL, id, "1", "1");
      read_url_async (url, metadata_read_cb, ms);
      g_free (url);
    }
    break;
  }

  if (error) {
    ms->callback (ms->source, ms->media, ms->user_data, error);
    g_error_free (error);
  } else if (media) {
    ms->callback (ms->source, ms->media, ms->user_data, NULL);
  }
}

static void
grl_youtube_source_cancel (GrlMediaSource *source, guint operation_id)
{
  OperationSpec *spec;
  g_debug ("grl_youtube_source_cancel");
  spec = (OperationSpec *) grl_media_source_get_operation_data (source,
                                                                operation_id);
  if (spec) {
    spec->cancelled = TRUE;
  }
}
