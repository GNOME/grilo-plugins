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

#include <media-store.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <string.h>
#include <stdlib.h>

#include "ms-jamendo.h"

/* --------- Logging  -------- */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "ms-jamendo"

#define JAMENDO_ID_SEP    "/"
#define JAMENDO_ROOT_NAME "Jamendo"

/* ------- Categories ------- */

#define JAMENDO_ARTIST "artist"
#define JAMENDO_ALBUM  "album"
#define JAMENDO_TRACK  "track"

/* ---- Jamendo Web API  ---- */

#define JAMENDO_BASE_ENTRY "http://api.jamendo.com/get2"
#define JAMENDO_FORMAT     "xml"
#define JAMENDO_RANGE      "n=%d&pn=%d"

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

#define PLUGIN_ID   "ms-jamendo"
#define PLUGIN_NAME "Jamendo"
#define PLUGIN_DESC "A plugin for browsing and searching Jamendo videos"

#define SOURCE_ID   "ms-jamendo"
#define SOURCE_NAME "Jamendo"
#define SOURCE_DESC "A source for browsing and searching Jamendo videos"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

enum {
  BROWSE,
  QUERY
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
    MsMediaSourceBrowseSpec *bs;
    MsMediaSourceQuerySpec *qs;
  } spec;
  xmlNodePtr node;
  xmlDocPtr doc;
  guint total_results;
  guint index;
} XmlParseEntries;

static MsJamendoSource *ms_jamendo_source_new (void);

gboolean ms_jamendo_plugin_init (MsPluginRegistry *registry,
				 const MsPluginInfo *plugin);

static const GList *ms_jamendo_source_supported_keys (MsMetadataSource *source);

static void ms_jamendo_source_metadata (MsMediaSource *source,
                                        MsMediaSourceMetadataSpec *ms);

static void ms_jamendo_source_browse (MsMediaSource *source,
				      MsMediaSourceBrowseSpec *bs);

static void ms_jamendo_source_query (MsMediaSource *source,
                                     MsMediaSourceQuerySpec *qs);

static void ms_jamendo_source_search (MsMediaSource *source,
				      MsMediaSourceSearchSpec *ss);

static gchar *read_url (const gchar *url);

/* ==================== Global Data  ================= */

guint root_dir_size = 2;

guint feeds_dir_size = 7;

guint categories_dir_size = 0;

/* =================== Jamendo Plugin  =============== */

gboolean
ms_jamendo_plugin_init (MsPluginRegistry *registry, const MsPluginInfo *plugin)
{
  g_debug ("jamendo_plugin_init\n");

  MsJamendoSource *source = ms_jamendo_source_new ();
  ms_plugin_registry_register_source (registry, plugin, MS_MEDIA_PLUGIN (source));
  return TRUE;
}

MS_PLUGIN_REGISTER (ms_jamendo_plugin_init, 
                    NULL, 
                    PLUGIN_ID,
                    PLUGIN_NAME, 
                    PLUGIN_DESC, 
                    PACKAGE_VERSION,
                    AUTHOR, 
                    LICENSE, 
                    SITE);

/* ================== Jamendo GObject ================ */

static MsJamendoSource *
ms_jamendo_source_new (void)
{
  g_debug ("ms_jamendo_source_new");
  return g_object_new (MS_JAMENDO_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       NULL);
}

static void
ms_jamendo_source_class_init (MsJamendoSourceClass * klass)
{
  MsMediaSourceClass *source_class = MS_MEDIA_SOURCE_CLASS (klass);
  MsMetadataSourceClass *metadata_class = MS_METADATA_SOURCE_CLASS (klass);
  source_class->metadata = ms_jamendo_source_metadata;
  source_class->browse = ms_jamendo_source_browse;
  source_class->query = ms_jamendo_source_query;
  source_class->search = ms_jamendo_source_search;
  metadata_class->supported_keys = ms_jamendo_source_supported_keys;

  if (!gnome_vfs_init ()) {
    g_error ("Failed to initialize Gnome VFS");
  }
}

static void
ms_jamendo_source_init (MsJamendoSource *source)
{
}

G_DEFINE_TYPE (MsJamendoSource, ms_jamendo_source, MS_TYPE_MEDIA_SOURCE);

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

static void
update_media_from_entry (MsContentMedia *media, const Entry *entry)
{
  gchar *id;

  if (entry->id) {
    id = g_strdup_printf ("%d/%s", entry->category, entry->id);
  } else {
    id = g_strdup_printf ("%d", entry->category);
  }

  /* Common fields */
  ms_content_media_set_id (media, id);
  g_free (id);

  if (entry->artist_name) {
    ms_content_set_string (MS_CONTENT (media), MS_METADATA_KEY_ARTIST, entry->artist_name);
  }

  if (entry->album_name) {
    ms_content_set_string (MS_CONTENT (media), MS_METADATA_KEY_ALBUM, entry->album_name);
  }

  /* Fields for artist */
  if (entry->category == JAMENDO_ARTIST_CAT) {
    if (entry->artist_name) {
      ms_content_media_set_title (media, entry->artist_name);
    }

    if (entry->artist_genre) {
      ms_content_set_string (MS_CONTENT (media), MS_METADATA_KEY_GENRE, entry->artist_genre);
    }

    if (entry->artist_url) {
      ms_content_media_set_site (media, entry->artist_url);
    }

    if (entry->artist_image) {
      ms_content_media_set_thumbnail (media, entry->artist_image);
    }

    /* Fields for album */
  } else if (entry->category == JAMENDO_ALBUM_CAT) {
    if (entry->album_name) {
      ms_content_media_set_title (media, entry->album_name);
    }

    if (entry->album_genre) {
      ms_content_set_string (MS_CONTENT (media), MS_METADATA_KEY_GENRE, entry->album_genre);
    }

    if (entry->album_url) {
      ms_content_media_set_site (media, entry->album_url);
    }

    if (entry->album_image) {
      ms_content_media_set_thumbnail (media, entry->album_image);
    }

    if (entry->album_duration) {
      ms_content_media_set_duration (media, atoi (entry->album_duration));
    }

    /* Fields for track */
  } else if (entry->category == JAMENDO_TRACK_CAT) {
    if (entry->track_name) {
      ms_content_media_set_title (media, entry->track_name);
    }

    if (entry->album_genre) {
      ms_content_audio_set_genre (MS_CONTENT_AUDIO (media), entry->album_genre);
    }

    if (entry->track_url) {
      ms_content_media_set_site (media, entry->track_url);
    }

    if (entry->album_image) {
      ms_content_media_set_thumbnail (media, entry->album_image);
    }

    if (entry->track_stream) {
      ms_content_media_set_url (media, entry->track_stream);
    }

    if (entry->track_duration) {
      ms_content_media_set_duration (media, atoi (entry->track_duration));
    }
  }
}


static void
update_media_from_root (MsContentMedia *media)
{
  ms_content_media_set_title (media, JAMENDO_ROOT_NAME);
  ms_content_box_set_childcount (MS_CONTENT_BOX (media), 2);
}

static void
update_media_from_artists (MsContentMedia *media)
{
  Entry *entry;

  entry = g_new0 (Entry, 1);
  entry->category = JAMENDO_ARTIST_CAT;
  entry->artist_name = g_strdup (JAMENDO_ARTIST "s");
  update_media_from_entry (media, entry);
  free_entry (entry);
}

static void
update_media_from_albums (MsContentMedia *media)
{
  Entry *entry;

  entry = g_new0 (Entry, 1);
  entry->category = JAMENDO_ALBUM_CAT;
  entry->album_name = g_strdup (JAMENDO_ALBUM "s");
  update_media_from_entry (media, entry);
  free_entry (entry);
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

static gboolean
xml_parse_entries_idle (gpointer user_data)
{
  XmlParseEntries *xpe = (XmlParseEntries *) user_data;
  gboolean parse_more = FALSE;
  MsContentMedia *media;
  Entry *entry;

  g_debug ("xml_parse_entries_idle");

  if (xpe->node) {
    entry = xml_parse_entry (xpe->doc, xpe->node);
    if (entry->category == JAMENDO_TRACK_CAT) {
      media = ms_content_audio_new ();
    } else {
      media = ms_content_box_new ();
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
    }

    parse_more = TRUE;
    xpe->node = xpe->node->next;
  }

  if (!parse_more) {
    xmlFreeDoc (xpe->doc);
    g_free (xpe);
  }

  return parse_more;
}

static XmlParseEntries *
xml_parse_result (const gchar *str, GError **error)
{
  xmlDocPtr doc;
  xmlNodePtr node;
  gint child_nodes = 0;

  doc = xmlRecoverDoc ((xmlChar *) str);
  if (!doc) {
    *error = g_error_new (MS_ERROR,
			  MS_ERROR_BROWSE_FAILED,
			  "Failed to parse Jamendo's response");
    goto free_resources;
  }

  node = xmlDocGetRootElement (doc);
  if (!node) {
    *error = g_error_new (MS_ERROR,
			  MS_ERROR_BROWSE_FAILED,
			  "Empty response from Jamendo");
    goto free_resources;
  }

  if (xmlStrcmp (node->name, (const xmlChar *) "data")) {
    *error = g_error_new (MS_ERROR,
			  MS_ERROR_BROWSE_FAILED,
			  "Unexpected response from Jamendo: no data");
    goto free_resources;
  }

  child_nodes = xml_count_children (node);
  node = node->xmlChildrenNode;

  XmlParseEntries *xpe = g_new (XmlParseEntries, 1);
  xpe->node = node;
  xpe->doc = doc;
  xpe->total_results = child_nodes;
  xpe->index = 0;

  return xpe;

 free_resources:
  xmlFreeDoc (doc);
  return NULL;
}

static void
send_toplevel_categories (MsMediaSourceBrowseSpec *bs)
{
  MsContentMedia *media;

  media = ms_content_box_new ();
  update_media_from_artists (media);
  bs->callback (bs->source, bs->browse_id, media, 1, bs->user_data, NULL);

  media = ms_content_box_new ();
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
ms_jamendo_source_supported_keys (MsMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = ms_metadata_key_list_new (MS_METADATA_KEY_ID,
				     MS_METADATA_KEY_TITLE,
                                     MS_METADATA_KEY_ARTIST,
                                     MS_METADATA_KEY_ALBUM,
                                     MS_METADATA_KEY_GENRE,
                                     MS_METADATA_KEY_URL,
                                     MS_METADATA_KEY_DURATION,
                                     MS_METADATA_KEY_THUMBNAIL,
                                     MS_METADATA_KEY_SITE,
                                     NULL);
  }
  return keys;
}

static void
ms_jamendo_source_metadata (MsMediaSource *source,
                            MsMediaSourceMetadataSpec *ms)
{
  Entry *entry = NULL;
  gchar *url = NULL;
  gchar *jamendo_keys = NULL;
  gchar *xmldata = NULL;
  const gchar *id;
  gchar **id_split = NULL;
  XmlParseEntries *xpe = NULL;
  GError *error = NULL;
  JamendoCategory category;

  g_debug ("ms_jamendo_source_metadata");

  if (!ms->media ||
      !ms_content_key_is_known (MS_CONTENT (ms->media), MS_METADATA_KEY_ID)) {
    /* Get info from root */
    if (!ms->media) {
      ms->media = ms_content_box_new ();
    }
    update_media_from_root (ms->media);
  } else {
    id = ms_content_media_get_id (ms->media);
    id_split = g_strsplit (id, JAMENDO_ID_SEP, 0);

    if (g_strv_length (id_split) == 0) {
      error = g_error_new (MS_ERROR,
                           MS_ERROR_METADATA_FAILED,
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
        error = g_error_new (MS_ERROR,
                             MS_ERROR_METADATA_FAILED,
                             "Invalid id: '%s'",
                             id);
        g_strfreev (id_split);
        goto send_error;
      }
    } else {
      error = g_error_new (MS_ERROR,
                           MS_ERROR_METADATA_FAILED,
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
    xmldata = read_url (url);
    g_free (url);

    if (!xmldata) {
      error = g_error_new (MS_ERROR,
                           MS_ERROR_METADATA_FAILED,
                           "Failed to connect to Jamendo");
      goto send_error;
    }

    xpe = xml_parse_result (xmldata, &error);
    g_free (xmldata);

    if (error) {
      g_free (xpe);
      goto send_error;
    }

    if (xpe->node) {
      entry = xml_parse_entry (xpe->doc, xpe->node);
      xmlFreeDoc (xpe->doc);
      g_free (xpe);
      update_media_from_entry (ms->media, entry);
      free_entry (entry);
    } else {
      error = g_error_new (MS_ERROR,
                           MS_ERROR_METADATA_FAILED,
                           "Unable to get information: '%s'",
                           id);
      xmlFreeDoc (xpe->doc);
      g_free (xpe);
      goto send_error;
    }
  }

  if (ms->media) {
    ms->callback (ms->source, ms->media, ms->user_data, NULL);
  }

  return;

 send_error:
  ms->callback (ms->source, NULL, ms->user_data, error);
  g_error_free (error);
}

static void
ms_jamendo_source_browse (MsMediaSource *source,
                          MsMediaSourceBrowseSpec *bs)
{
  gchar *xmldata, *url, *jamendo_keys;
  GError *error = NULL;
  gchar **container_split = NULL;
  JamendoCategory category;
  XmlParseEntries *xpe = NULL;
  const gchar *container_id;

  g_debug ("ms_jamendo_source_browse");

  container_id = ms_content_media_get_id (bs->container);

  if (!container_id) {
    /* Root category: return top-level predefined categories */
    send_toplevel_categories (bs);
    return;
  }

  container_split = g_strsplit (container_id, JAMENDO_ID_SEP, 0);

  if (g_strv_length (container_split) == 0) {
    error = g_error_new (MS_ERROR,
                         MS_ERROR_BROWSE_FAILED,
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
        url = g_strdup_printf (JAMENDO_GET_ARTISTS, jamendo_keys, bs->count, bs->skip + 1);
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
        url = g_strdup_printf (JAMENDO_GET_ALBUMS, jamendo_keys, bs->count, bs->skip + 1);
      }
      g_free (jamendo_keys);

    } else if (category == JAMENDO_TRACK_CAT) {
      error = g_error_new (MS_ERROR,
                           MS_ERROR_BROWSE_FAILED,
                           "Cannot browse through a track: '%s'",
                           container_id);
    } else {
      error = g_error_new (MS_ERROR,
                           MS_ERROR_BROWSE_FAILED,
                           "Invalid container-id: '%s'",
                           container_id);
    }
  }

  if (error) {
    bs->callback (source, bs->browse_id, NULL, 0, bs->user_data, error);
    g_error_free (error);
    return;
  }

  xmldata = read_url (url);
  g_free (url);
  if (container_split) {
    g_strfreev (container_split);
  }

  if (!xmldata) {
    error = g_error_new (MS_ERROR,
                         MS_ERROR_BROWSE_FAILED,
                         "Failed to connect to Jamendo");
    bs->callback (source, bs->browse_id, NULL, 0, bs->user_data, error);
    g_error_free (error);
    return;
  }

  xpe = xml_parse_result (xmldata, &error);
  if (error) {
    bs->callback (bs->source, bs->browse_id, NULL, 0, bs->user_data, error);
    g_error_free (error);
  } else {
    /* Check if there are results */
    if (xpe->node) {
      xpe->type = BROWSE;
      xpe->spec.bs = bs;
      g_idle_add (xml_parse_entries_idle, xpe);
    } else {
      bs->callback (source, bs->browse_id, NULL, 0, bs->user_data, NULL);
      g_free (xpe);
    }
    g_free (xmldata);
  }
}

/*
 * Query format is "<type>=<text>", where <type> can be either 'artist', 'album'
 * or 'track' and 'text' is the term to search.
 *
 * The result will be also a <type>.
 *
 * Example: search for artists that have the "Shake" in their name or description:
 * "artist=Shake"
 *
 */
static void
ms_jamendo_source_query (MsMediaSource *source,
                         MsMediaSourceQuerySpec *qs)
{
  GError *error = NULL;
  JamendoCategory category;
  gchar *term = NULL;
  gchar *url;
  gchar *jamendo_keys = NULL;
  gchar *query = NULL;
  XmlParseEntries *xpe = NULL;
  gchar *xmldata;

  g_debug ("ms_jamendo_source_query");

  if (!parse_query (qs->query, &category, &term)) {
    error = g_error_new (MS_ERROR,
                         MS_ERROR_QUERY_FAILED,
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

  xmldata = read_url (url);
  g_free (url);

  if (!xmldata) {
    error = g_error_new (MS_ERROR,
                         MS_ERROR_QUERY_FAILED,
                         "Failed to connect to Jamendo");
    goto send_error;
  }

  xpe = xml_parse_result (xmldata, &error);
  g_free (xmldata);
  if (error) {
    goto send_error;
  }

  /* Check if there are results */
  if (xpe->node) {
    xpe->type = QUERY;
    xpe->spec.qs = qs;
    g_idle_add (xml_parse_entries_idle, xpe);
  } else {
    qs->callback (qs->source, qs->query_id, NULL, 0, qs->user_data, NULL);
    g_free (xpe);
  }

  return;

 send_error:
  qs->callback (qs->source, qs->query_id, NULL, 0, qs->user_data, error);
  g_error_free (error);
}


static void
ms_jamendo_source_search (MsMediaSource *source,
                          MsMediaSourceSearchSpec *ss)
{
  gchar *query;

  g_debug ("ms_jamendo_source_search");

  query = g_strconcat (JAMENDO_TRACK "=", ss->text, NULL);

  ms_media_source_query (source,
                         query,
                         ss->keys,
                         ss->skip,
                         ss->count,
                         ss->flags,
                         ss->callback,
                         ss->user_data);
  g_free (query);
}
