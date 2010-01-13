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

#define CONTAINER_SEP "/"

/* ------- Categories ------- */

#define JAMENDO_ARTIST "artist"
#define JAMENDO_ALBUM  "album"
#define JAMENDO_TRACK  "track"

/* ---- Jamendo Web API  ---- */

#define JAMENDO_BASE_ENTRY "http://api.jamendo.com/get2"
#define JAMENDO_FORMAT     "xml"
#define JAMENDO_RANGE      "n=%d&pn=%d" 

#define JAMENDO_ARTIST_ENTRY JAMENDO_BASE_ENTRY "/%s/" JAMENDO_ARTIST "/" JAMENDO_FORMAT
#define JAMENDO_ALBUM_ENTRY  JAMENDO_BASE_ENTRY "/%s/" JAMENDO_ALBUM  "/" JAMENDO_FORMAT
#define JAMENDO_TRACK_ENTRY  JAMENDO_BASE_ENTRY "/%s/" JAMENDO_TRACK  "/" JAMENDO_FORMAT

#define JAMENDO_GET_ARTISTS JAMENDO_ARTIST_ENTRY "/?" JAMENDO_RANGE
#define JAMENDO_GET_ALBUMS  JAMENDO_ALBUM_ENTRY  "/?" JAMENDO_RANGE

#define JAMENDO_GET_ALBUMS_FROM_ARTIST JAMENDO_ALBUM_ENTRY "/"          \
  JAMENDO_ALBUM "_" JAMENDO_ARTIST "/?" JAMENDO_RANGE "&artist_id=%s"

#define JAMENDO_GET_TRACKS_FROM_ALBUM JAMENDO_TRACK_ENTRY "/"          \
  JAMENDO_TRACK "_" JAMENDO_ALBUM "+" JAMENDO_ALBUM "_" JAMENDO_ARTIST \
  "/?" JAMENDO_RANGE "&album_id=%s"

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
  MsMediaSource *source;
  guint operation_id;
  const GList *keys;
  guint skip;
  guint count;
  MsMediaSourceResultCb callback;
  gpointer user_data;
} OperationSpec;

typedef struct {
  OperationSpec *os;
  xmlNodePtr node;
  xmlDocPtr doc;
  guint total_results;
  guint index;
} XmlParseEntriesIdle;

static MsJamendoSource *ms_jamendo_source_new (void);

gboolean ms_jamendo_plugin_init (MsPluginRegistry *registry,
				 const MsPluginInfo *plugin);

static const GList *ms_jamendo_source_supported_keys (MsMetadataSource *source);

static void ms_jamendo_source_browse (MsMediaSource *source,
				      MsMediaSourceBrowseSpec *bs);

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
  source_class->browse = ms_jamendo_source_browse;
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

static MsContentMedia *
build_media_from_entry (const Entry *entry)
{
  MsContentMedia *media;
  gchar *id;

  if (entry->id) {
    id = g_strdup_printf ("%d/%s", entry->category, entry->id);
  } else {
    id = g_strdup_printf ("%d", entry->category);
  }

  if (entry->category == JAMENDO_TRACK_CAT) {
    media = ms_content_audio_new ();
  } else {
    media = ms_content_box_new ();
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

  return media;
}

static void
xml_parse_entry (xmlDocPtr doc, xmlNodePtr entry, Entry *data)
{
  xmlNodePtr node;
  xmlNs *ns;

  if (strcmp ((gchar *) entry->name, JAMENDO_ARTIST) == 0) {
    data->category = JAMENDO_ARTIST_CAT;
  } else if (strcmp ((gchar *) entry->name, JAMENDO_ALBUM) == 0) {
    data->category = JAMENDO_ALBUM_CAT;
  } else if (strcmp ((gchar *) entry->name, JAMENDO_TRACK) == 0) {
    data->category = JAMENDO_TRACK_CAT;
  } else {
    g_return_if_reached ();
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
}

static gboolean
xml_parse_entries_idle (gpointer user_data)
{
  XmlParseEntriesIdle *xpei = (XmlParseEntriesIdle *) user_data;
  gboolean parse_more = FALSE;

  g_debug ("xml_parse_entries_idle");

  if (xpei->node) {
    Entry *entry = g_new0 (Entry, 1);
    xml_parse_entry (xpei->doc, xpei->node, entry);
    if (0) print_entry (entry);
    MsContentMedia *media = build_media_from_entry (entry);
    free_entry (entry);

    xpei->index++;
    xpei->os->callback (xpei->os->source,
                        xpei->os->operation_id,
                        media,
                        xpei->total_results - xpei->index,
                        xpei->os->user_data,
		       NULL);

    parse_more = TRUE;
    xpei->node = xpei->node->next;
  }

  if (!parse_more) {
    xmlFreeDoc (xpei->doc);
    g_free (xpei->os);
    g_free (xpei);
  }

  return parse_more;
}

static void
xml_parse_result (OperationSpec *os, const gchar *str, GError **error)
{
  xmlDocPtr doc;
  xmlNodePtr node;

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

  node = node->xmlChildrenNode;

  /* Now go for the entries */
  XmlParseEntriesIdle *xpei = g_new (XmlParseEntriesIdle, 1);
  xpei->os = os;
  xpei->node = node;
  xpei->doc = doc;
  xpei->total_results = 0;
  xpei->index = 0;
  g_idle_add (xml_parse_entries_idle, xpei);

  return;

 free_resources:
  xmlFreeDoc (doc);
  return;
}

static void
send_toplevel_categories (OperationSpec *os)
{
  Entry *entry;
  MsContentMedia *media;

  /* Send 'artists' root category */
  entry = g_new0 (Entry, 1);
  entry->category = JAMENDO_ARTIST_CAT;
  entry->artist_name = g_strdup (JAMENDO_ARTIST "s");
  media = build_media_from_entry (entry);
  free_entry (entry);
  os->callback (os->source, os->operation_id, media, 1, os->user_data, NULL);

  /* Send 'albums' root category */
  entry = g_new0 (Entry, 1);
  entry->category = JAMENDO_ALBUM_CAT;
  entry->album_name = g_strdup (JAMENDO_ALBUM "s");
  media = build_media_from_entry (entry);
  free_entry (entry);
  os->callback (os->source, os->operation_id, media, 1, os->user_data, NULL);
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
ms_jamendo_source_browse (MsMediaSource *source, MsMediaSourceBrowseSpec *bs)
{
  gchar *xmldata, *url, *jamendo_keys;
  GError *error = NULL;
  OperationSpec *os;
  gchar **container_split = NULL;
  JamendoCategory category;

  g_debug ("ms_jamendo_source_browse (%s)", bs->container_id);

  os = g_new0 (OperationSpec, 1);
  os->source = bs->source;
  os->operation_id = bs->browse_id;
  os->keys = bs->keys;
  os->skip = bs->skip;
  os->count = bs->count;
  os->callback = bs->callback;
  os->user_data = bs->user_data;

  if (!bs->container_id) {
    /* Root category: return top-level predefined categories */
    send_toplevel_categories (os);
    g_free (os);
    return;
  }

  container_split = g_strsplit (bs->container_id, CONTAINER_SEP, 0);

  if (g_strv_length (container_split) == 0) {
    error = g_error_new (MS_ERROR,
                         MS_ERROR_BROWSE_FAILED,
                         "Invalid container-id: '%s'",
                         bs->container_id);
  } else {
    category = atoi (container_split[0]);

    if (category == JAMENDO_ARTIST_CAT) {
      /* Browsing through artists */
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
        jamendo_keys = get_jamendo_keys (JAMENDO_ARTIST_CAT);
        url = g_strdup_printf (JAMENDO_GET_ARTISTS, jamendo_keys, bs->count, bs->skip + 1);
      }
      g_free (jamendo_keys);

    } else if (category == JAMENDO_ALBUM_CAT) {
      /* Browsing through albums */
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
        jamendo_keys = get_jamendo_keys (JAMENDO_ALBUM_CAT);
        url = g_strdup_printf (JAMENDO_GET_ALBUMS, jamendo_keys, bs->count, bs->skip + 1);
      }
      g_free (jamendo_keys);

    } else if (category == JAMENDO_TRACK_CAT) {
      error = g_error_new (MS_ERROR,
                           MS_ERROR_BROWSE_FAILED,
                           "Cannot browse through a track: '%s'",
                           bs->container_id);
    } else {
      error = g_error_new (MS_ERROR,
                           MS_ERROR_BROWSE_FAILED,
                           "Invalid container-id: '%s'",
                           bs->container_id);
    }
  }

  if (error) {
    bs->callback (source, bs->browse_id, NULL, 0, bs->user_data, error);
    g_error_free (error);
    g_free (os);
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
    g_free (os);
    return;
  }

  xml_parse_result (os, xmldata, &error);
  g_free (xmldata);

  if (error) {
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
    g_free (os);
  }
}
