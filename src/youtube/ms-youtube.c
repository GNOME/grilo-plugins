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

#include <media-store.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <string.h>
#include <stdlib.h>

#include "ms-youtube.h"

/* --------- Logging  -------- */ 

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "ms-youtube"

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
#define YOUTUBE_SEARCH_URL      "http://gdata.youtube.com/feeds/api/videos?vq=%s&start-index=%d&max-results=%d"
#define YOUTUBE_CATEGORY_URL    "http://gdata.youtube.com/feeds/api/videos/-/%s?&start-index=%s&max-results=%s"

/* --- Youtube parsing hints ---- */

#define YOUTUBE_TOTAL_RESULTS_START "<openSearch:totalResults>"
#define YOUTUBE_TOTAL_RESULTS_END   "</openSearch:totalResults>"

/* --- Plugin information --- */

#define PLUGIN_ID   "ms-youtube"
#define PLUGIN_NAME "Youtube"
#define PLUGIN_DESC "A plugin for browsing and searching Youtube videos"

#define SOURCE_ID   "ms-youtube"
#define SOURCE_NAME "Youtube"
#define SOURCE_DESC "A source for browsing and searching Youtube videos"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

typedef struct {
  gchar *id;
  gchar *title;
  gchar *published;
  gchar *author;
  gchar *thumbnail;
  gchar *description;
  gchar *duration;
  gboolean restricted;
} Entry; 

typedef struct {
  MsMediaSource *source;
  guint operation_id;
  const GList *keys;
  guint skip;
  guint count;
  MsMediaSourceResultCb callback;
  gpointer user_data;
} OperationSpec;

typedef struct {
  MsMetadataSource *source;
  const GList *keys;
  MsMediaSourceMetadataCb callback;
  gpointer user_data;
} MetadataOperationSpec;

typedef struct {
  OperationSpec *os;
  xmlNodePtr node;
  xmlDocPtr doc;
  guint total_results;
  guint index;
} ParseEntriesIdle;

typedef struct {
  gchar *id;
  gchar *name;
  gchar *url;
} CategoryInfo;

typedef enum {
  YOUTUBE_MEDIA_TYPE_ROOT,
  YOUTUBE_MEDIA_TYPE_FEEDS,
  YOUTUBE_MEDIA_TYPE_CATEGORIES,
  YOUTUBE_MEDIA_TYPE_FEED,
  YOUTUBE_MEDIA_TYPE_CATEGORY,
  YOUTUBE_MEDIA_TYPE_VIDEO,
} YoutubeMediaType;

static MsYoutubeSource *ms_youtube_source_new (void);

gboolean ms_youtube_plugin_init (MsPluginRegistry *registry,
				 const MsPluginInfo *plugin);

static const GList *ms_youtube_source_supported_keys (MsMetadataSource *source);

static const GList *ms_youtube_source_slow_keys (MsMetadataSource *source);

static void ms_youtube_source_metadata (MsMediaSource *source,
					MsMediaSourceMetadataSpec *ms);

static void ms_youtube_source_search (MsMediaSource *source,
				      MsMediaSourceSearchSpec *ss);

static void ms_youtube_source_browse (MsMediaSource *source,
				      MsMediaSourceBrowseSpec *bs);

static gchar *read_url (const gchar *url);

static void build_categories_directory (void);

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
ms_youtube_plugin_init (MsPluginRegistry *registry, const MsPluginInfo *plugin)
{
  g_debug ("youtube_plugin_init\n");

  MsYoutubeSource *source = ms_youtube_source_new ();
  ms_plugin_registry_register_source (registry, plugin, MS_MEDIA_PLUGIN (source));
  return TRUE;
}

MS_PLUGIN_REGISTER (ms_youtube_plugin_init, 
                    NULL, 
                    PLUGIN_ID,
                    PLUGIN_NAME, 
                    PLUGIN_DESC, 
                    PACKAGE_VERSION,
                    AUTHOR, 
                    LICENSE, 
                    SITE);

/* ================== Youtube GObject ================ */

static MsYoutubeSource *
ms_youtube_source_new (void)
{
  g_debug ("ms_youtube_source_new");
  return g_object_new (MS_YOUTUBE_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       NULL);
}

static void
ms_youtube_source_class_init (MsYoutubeSourceClass * klass)
{
  MsMediaSourceClass *source_class = MS_MEDIA_SOURCE_CLASS (klass);
  MsMetadataSourceClass *metadata_class = MS_METADATA_SOURCE_CLASS (klass);
  source_class->browse = ms_youtube_source_browse;
  source_class->search = ms_youtube_source_search;
  source_class->metadata = ms_youtube_source_metadata;
  metadata_class->supported_keys = ms_youtube_source_supported_keys;
  metadata_class->slow_keys = ms_youtube_source_slow_keys;

  if (!gnome_vfs_init ()) {
    g_error ("Failed to initialize Gnome VFS");
  }
}

static void
ms_youtube_source_init (MsYoutubeSource *source)
{
  if (!categories_dir) {
    build_categories_directory ();
  }
}

G_DEFINE_TYPE (MsYoutubeSource, ms_youtube_source, MS_TYPE_MEDIA_SOURCE);

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
  g_free (entry);
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

static gchar *
read_url (const gchar *url)
{
  gchar buffer[1025];
  GnomeVFSFileSize bytes_read;
  GnomeVFSResult r;
  GString *data;
  GnomeVFSHandle *fh;

  /* Open URL */
  g_debug ("Opening '%s'", url);
  r = gnome_vfs_open (&fh, url, GNOME_VFS_OPEN_READ);
  if (r != GNOME_VFS_OK) {
    g_warning ("Failed to open '%s' - %d", url, r);
    return NULL;
  }

  /* Read URL contents */
  g_debug ("Reading data from '%s'", url);
  data = g_string_new ("");
  do {
    gnome_vfs_read (fh, buffer, 1024, &bytes_read);
    buffer[bytes_read] = '\0';
    g_string_append (data, buffer);
  } while (bytes_read > 0);
  g_debug ("  Done reading data from url");

  gnome_vfs_close (fh);

  return g_string_free (data, FALSE);
}

static MsContent *
build_media_from_entry (const Entry *entry, const GList *keys)
{
  MsContentMedia *media;
  gchar *url;
  GList *iter;

  media = ms_content_media_new (); 

  iter = (GList *) keys;
  while (iter) {
    MsKeyID key_id = GPOINTER_TO_UINT (iter->data);
    switch (key_id) {
    case MS_METADATA_KEY_ID:
      ms_content_media_set_id (media, entry->id);
      break;
    case MS_METADATA_KEY_TITLE:
      ms_content_media_set_title (media, entry->title);
      break;
    case MS_METADATA_KEY_AUTHOR:
      ms_content_media_set_author (media, entry->author);
      break;
    case MS_METADATA_KEY_DESCRIPTION:
      ms_content_media_set_description (media, entry->description);
      break;
    case MS_METADATA_KEY_THUMBNAIL:
      ms_content_media_set_thumbnail (media, entry->thumbnail);
      break;
    case MS_METADATA_KEY_DATE:
      ms_content_set_string (MS_CONTENT (media), 
			     MS_METADATA_KEY_DATE, entry->published);
      break;
    case MS_METADATA_KEY_DURATION:
      ms_content_set_string (MS_CONTENT (media), 
			     MS_METADATA_KEY_DURATION, entry->duration);
      break;
    case MS_METADATA_KEY_URL:
      if (!entry->restricted) {
	url = get_video_url (entry->id);
	if (url) {
	  ms_content_media_set_url (media, url);
	}
	g_free (url);
      }
      break;
    default:
      break;
    }
    iter = g_list_next (iter);
  }

  return MS_CONTENT (media);
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

  while (pei->node && xmlStrcmp (pei->node->name,
				 (const xmlChar *) "entry")) {
    pei->node = pei->node->next;
  }

  if (pei->node) {
    Entry *entry = g_new0 (Entry, 1);
    parse_entry (pei->doc, pei->node, entry);
    if (0) print_entry (entry);
    MsContent *media = build_media_from_entry (entry, pei->os->keys);
    free_entry (entry);
    
    pei->index++;
    pei->os->callback (pei->os->source,
		       pei->os->operation_id,
		       media,
		       pei->total_results - pei->index,
		       pei->os->user_data,
		       NULL);
    
    parse_more = TRUE;
    pei->node = pei->node->next;
  }

  if (!parse_more) {
    xmlFreeDoc (pei->doc);
    g_free (pei->os);
    g_free (pei);
  }
  
  return parse_more;
}

static void
parse_feed (OperationSpec *os, const gchar *str, GError **error)
{
  xmlDocPtr doc;
  xmlNodePtr node;
  guint total_results = 0;
  xmlNs *ns;
  
  doc = xmlRecoverDoc ((xmlChar *) str);
  if (!doc) {
    *error = g_error_new (MS_ERROR, 
			  MS_ERROR_BROWSE_FAILED,
			  "Failed to parse Youtube's response");
    goto free_resources;
  }

  node = xmlDocGetRootElement (doc);
  if (!node) {
    *error = g_error_new (MS_ERROR, 
			  MS_ERROR_BROWSE_FAILED,
			  "Empty response from Youtube");
    goto free_resources;
  }

  if (xmlStrcmp (node->name, (const xmlChar *) "feed")) {
    *error = g_error_new (MS_ERROR, 
			  MS_ERROR_BROWSE_FAILED,
			  "Unexpected response from Youtube: no feed");
    goto free_resources;
  }

  node = node->xmlChildrenNode;
  if (!node) {
    *error = g_error_new (MS_ERROR, 
			  MS_ERROR_BROWSE_FAILED,
			  "Unexpected response from Youtube: empty feed");
    goto free_resources;
  }

  /* checkout search information looking for totalResults */
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

  /* Compute # of items to send */
  if (total_results >= os->skip + os->count) {
    total_results = os->count;
  } else if (total_results > os->skip) {
    total_results -= os->skip;
  } else {
    /* No results to send */
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, NULL);
    goto free_resources;
  }

  /* Now go for the entries */
  ParseEntriesIdle *pei = g_new (ParseEntriesIdle, 1);
  pei->os = os;
  pei->node = node;
  pei->doc = doc;
  pei->total_results = total_results;
  pei->index = 0;
  g_idle_add (parse_entries_idle, pei);
  return;
  
 free_resources:
  xmlFreeDoc (doc);
  return;
}

static MsContent *
parse_metadata_entry (MsMediaSourceMetadataSpec *os,
		      xmlDocPtr doc,
		      xmlNodePtr node,
		      GError **error)
{
  xmlNs *ns;
  guint total_results = 0;
  MsContent *media = NULL;

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

  /* Should have exactly 1 result */
  if (total_results != 1) {
    *error = g_error_new (MS_ERROR, 
			  MS_ERROR_MEDIA_NOT_FOUND,
			  "Could not find requested media");
    return NULL;
  }

  /* Now go for the entry data */
  while (node && xmlStrcmp (node->name, (const xmlChar *) "entry")) {
    node = node->next;
  }
  if (node) {
    Entry *entry = g_new0 (Entry, 1);
    parse_entry (doc, node, entry);    
    if (0) print_entry (entry);
    media = build_media_from_entry (entry, os->keys);    
    free_entry (entry);
  } 

  return media;
}

static MsContent *
parse_metadata_feed (MsMediaSourceMetadataSpec *os,
		     const gchar *str,
		     GError **error)
{
  xmlDocPtr doc;
  xmlNodePtr node;
  MsContent *media = NULL;
  
  doc = xmlRecoverDoc ((xmlChar *) str);
  if (!doc) {
    *error = g_error_new (MS_ERROR, 
			  MS_ERROR_METADATA_FAILED,
			  "Failed to parse Youtube's response");
    goto free_resources;
  }

  node = xmlDocGetRootElement (doc);
  if (!node) {
    *error = g_error_new (MS_ERROR, 
			  MS_ERROR_METADATA_FAILED,
			  "Empty response from Youtube");
    goto free_resources;
  }

  if (xmlStrcmp (node->name, (const xmlChar *) "feed")) {
    *error = g_error_new (MS_ERROR, 
			  MS_ERROR_METADATA_FAILED,
			  "Unexpected response from Youtube: no feed");
    goto free_resources;
  }

  node = node->xmlChildrenNode;
  if (!node) {
    *error = g_error_new (MS_ERROR, 
			  MS_ERROR_METADATA_FAILED,
			  "Unexpected response from Youtube: empty feed");
    goto free_resources;
  }

  media = parse_metadata_entry (os, doc, node, error);
  
 free_resources:
  xmlFreeDoc (doc);
  return media;
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
build_categories_directory (void)
{
  gchar *xmldata;
  xmlDocPtr doc;
  xmlNodePtr node;

  xmldata = read_url (YOUTUBE_CATEGORIES_URL);
  if (!xmldata) {
    g_warning ("Failed to build category directory (1)");
    return;
  };
  
  doc = xmlRecoverDoc ((xmlChar *) xmldata);
  if (!doc) {
    g_warning ("Failed to build category directory (2)");
    goto free_resources;
  }

  node = xmlDocGetRootElement (doc);
  if (!node) {
    g_warning ("Failed to build category directory (3)");
    goto free_resources;
  }

  if (xmlStrcmp (node->name, (const xmlChar *) "categories")) {
    g_warning ("Failed to build category directory (4)");
    goto free_resources;
  }

  node = node->xmlChildrenNode;
  if (!node) {
    g_warning ("Failed to build category directory (5)");
    goto free_resources;
  }

  parse_categories (doc, node);
  
 free_resources:
  xmlFreeDoc (doc);
  return;
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
set_category_childcount (MsContent *content, CategoryInfo *dir, guint index)
{
  gint childcount;

  if (dir == NULL) {
    /* Special case: we want childcount of root category */
    childcount = root_dir_size;
  } else if (!strcmp (dir[index].id, YOUTUBE_FEEDS_ID)) {
    childcount = feeds_dir_size;
  } else if (!strcmp (dir[index].id, YOUTUBE_CATEGORIES_ID)) {
    childcount = categories_dir_size;
  } else {
    const gchar *_url = get_container_url (dir[index].id);
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
  }

  ms_content_set_int (content, MS_METADATA_KEY_CHILDCOUNT, childcount);
}

static MsContent *
produce_container_from_directory (CategoryInfo *dir,
				  guint index,
				  gboolean set_childcount)
{
  MsContentMedia *content;

  content = ms_content_media_container_new ();
  if (!dir) {
    ms_content_media_set_id (content, NULL);
    ms_content_media_set_title (content, YOUTUBE_ROOT_NAME);
  } else {
    ms_content_media_set_id (content, dir[index].id);
    ms_content_media_set_title (content, dir[index].name);
  }
  if (set_childcount) {
    set_category_childcount (MS_CONTENT (content), dir, index);
  }

  return MS_CONTENT (content);
}

static MsContent *
produce_container_from_directory_by_id (CategoryInfo *dir,
					const gchar *id,
					gboolean set_childcount)
{
  MsContent *content;
  guint index = 0;

  while (dir[index].id && strcmp (dir[index].id, id)) index++;
  if (dir[index].id) {
    content = produce_container_from_directory (dir, index, set_childcount);
  } else {
    content = NULL;
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

static void
produce_from_directory (CategoryInfo *dir,
			guint dir_size,
			MsMediaSourceBrowseSpec *bs)
{
  g_debug ("produce_from_directory");

  guint index, remaining;
  gboolean set_childcount;
  YoutubeMediaType media_type;

  if (bs->skip >= dir_size) {
    /* No results */
    bs->callback (bs->source,
		  bs->browse_id,
		  NULL,
		  0,
		  bs->user_data,
		  NULL);        
  } else {
    /* Do not compute childcount when it is expensive and user requested
       MS_RESOLVE_FAST_ONLY */
    media_type = classify_media_id (bs->container_id);
    if ((bs->flags & MS_RESOLVE_FAST_ONLY) && 
	(media_type == YOUTUBE_MEDIA_TYPE_CATEGORIES ||
	 media_type == YOUTUBE_MEDIA_TYPE_FEEDS)) {
      set_childcount = FALSE;
    } else {
      set_childcount =
	(g_list_find (bs->keys,
		      GUINT_TO_POINTER (MS_METADATA_KEY_CHILDCOUNT)) != NULL);
    }
    index = bs->skip;
    remaining = MIN (dir_size - bs->skip, bs->count);
    do {
      MsContent *content =
	produce_container_from_directory (dir, index, set_childcount);
      bs->callback (bs->source,
		    bs->browse_id,
		    content,
		    --remaining,
		    bs->user_data,
		    NULL);    
      index++;
    } while (remaining > 0);
  }
}

/* ================== API Implementation ================ */

static const GList *
ms_youtube_source_supported_keys (MsMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = ms_metadata_key_list_new (MS_METADATA_KEY_TITLE, 
				     MS_METADATA_KEY_URL,
				     MS_METADATA_KEY_AUTHOR,
				     MS_METADATA_KEY_DESCRIPTION,
				     MS_METADATA_KEY_DURATION,
				     MS_METADATA_KEY_DATE,
				     MS_METADATA_KEY_THUMBNAIL,
				     MS_METADATA_KEY_CHILDCOUNT,
				     NULL);
  }
  return keys;
}

static const GList *
ms_youtube_source_slow_keys (MsMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    /* childcount may or may not be slow depending on the category, 
       so we handle it as a non-slow key and then we decide if we
       resolve or not depending on the category and the flags set */
    keys = ms_metadata_key_list_new (MS_METADATA_KEY_URL,
				     NULL);
  }
  return keys;  
}

static void
ms_youtube_source_browse (MsMediaSource *source, MsMediaSourceBrowseSpec *bs)
{
  gchar *xmldata, *url;
  const gchar *_url;
  GError *error = NULL;
  OperationSpec *os;

  g_debug ("ms_youtube_source_browse (%s)", bs->container_id);

  if (!bs->container_id) {
    produce_from_directory (root_dir, root_dir_size, bs);
  } else if (!strcmp (bs->container_id, YOUTUBE_FEEDS_ID)) {
    produce_from_directory (feeds_dir, feeds_dir_size, bs);
  } else if (!strcmp (bs->container_id, YOUTUBE_CATEGORIES_ID)) {
    produce_from_directory (categories_dir, categories_dir_size, bs);
  } else {
    _url = get_container_url (bs->container_id);
    if (!_url) {
      GError *error = g_error_new (MS_ERROR,
				   MS_ERROR_BROWSE_FAILED,
				   "Invalid container-id: '%s'",
				   bs->container_id);
      bs->callback (source, bs->browse_id, NULL, 0, bs->user_data, error);
      g_error_free (error);
      return;
    }

    url = g_strdup_printf (_url, bs->skip + 1, bs->count);
    xmldata = read_url (url);
    g_free (url);
    
    if (!xmldata) {
      GError *error = g_error_new (MS_ERROR,
				   MS_ERROR_BROWSE_FAILED,
				   "Failed to connect to Youtube");
      bs->callback (source, bs->browse_id, NULL, 0, bs->user_data, error);
      g_error_free (error);
      return;
    }
    
    os = g_new0 (OperationSpec, 1);
    os->source = bs->source;
    os->operation_id = bs->browse_id;
    os->keys = bs->keys;
    os->skip = bs->skip;
    os->count = bs->count;
    os->callback = bs->callback;
    os->user_data = bs->user_data;
    
    parse_feed (os, xmldata, &error);
    g_free (xmldata);
    
    if (error) {
      os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
      g_error_free (error);
      g_free (os);
    }
  }
}

static void
ms_youtube_source_search (MsMediaSource *source, MsMediaSourceSearchSpec *ss)
{
  gchar *xmldata, *url;
  OperationSpec *os;
  GError *error = NULL;

  g_debug ("ms_youtube_source_search");

  if (ss->filter) {
    g_warning ("Search filter not supported, ignoring filter argument");
  }

  url = g_strdup_printf (YOUTUBE_SEARCH_URL, ss->text, ss->skip + 1, ss->count);
  xmldata = read_url (url);
  g_free (url);

  if (!xmldata) {
    GError *error = g_error_new (MS_ERROR,
				 MS_ERROR_SEARCH_FAILED,
				 "Failed to connect to Youtube");
    ss->callback (source, ss->search_id, NULL, 0, ss->user_data, error);    
    g_error_free (error);
    return;
  }

  os = g_new0 (OperationSpec, 1);
  os->source = source;
  os->operation_id = ss->search_id;
  os->keys = ss->keys;
  os->skip = ss->skip;
  os->count = ss->count;
  os->callback = ss->callback;
  os->user_data = ss->user_data;

  parse_feed (os, xmldata, &error);
  g_free (xmldata);

  if (error) {
    error->code = MS_ERROR_SEARCH_FAILED;
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
    g_free (os);
  }
}

static void
ms_youtube_source_metadata (MsMediaSource *source,
			    MsMediaSourceMetadataSpec *ms)
{
  gchar *xmldata, *url;
  GError *error = NULL;
  MsContent *media;
  YoutubeMediaType media_type;
  gboolean set_childcount;

  g_debug ("ms_youtube_source_metadata");

  media_type = classify_media_id (ms->object_id);

  /* Do not compute childcount for expensive categories
     if user requested that */
  if ((ms->flags & MS_RESOLVE_FAST_ONLY) && 
      (media_type == YOUTUBE_MEDIA_TYPE_CATEGORIES ||
       media_type == YOUTUBE_MEDIA_TYPE_FEEDS)) {
    set_childcount = FALSE;
  } else {
    set_childcount =
      (g_list_find (ms->keys,
		    GUINT_TO_POINTER (MS_METADATA_KEY_CHILDCOUNT)) != NULL);
  }

  switch (media_type) {
  case YOUTUBE_MEDIA_TYPE_ROOT:
    media = produce_container_from_directory (NULL, 0, set_childcount);
    break;
  case YOUTUBE_MEDIA_TYPE_FEEDS:
    media = produce_container_from_directory (root_dir, 0, set_childcount);
    break;
  case YOUTUBE_MEDIA_TYPE_CATEGORIES:
    media = produce_container_from_directory (root_dir, 1, set_childcount);
    break;
  case YOUTUBE_MEDIA_TYPE_FEED:
    media = produce_container_from_directory_by_id (feeds_dir,
						    ms->object_id,
						    set_childcount);
    break;
  case YOUTUBE_MEDIA_TYPE_CATEGORY:
    media = produce_container_from_directory_by_id (categories_dir,
						    ms->object_id,
						    set_childcount);
    break;
  case YOUTUBE_MEDIA_TYPE_VIDEO:
  default:
    {
      /* For metadata retrieval we just search by text using the video ID */
      url = g_strdup_printf (YOUTUBE_SEARCH_URL, ms->object_id, 1, 1);
      xmldata = read_url (url);
      g_free (url);
      
      if (!xmldata) {
	GError *error = g_error_new (MS_ERROR,
				     MS_ERROR_METADATA_FAILED,
				     "Failed to connect to Youtube");
	ms->callback (source, NULL, ms->user_data, error);    
	g_error_free (error);
	return;
      }
      
      media = parse_metadata_feed (ms, xmldata, &error);
      g_free (xmldata);   
    }
    break;
  }

  if (error) {
    ms->callback (ms->source, NULL, ms->user_data, error);
    g_error_free (error);
  } else {
    ms->callback (ms->source, media, ms->user_data, NULL);
  }
}
