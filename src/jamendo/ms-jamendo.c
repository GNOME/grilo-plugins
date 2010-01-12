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

#define JAMENDO_ENTRYPOINT  "http://api.jamendo.com/get2"
#define JAMENDO_FORMAT      "xml"

#define JAMENDO_GET_ARTISTS JAMENDO_ENTRYPOINT "/%s/" JAMENDO_ARTIST "/" JAMENDO_FORMAT "/?n=%d&pn=%d"
#define JAMENDO_GET_ALBUMS  JAMENDO_ENTRYPOINT "/%s/" JAMENDO_ALBUM  "/" JAMENDO_FORMAT "/?n=%d&pn=%d"

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

typedef struct {
  gchar *type;
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
} XmlParseEntriesIdle;

typedef struct {
  gchar *id;
  gchar *name;
  gchar *url;
} CategoryInfo;

static MsJamendoSource *ms_jamendo_source_new (void);

gboolean ms_jamendo_plugin_init (MsPluginRegistry *registry,
				 const MsPluginInfo *plugin);

static MsSupportedOps ms_jamendo_source_supported_operations (MsMetadataSource *source);

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
  metadata_class->supported_operations = ms_jamendo_source_supported_operations;
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
  g_print ("   Album Image: %s\n", entry->album_image);
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
build_media_from_entry (const Entry *entry, const GList *keys)
{
  MsContentMedia *media;
  GList *iter;
  gchar *id;

  id = g_strconcat (entry->type, "/", entry->id, NULL);

  if (strcmp (entry->type, JAMENDO_TRACK) == 0) {
    media = ms_content_audio_new ();
  } else {
    media = ms_content_box_new ();
  }

  iter = (GList *) keys;
  while (iter) {
    MsKeyID key_id = GPOINTER_TO_UINT (iter->data);
    switch (key_id) {
    case MS_METADATA_KEY_ID:
      ms_content_media_set_id (media, id);
      break;

    case MS_METADATA_KEY_TITLE:
      if (strcmp (entry->type, JAMENDO_ARTIST) == 0) {
        ms_content_media_set_title (media, entry->artist_name);
      } else if (strcmp (entry->type, JAMENDO_ALBUM) == 0) {
        ms_content_media_set_title (media, entry->album_name);
      }
      break;

    case MS_METADATA_KEY_ARTIST:
      ms_content_set_string (MS_CONTENT (media), MS_METADATA_KEY_ARTIST, entry->artist_name);
      break;

    case MS_METADATA_KEY_ALBUM:
      ms_content_set_string (MS_CONTENT (media), MS_METADATA_KEY_ALBUM, entry->album_name);
      break;

    case MS_METADATA_KEY_GENRE:
      if (strcmp (entry->type, JAMENDO_ARTIST) == 0) {
        ms_content_set_string (MS_CONTENT (media), MS_METADATA_KEY_GENRE, entry->artist_genre);
      } else if (strcmp (entry->type, JAMENDO_ALBUM) == 0) {
        ms_content_set_string (MS_CONTENT (media), MS_METADATA_KEY_GENRE, entry->album_genre);
      }
      break;

    case MS_METADATA_KEY_URL:
      if (strcmp (entry->type, JAMENDO_ARTIST) == 0) {
        ms_content_media_set_url (media, entry->artist_url);
      } else if (strcmp (entry->type, JAMENDO_ALBUM) == 0) {
        ms_content_media_set_url (media, entry->album_url);
      }
      break;

    case MS_METADATA_KEY_THUMBNAIL:
      if (strcmp (entry->type, JAMENDO_ARTIST) == 0) {
        ms_content_media_set_thumbnail (media, entry->artist_image);
      } else if (strcmp (entry->type, JAMENDO_ALBUM) == 0) {
        ms_content_media_set_thumbnail (media, entry->album_image);
      }
      break;

    default:
      break;
    }
    iter = g_list_next (iter);
  }

  g_free (id);

  return media;
}

static void
xml_parse_entry (xmlDocPtr doc, xmlNodePtr entry, Entry *data)
{
  xmlNodePtr node;
  xmlNs *ns;

  data->type = g_strdup ((gchar *) entry->name);

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

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "artist_genre") &&
               (strcmp (data->type, JAMENDO_ARTIST) == 0)) {
      data->artist_genre =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "artist_url") &&
               (strcmp (data->type, JAMENDO_ARTIST) == 0)) {
      data->artist_url =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "artist_image") &&
               (strcmp (data->type, JAMENDO_ARTIST) == 0)) {
      data->artist_image =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "album_genre") &&
               (strcmp (data->type, JAMENDO_ALBUM) == 0)) {
      data->album_genre =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "album_url") &&
               (strcmp (data->type, JAMENDO_ALBUM) == 0)) {
      data->album_url =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "album_duration")) {
      data->album_duration =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "album_image")) {
      data->album_image =
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
    MsContentMedia *media = build_media_from_entry (entry, xpei->os->keys);
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
  entry->type = JAMENDO_ARTIST;
  entry->artist_name = g_strdup (JAMENDO_ARTIST "s");
  media = build_media_from_entry (entry, os->keys);
  free_entry (entry);
  os->callback (os->source, os->operation_id, media, 1, os->user_data, NULL);

  /* Send 'albums' root category */
  entry = g_new0 (Entry, 1);
  entry->type = JAMENDO_ALBUM;
  entry->album_name = g_strdup (JAMENDO_ALBUM "s");
  media = build_media_from_entry (entry, os->keys);
  free_entry (entry);
  os->callback (os->source, os->operation_id, media, 1, os->user_data, NULL);
}

static gchar *
marshall_keys (const gchar *type, GList *keys)
{
  gchar *jamendo_keys = NULL;
  gchar *keys_for_artist = "artist_name+artist_genre+artist_image+artist_url";
  gchar *keys_for_album  = "album_name+album_genre+album_image+album_url+album_duration";

  if (strcmp (type, JAMENDO_ARTIST) == 0) {
    jamendo_keys = g_strconcat ("id+", keys_for_artist, NULL);
  } else if (strcmp (type, JAMENDO_ALBUM) == 0) {
    jamendo_keys = g_strconcat ("id+", keys_for_artist, "+", keys_for_album, NULL);
  }

  return jamendo_keys;
}
/* ================== API Implementation ================ */

static MsSupportedOps
ms_jamendo_source_supported_operations (MsMetadataSource *source)
{
  return MS_OP_BROWSE;
}

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
    jamendo_keys = marshall_keys (container_split[0], bs->keys);

    if (strcmp (container_split[0], JAMENDO_ARTIST) == 0) {
      /* Browsing through artists */
      url = g_strdup_printf (JAMENDO_GET_ARTISTS, jamendo_keys, bs->count, bs->skip + 1);
    } else if (strcmp (container_split[0], JAMENDO_ALBUM) == 0) {
      /* Browsing through albums */
      url = g_strdup_printf (JAMENDO_GET_ALBUMS, jamendo_keys, bs->count, bs->skip + 1);
    } else if (strcmp (container_split[0], JAMENDO_TRACK) == 0) {
      /* We're managing tracks */
      g_strdup ("");
    } else {
      error = g_error_new (MS_ERROR,
                           MS_ERROR_BROWSE_FAILED,
                           "Invalid container-id: '%s'",
                           bs->container_id);
    }
    g_free (jamendo_keys);
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
