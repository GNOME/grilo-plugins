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
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <string.h>
#include <stdlib.h>

#include "util/gnomevfs.h"
#include "grl-jamendo.h"

/* --------- Logging  -------- */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "grl-jamendo"

#define JAMENDO_ID_SEP    "/"
#define JAMENDO_ROOT_NAME "Jamendo"

/* ------- Categories ------- */

#define JAMENDO_ARTIST "artist"
#define JAMENDO_ALBUM  "album"
#define JAMENDO_TRACK  "track"

/* ---- Jamendo Web API  ---- */

#define JAMENDO_BASE_ENTRY "http://api.jamendo.com/get2"
#define JAMENDO_FORMAT     "xml"
#define JAMENDO_RANGE      "n=%u&pn=%u"

#define JAMENDO_ARTIST_ENTRY JAMENDO_BASE_ENTRY "/%s/" JAMENDO_ARTIST "/" JAMENDO_FORMAT

#define JAMENDO_ALBUM_ENTRY  JAMENDO_BASE_ENTRY "/%s/" JAMENDO_ALBUM  "/" JAMENDO_FORMAT \
  "/" JAMENDO_ALBUM "_" JAMENDO_ARTIST

#define JAMENDO_TRACK_ENTRY  JAMENDO_BASE_ENTRY "/%s/" JAMENDO_TRACK  "/" JAMENDO_FORMAT \
  "/" JAMENDO_ALBUM "_" JAMENDO_ARTIST "+" JAMENDO_TRACK "_" JAMENDO_ALBUM

#define JAMENDO_GET_ARTISTS JAMENDO_ARTIST_ENTRY "/?" JAMENDO_RANGE
#define JAMENDO_GET_ALBUMS  JAMENDO_ALBUM_ENTRY  "/?" JAMENDO_RANGE
#define JAMENDO_GET_TRACKS  JAMENDO_TRACK_ENTRY  "/?" JAMENDO_RANGE

#define JAMENDO_GET_ALBUMS_FROM_ARTIST JAMENDO_ALBUM_ENTRY "/?" JAMENDO_RANGE "&artist_id=%s"
#define JAMENDO_GET_TRACKS_FROM_ALBUM JAMENDO_TRACK_ENTRY  "/?" JAMENDO_RANGE "&album_id=%s"
#define JAMENDO_GET_ARTIST JAMENDO_ARTIST_ENTRY "/?id=%s"

#define JAMENDO_GET_ALBUM  JAMENDO_ALBUM_ENTRY  "/?id=%s"
#define JAMENDO_GET_TRACK  JAMENDO_TRACK_ENTRY  "/?id=%s"

#define JAMENDO_SEARCH_ARTIST JAMENDO_ARTIST_ENTRY "/?" JAMENDO_RANGE "&searchquery=%s"
#define JAMENDO_SEARCH_ALBUM  JAMENDO_ALBUM_ENTRY  "/?" JAMENDO_RANGE "&searchquery=%s"
#define JAMENDO_SEARCH_TRACK  JAMENDO_TRACK_ENTRY  "/?" JAMENDO_RANGE "&searchquery=%s"

/* --- Plugin information --- */

#define PLUGIN_ID   "grl-jamendo"
#define PLUGIN_NAME "Jamendo"
#define PLUGIN_DESC "A plugin for browsing and searching Jamendo videos"

#define SOURCE_ID   "grl-jamendo"
#define SOURCE_NAME "Jamendo"
#define SOURCE_DESC "A source for browsing and searching Jamendo videos"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

enum {
  METADATA,
  BROWSE,
  QUERY,
  SEARCH
};

typedef enum {
  JAMENDO_ARTIST_CAT = 1,
  JAMENDO_ALBUM_CAT,
  JAMENDO_TRACK_CAT
} JamendoCategory;

typedef struct {
  JamendoCategory category;
  gchar *id;
  gchar *artist_name;
  gchar *artist_genre;
  gchar *artist_url;
  gchar *artist_image;
  gchar *album_name;
  gchar *album_genre;
  gchar *album_url;
  gchar *album_duration;
  gchar *album_image;
  gchar *track_name;
  gchar *track_url;
  gchar *track_stream;
  gchar *track_duration;
} Entry;

typedef struct {
  gint type;
  union {
    GrlMediaSourceBrowseSpec *bs;
    GrlMediaSourceQuerySpec *qs;
    GrlMediaSourceMetadataSpec *ms;
    GrlMediaSourceSearchSpec *ss;
  } spec;
  xmlNodePtr node;
  xmlDocPtr doc;
  guint total_results;
  guint index;
  gboolean cancelled;
} XmlParseEntries;

static GrlJamendoSource *grl_jamendo_source_new (void);

gboolean grl_jamendo_plugin_init (GrlPluginRegistry *registry,
                                  const GrlPluginInfo *plugin,
                                  GList *configs);

static const GList *grl_jamendo_source_supported_keys (GrlMetadataSource *source);

static void grl_jamendo_source_metadata (GrlMediaSource *source,
                                         GrlMediaSourceMetadataSpec *ms);

static void grl_jamendo_source_browse (GrlMediaSource *source,
                                       GrlMediaSourceBrowseSpec *bs);

static void grl_jamendo_source_query (GrlMediaSource *source,
                                      GrlMediaSourceQuerySpec *qs);

static void grl_jamendo_source_search (GrlMediaSource *source,
                                       GrlMediaSourceSearchSpec *ss);

static void grl_jamendo_source_cancel (GrlMediaSource *source,
                                       guint operation_id);

/* =================== Jamendo Plugin  =============== */

gboolean
grl_jamendo_plugin_init (GrlPluginRegistry *registry,
                         const GrlPluginInfo *plugin,
                         GList *configs)
{
  g_debug ("jamendo_plugin_init\n");

  GrlJamendoSource *source = grl_jamendo_source_new ();
  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source));
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_jamendo_plugin_init,
                     NULL,
                     PLUGIN_ID,
                     PLUGIN_NAME,
                     PLUGIN_DESC,
                     PACKAGE_VERSION,
                     AUTHOR,
                     LICENSE,
                     SITE);

/* ================== Jamendo GObject ================ */

static GrlJamendoSource *
grl_jamendo_source_new (void)
{
  g_debug ("grl_jamendo_source_new");
  return g_object_new (GRL_JAMENDO_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       NULL);
}

static void
grl_jamendo_source_class_init (GrlJamendoSourceClass * klass)
{
  GrlMediaSourceClass *source_class = GRL_MEDIA_SOURCE_CLASS (klass);
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);
  source_class->metadata = grl_jamendo_source_metadata;
  source_class->browse = grl_jamendo_source_browse;
  source_class->query = grl_jamendo_source_query;
  source_class->search = grl_jamendo_source_search;
  source_class->cancel = grl_jamendo_source_cancel;
  metadata_class->supported_keys = grl_jamendo_source_supported_keys;
}

static void
grl_jamendo_source_init (GrlJamendoSource *source)
{
}

G_DEFINE_TYPE (GrlJamendoSource, grl_jamendo_source, GRL_TYPE_MEDIA_SOURCE);

/* ======================= Utilities ==================== */

#if 0
static void
print_entry (Entry *entry)
{
  g_print ("Entry Information:\n");
  g_print ("            ID: %s\n", entry->id);
  g_print ("   Artist Name: %s\n", entry->artist_name);
  g_print ("  Artist Genre: %s\n", entry->artist_genre);
  g_print ("    Artist URL: %s\n", entry->artist_url);
  g_print ("  Artist Image: %s\n", entry->artist_image);
  g_print ("    Album Name: %s\n", entry->album_name);
  g_print ("   Album Genre: %s\n", entry->album_genre);
  g_print ("     Album URL: %s\n", entry->album_url);
  g_print ("Album Duration: %s\n", entry->album_duration);
  g_print ("   Album Image: %s\n", entry->album_image);
  g_print ("    Track Name: %s\n", entry->track_name);
  g_print ("     Track URL: %s\n", entry->track_url);
  g_print ("  Track Stream: %s\n", entry->track_stream);
  g_print ("Track Duration: %s\n", entry->track_duration);
}
#endif

static void
free_entry (Entry *entry)
{
  g_free (entry->id);
  g_free (entry->artist_name);
  g_free (entry->artist_genre);
  g_free (entry->artist_url);
  g_free (entry->artist_image);
  g_free (entry->album_name);
  g_free (entry->album_genre);
  g_free (entry->album_url);
  g_free (entry->album_duration);
  g_free (entry->album_image);
  g_free (entry->track_name);
  g_free (entry->track_url);
  g_free (entry->track_stream);
  g_free (entry->track_duration);
  g_free (entry);
}

static gint
xml_count_children (xmlNodePtr node)
{
#if (LIBXML2_VERSION >= 20700)
  return xmlChildElementCount (node);
#else
  gint nchildren = 0;
  xmlNodePtr i = node->xmlChildrenNode;

  while (i) {
    nchildren++;
    i = i->next;
  }

  return nchildren;
#endif
}

static void
xml_parse_result (const gchar *str, GError **error, XmlParseEntries *xpe)
{
  xmlDocPtr doc;
  xmlNodePtr node;
  gint child_nodes = 0;

  doc = xmlRecoverDoc ((xmlChar *) str);
  if (!doc) {
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_BROWSE_FAILED,
			  "Failed to parse Jamendo's response");
    goto free_resources;
  }

  node = xmlDocGetRootElement (doc);
  if (!node) {
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_BROWSE_FAILED,
			  "Empty response from Jamendo");
    goto free_resources;
  }

  if (xmlStrcmp (node->name, (const xmlChar *) "data")) {
    *error = g_error_new (GRL_ERROR,
			  GRL_ERROR_BROWSE_FAILED,
			  "Unexpected response from Jamendo: no data");
    goto free_resources;
  }

  child_nodes = xml_count_children (node);
  node = node->xmlChildrenNode;

  xpe->node = node;
  xpe->doc = doc;
  xpe->total_results = child_nodes;

  return;

 free_resources:
  xmlFreeDoc (doc);
}

static Entry *
xml_parse_entry (xmlDocPtr doc, xmlNodePtr entry)
{
  xmlNodePtr node;
  xmlNs *ns;
  Entry *data = g_new0 (Entry, 1);

  if (strcmp ((gchar *) entry->name, JAMENDO_ARTIST) == 0) {
    data->category = JAMENDO_ARTIST_CAT;
  } else if (strcmp ((gchar *) entry->name, JAMENDO_ALBUM) == 0) {
    data->category = JAMENDO_ALBUM_CAT;
  } else if (strcmp ((gchar *) entry->name, JAMENDO_TRACK) == 0) {
    data->category = JAMENDO_TRACK_CAT;
  } else {
    g_return_val_if_reached (NULL);
  }

  node = entry->xmlChildrenNode;

  while (node) {
    ns = node->ns;

    if (!xmlStrcmp (node->name, (const xmlChar *) "id")) {
      data->id =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "artist_name")) {
      data->artist_name =
	(gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "album_name")) {
      data->album_name =
	(gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "artist_genre")) {
      data->artist_genre =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "artist_url")) {
      data->artist_url =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "artist_image")) {
      data->artist_image =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "album_genre")) {
      data->album_genre =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "album_url")) {
      data->album_url =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "album_duration")) {
      data->album_duration =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "album_image")) {
      data->album_image =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "track_name")) {
      data->track_name =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "track_url")) {
      data->track_url =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "track_stream")) {
      data->track_stream =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "track_duration")) {
      data->track_duration =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    }

    node = node->next;
  }

  return data;
}

static void
update_media_from_entry (GrlMedia *media, const Entry *entry)
{
  gchar *id;

  if (entry->id) {
    id = g_strdup_printf ("%d/%s", entry->category, entry->id);
  } else {
    id = g_strdup_printf ("%d", entry->category);
  }

  /* Common fields */
  grl_media_set_id (media, id);
  g_free (id);

  if (entry->artist_name) {
    grl_data_set_string (GRL_DATA (media),
                         GRL_METADATA_KEY_ARTIST,
                         entry->artist_name);
  }

  if (entry->album_name) {
    grl_data_set_string (GRL_DATA (media),
                         GRL_METADATA_KEY_ALBUM,
                         entry->album_name);
  }

  /* Fields for artist */
  if (entry->category == JAMENDO_ARTIST_CAT) {
    if (entry->artist_name) {
      grl_media_set_title (media, entry->artist_name);
    }

    if (entry->artist_genre) {
      grl_data_set_string (GRL_DATA (media),
                           GRL_METADATA_KEY_GENRE,
                           entry->artist_genre);
    }

    if (entry->artist_url) {
      grl_media_set_site (media, entry->artist_url);
    }

    if (entry->artist_image) {
      grl_media_set_thumbnail (media, entry->artist_image);
    }

    /* Fields for album */
  } else if (entry->category == JAMENDO_ALBUM_CAT) {
    if (entry->album_name) {
      grl_media_set_title (media, entry->album_name);
    }

    if (entry->album_genre) {
      grl_data_set_string (GRL_DATA (media),
                           GRL_METADATA_KEY_GENRE,
                           entry->album_genre);
    }

    if (entry->album_url) {
      grl_media_set_site (media, entry->album_url);
    }

    if (entry->album_image) {
      grl_media_set_thumbnail (media, entry->album_image);
    }

    if (entry->album_duration) {
      grl_media_set_duration (media, atoi (entry->album_duration));
    }

    /* Fields for track */
  } else if (entry->category == JAMENDO_TRACK_CAT) {
    if (entry->track_name) {
      grl_media_set_title (media, entry->track_name);
    }

    if (entry->album_genre) {
      grl_media_audio_set_genre (GRL_MEDIA_AUDIO (media),
                                 entry->album_genre);
    }

    if (entry->track_url) {
      grl_media_set_site (media, entry->track_url);
    }

    if (entry->album_image) {
      grl_media_set_thumbnail (media, entry->album_image);
    }

    if (entry->track_stream) {
      grl_media_set_url (media, entry->track_stream);
    }

    if (entry->track_duration) {
      grl_media_set_duration (media, atoi (entry->track_duration));
    }
  }
}

static gboolean
xml_parse_entries_idle (gpointer user_data)
{
  XmlParseEntries *xpe = (XmlParseEntries *) user_data;
  gboolean parse_more;
  GrlMedia *media;
  Entry *entry;

  g_debug ("xml_parse_entries_idle");

  parse_more = (xpe->cancelled == FALSE && xpe->node);

  if (parse_more) {
    entry = xml_parse_entry (xpe->doc, xpe->node);
    if (entry->category == JAMENDO_TRACK_CAT) {
      media = grl_media_audio_new ();
    } else {
      media = grl_media_box_new ();
    }

    update_media_from_entry (media, entry);
    free_entry (entry);

    xpe->index++;
    switch (xpe->type) {
    case BROWSE:
      xpe->spec.bs->callback (xpe->spec.bs->source,
                              xpe->spec.bs->browse_id,
                              media,
                              xpe->total_results - xpe->index,
                              xpe->spec.bs->user_data,
                              NULL);
      break;
    case QUERY:
      xpe->spec.qs->callback (xpe->spec.qs->source,
                              xpe->spec.qs->query_id,
                              media,
                              xpe->total_results - xpe->index,
                              xpe->spec.qs->user_data,
                              NULL);
      break;
    case SEARCH:
      xpe->spec.ss->callback (xpe->spec.ss->source,
                              xpe->spec.ss->search_id,
                              media,
                              xpe->total_results - xpe->index,
                              xpe->spec.ss->user_data,
                              NULL);
      break;
    }

    xpe->node = xpe->node->next;
  }

  if (!parse_more) {
    xmlFreeDoc (xpe->doc);
    g_free (xpe);
  }

  return parse_more;
}

static void
read_done_cb (gchar *contents, gpointer user_data)
{
  XmlParseEntries *xpe = user_data;
  gint error_code = -1;
  GError *error = NULL;
  Entry *entry = NULL;

  /* Check if operation was cancelled */
  if (xpe->cancelled) {
    g_free (xpe);
    return;
  }

  if (!contents) {
    switch (xpe->type) {
    case METADATA:
      error_code = GRL_ERROR_METADATA_FAILED;
      break;
    case BROWSE:
      error_code = GRL_ERROR_BROWSE_FAILED;
      break;
    case QUERY:
      error_code = GRL_ERROR_QUERY_FAILED;
      break;
    case SEARCH:
      error_code = GRL_ERROR_SEARCH_FAILED;
      break;
    }

    error = g_error_new (GRL_ERROR,
                         error_code,
                         "Failed to connect Jamendo");
    goto invoke_cb;
  }

  xml_parse_result (contents, &error, xpe);
  g_free (contents);

  if (error) {
    goto invoke_cb;
  }

  if (xpe->node) {
    if (xpe->type == METADATA) {
      entry = xml_parse_entry (xpe->doc, xpe->node);
      xmlFreeDoc (xpe->doc);
      update_media_from_entry (xpe->spec.ms->media, entry);
      free_entry (entry);
      goto invoke_cb;
    } else {
      g_idle_add (xml_parse_entries_idle, xpe);
    }
  } else {
    if (xpe->type == METADATA) {
      error = g_error_new (GRL_ERROR,
                           GRL_ERROR_METADATA_FAILED,
                           "Unable to get information: '%s'",
                           grl_media_get_id (xpe->spec.ms->media));
    }
    goto invoke_cb;
  }

  return;

 invoke_cb:
  switch (xpe->type) {
  case METADATA:
    xpe->spec.ms->callback (xpe->spec.ms->source,
                            xpe->spec.ms->media,
                            xpe->spec.ms->user_data,
                            error);
    break;
  case BROWSE:
    xpe->spec.bs->callback (xpe->spec.bs->source,
                            xpe->spec.bs->browse_id,
                            NULL,
                            0,
                            xpe->spec.bs->user_data,
                            error);
    break;
  case QUERY:
    xpe->spec.qs->callback (xpe->spec.qs->source,
                            xpe->spec.qs->query_id,
                            NULL,
                            0,
                            xpe->spec.qs->user_data,
                            error);
    break;
  case SEARCH:
    xpe->spec.ss->callback (xpe->spec.ss->source,
                            xpe->spec.ss->search_id,
                            NULL,
                            0,
                            xpe->spec.ss->user_data,
                            error);
    break;
  }

  g_free (xpe);
  if (error) {
    g_error_free (error);
  }
}

static void
read_url_async (const gchar *url, gpointer user_data)
{
  g_debug ("Opening '%s'", url);
  grl_plugins_gnome_vfs_read_url_async (url, read_done_cb, user_data);
}

static void
update_media_from_root (GrlMedia *media)
{
  grl_media_set_title (media, JAMENDO_ROOT_NAME);
  grl_media_box_set_childcount (GRL_MEDIA_BOX (media), 2);
}

static void
update_media_from_artists (GrlMedia *media)
{
  Entry *entry;

  entry = g_new0 (Entry, 1);
  entry->category = JAMENDO_ARTIST_CAT;
  entry->artist_name = g_strdup (JAMENDO_ARTIST "s");
  update_media_from_entry (media, entry);
  free_entry (entry);
}

static void
update_media_from_albums (GrlMedia *media)
{
  Entry *entry;

  entry = g_new0 (Entry, 1);
  entry->category = JAMENDO_ALBUM_CAT;
  entry->album_name = g_strdup (JAMENDO_ALBUM "s");
  update_media_from_entry (media, entry);
  free_entry (entry);
}

static void
send_toplevel_categories (GrlMediaSourceBrowseSpec *bs)
{
  GrlMedia *media;

  media = grl_media_box_new ();
  update_media_from_artists (media);
  bs->callback (bs->source, bs->browse_id, media, 1, bs->user_data, NULL);

  media = grl_media_box_new ();
  update_media_from_albums (media);
  bs->callback (bs->source, bs->browse_id, media, 0, bs->user_data, NULL);
}

static gchar *
get_jamendo_keys (JamendoCategory category)
{
  gchar *jamendo_keys = NULL;
  gchar *keys_for_artist = "artist_name+artist_genre+artist_image+artist_url";
  gchar *keys_for_album  = "album_name+album_genre+album_image+album_url+album_duration";
  gchar *keys_for_track  = "track_name+track_stream+track_url+track_duration";

  if (category == JAMENDO_ARTIST_CAT) {
    jamendo_keys = g_strconcat ("id+", keys_for_artist, NULL);
  } else if (category == JAMENDO_ALBUM_CAT) {
    jamendo_keys = g_strconcat ("id+", keys_for_artist,
                                "+", keys_for_album,
                                NULL);
  } else if (category == JAMENDO_TRACK_CAT) {
    jamendo_keys = g_strconcat ("id+", keys_for_artist,
                                "+", keys_for_album,
                                "+", keys_for_track,
                                NULL);
  }

  return jamendo_keys;
}

static gboolean
parse_query (const gchar *query, JamendoCategory *category, gchar **term)
{
  if (!query) {
    return FALSE;
  }

  if (g_str_has_prefix (query, JAMENDO_ARTIST "=")) {
    *category = JAMENDO_ARTIST_CAT;
    query += 7;
  } else if (g_str_has_prefix (query, JAMENDO_ALBUM "=")) {
    *category = JAMENDO_ALBUM_CAT;
    query += 6;
  } else if (g_str_has_prefix (query, JAMENDO_TRACK "=")) {
    *category = JAMENDO_TRACK_CAT;
    query += 6;
  } else {
    return FALSE;
  }

  *term = g_uri_escape_string (query, NULL, TRUE);
  return TRUE;
}

/* ================== API Implementation ================ */

static const GList *
grl_jamendo_source_supported_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_ARTIST,
                                      GRL_METADATA_KEY_ALBUM,
                                      GRL_METADATA_KEY_GENRE,
                                      GRL_METADATA_KEY_URL,
                                      GRL_METADATA_KEY_DURATION,
                                      GRL_METADATA_KEY_THUMBNAIL,
                                      GRL_METADATA_KEY_SITE,
                                      NULL);
  }
  return keys;
}

static void
grl_jamendo_source_metadata (GrlMediaSource *source,
                             GrlMediaSourceMetadataSpec *ms)
{
  gchar *url = NULL;
  gchar *jamendo_keys = NULL;
  const gchar *id;
  gchar **id_split = NULL;
  XmlParseEntries *xpe = NULL;
  GError *error = NULL;
  JamendoCategory category;

  g_debug ("grl_jamendo_source_metadata");

  if (!ms->media ||
      !grl_data_key_is_known (GRL_DATA (ms->media),
                              GRL_METADATA_KEY_ID)) {
    /* Get info from root */
    if (!ms->media) {
      ms->media = grl_media_box_new ();
    }
    update_media_from_root (ms->media);
  } else {
    id = grl_media_get_id (ms->media);
    id_split = g_strsplit (id, JAMENDO_ID_SEP, 0);

    if (g_strv_length (id_split) == 0) {
      error = g_error_new (GRL_ERROR,
                           GRL_ERROR_METADATA_FAILED,
                           "Invalid id: '%s'",
                           id);
      goto send_error;
    }

    category = atoi (id_split[0]);

    if (category == JAMENDO_ARTIST_CAT) {
      if (id_split[1]) {
        /* Requesting information from a specific artist */
        jamendo_keys = get_jamendo_keys (JAMENDO_ARTIST_CAT);
        url =
          g_strdup_printf (JAMENDO_GET_ARTIST,
                           jamendo_keys,
                           id_split[1]);
        g_free (jamendo_keys);
      } else {
        /* Requesting information from artist category */
        update_media_from_artists (ms->media);
      }
    } else if (category == JAMENDO_ALBUM_CAT) {
      if (id_split[1]) {
        /* Requesting information from a specific album */
        jamendo_keys = get_jamendo_keys (JAMENDO_ALBUM_CAT);
        url =
          g_strdup_printf (JAMENDO_GET_ALBUM,
                           jamendo_keys,
                           id_split[1]);
        g_free (jamendo_keys);
      } else {
        /* Requesting information from album category */
        update_media_from_albums (ms->media);
      }
    } else if (category == JAMENDO_TRACK_CAT) {
      if (id_split[1]) {
        /* Requesting information from a specific song */
        jamendo_keys = get_jamendo_keys (JAMENDO_TRACK_CAT);
        url =
          g_strdup_printf (JAMENDO_GET_TRACK,
                           jamendo_keys,
                           id_split[1]);
        g_free (jamendo_keys);
      } else {
        error = g_error_new (GRL_ERROR,
                             GRL_ERROR_METADATA_FAILED,
                             "Invalid id: '%s'",
                             id);
        g_strfreev (id_split);
        goto send_error;
      }
    } else {
      error = g_error_new (GRL_ERROR,
                           GRL_ERROR_METADATA_FAILED,
                           "Invalid id: '%s'",
                           id);
      g_strfreev (id_split);
      goto send_error;
    }
  }

  if (id_split) {
    g_strfreev (id_split);
  }

  if (url) {
    xpe = g_new0 (XmlParseEntries, 1);
    xpe->type = METADATA;
    xpe->spec.ms = ms;
    read_url_async (url, xpe);
    g_free (url);
  } else {
    if (ms->media) {
      ms->callback (ms->source, ms->media, ms->user_data, NULL);
    }
  }

  return;

 send_error:
  ms->callback (ms->source, NULL, ms->user_data, error);
  g_error_free (error);
}

static void
grl_jamendo_source_browse (GrlMediaSource *source,
                           GrlMediaSourceBrowseSpec *bs)
{
  gchar *url = NULL;
  gchar *jamendo_keys;
  gchar **container_split = NULL;
  JamendoCategory category;
  XmlParseEntries *xpe = NULL;
  const gchar *container_id;
  GError *error = NULL;

  g_debug ("grl_jamendo_source_browse");

  container_id = grl_media_get_id (bs->container);

  if (!container_id) {
    /* Root category: return top-level predefined categories */
    send_toplevel_categories (bs);
    return;
  }

  container_split = g_strsplit (container_id, JAMENDO_ID_SEP, 0);

  if (g_strv_length (container_split) == 0) {
    error = g_error_new (GRL_ERROR,
                         GRL_ERROR_BROWSE_FAILED,
                         "Invalid container-id: '%s'",
                         container_id);
  } else {
    category = atoi (container_split[0]);

    if (category == JAMENDO_ARTIST_CAT) {
      if (container_split[1]) {
        jamendo_keys = get_jamendo_keys (JAMENDO_ALBUM_CAT);
        /* Requesting information from a specific artist */
        url =
          g_strdup_printf (JAMENDO_GET_ALBUMS_FROM_ARTIST,
                           jamendo_keys,
                           bs->count,
                           bs->skip + 1,
                           container_split[1]);
      } else {
        /* Browsing through artists */
        jamendo_keys = get_jamendo_keys (JAMENDO_ARTIST_CAT);
        url = g_strdup_printf (JAMENDO_GET_ARTISTS,
                               jamendo_keys,
                               bs->count,
                               bs->skip + 1);
      }
      g_free (jamendo_keys);

    } else if (category == JAMENDO_ALBUM_CAT) {
      if (container_split[1]) {
        /* Requesting information from a specific album */
        jamendo_keys = get_jamendo_keys (JAMENDO_TRACK_CAT);
        url =
          g_strdup_printf (JAMENDO_GET_TRACKS_FROM_ALBUM,
                           jamendo_keys,
                           bs->count,
                           bs->skip + 1,
                           container_split[1]);
      } else {
        /* Browsing through albums */
        jamendo_keys = get_jamendo_keys (JAMENDO_ALBUM_CAT);
        url = g_strdup_printf (JAMENDO_GET_ALBUMS,
                               jamendo_keys,
                               bs->count,
                               bs->skip + 1);
      }
      g_free (jamendo_keys);

    } else if (category == JAMENDO_TRACK_CAT) {
      error = g_error_new (GRL_ERROR,
                           GRL_ERROR_BROWSE_FAILED,
                           "Cannot browse through a track: '%s'",
                           container_id);
    } else {
      error = g_error_new (GRL_ERROR,
                           GRL_ERROR_BROWSE_FAILED,
                           "Invalid container-id: '%s'",
                           container_id);
    }
  }

  if (error) {
    bs->callback (source, bs->browse_id, NULL, 0, bs->user_data, error);
    g_error_free (error);
    return;
  }

  xpe = g_new0 (XmlParseEntries, 1);
  xpe->type = BROWSE;
  xpe->spec.bs = bs;

  grl_media_source_set_operation_data (source, bs->browse_id, xpe);

  read_url_async (url, xpe);
  g_free (url);
  if (container_split) {
    g_strfreev (container_split);
  }
}

/*
 * Query format is "<type>=<text>", where <type> can be either 'artist', 'album'
 * or 'track' and 'text' is the term to search.
 *
 * The result will be also a <type>.
 *
 * Example: search for artists that have the "Shake" in their name or
 * description: "artist=Shake"
 *
 */
static void
grl_jamendo_source_query (GrlMediaSource *source,
                          GrlMediaSourceQuerySpec *qs)
{
  GError *error = NULL;
  JamendoCategory category;
  gchar *term = NULL;
  gchar *url;
  gchar *jamendo_keys = NULL;
  gchar *query = NULL;
  XmlParseEntries *xpe = NULL;

  g_debug ("grl_jamendo_source_query");

  if (!parse_query (qs->query, &category, &term)) {
    error = g_error_new (GRL_ERROR,
                         GRL_ERROR_QUERY_FAILED,
                         "Query malformed: '%s'",
                         qs->query);
    goto send_error;
  }

  jamendo_keys = get_jamendo_keys (category);
  switch (category) {
  case JAMENDO_ARTIST_CAT:
    query = JAMENDO_SEARCH_ARTIST;
    break;
  case JAMENDO_ALBUM_CAT:
    query = JAMENDO_SEARCH_ALBUM;
    break;
  case JAMENDO_TRACK_CAT:
    query = JAMENDO_SEARCH_TRACK;
    break;
  }

  url = g_strdup_printf (query,
                         jamendo_keys,
                         qs->count,
                         qs->skip + 1,
                         term);
  g_free (term);

  xpe = g_new0 (XmlParseEntries, 1);
  xpe->type = QUERY;
  xpe->spec.qs = qs;

  grl_media_source_set_operation_data (source, qs->query_id, xpe);

  read_url_async (url, xpe);
  g_free (url);

  return;

 send_error:
  qs->callback (qs->source, qs->query_id, NULL, 0, qs->user_data, error);
  g_error_free (error);
}


static void
grl_jamendo_source_search (GrlMediaSource *source,
                           GrlMediaSourceSearchSpec *ss)
{
  XmlParseEntries *xpe;
  gchar *jamendo_keys;
  gchar *url;

  g_debug ("grl_jamendo_source_search");

  jamendo_keys = get_jamendo_keys (JAMENDO_TRACK_CAT);

  url = g_strdup_printf (JAMENDO_SEARCH_TRACK,
                         jamendo_keys,
                         ss->count,
                         ss->skip + 1,
                         ss->text);

  xpe = g_new0 (XmlParseEntries, 1);
  xpe->type = SEARCH;
  xpe->spec.ss = ss;

  grl_media_source_set_operation_data (source, ss->search_id, xpe);

  read_url_async (url, xpe);
  g_free (url);
}

static void
grl_jamendo_source_cancel (GrlMediaSource *source, guint operation_id)
{
  XmlParseEntries *xpe;

  g_debug ("grl_jamendo_source_cancel");

  xpe = (XmlParseEntries *) grl_media_source_get_operation_data (source,
                                                                 operation_id);

  if (xpe) {
    xpe->cancelled = TRUE;
  }
}
