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

#include "youtube-source.h"
#include "../src/plugin-registry.h"
#include "../src/content/content-media.h"

#include <glib.h>
#include <libgnomevfs/gnome-vfs.h>
#include <string.h>

#define PLUGIN_ID "youtube-plugin-id"

gboolean youtube_plugin_init (PluginRegistry *registry, const PluginInfo *plugin);

PLUGIN_REGISTER (youtube_plugin_init, 
		 NULL, 
                 PLUGIN_ID,
		 "Youtube plugin", 
		 "A plugin for browsing youtube videos", 
		 "0.0.1",
		 "Igalia S.L.", 
		 "LGPL", 
		 "http://www.igalia.com");

gboolean
youtube_plugin_init (PluginRegistry *registry, const PluginInfo *plugin)
{
  g_print ("youtube_plugin_init\n");
  YoutubeSource *source = youtube_source_new ();
  plugin_registry_register_source (registry, plugin, MEDIA_PLUGIN (source));
  return TRUE;
}

static gchar *
youtube_get_video_url (gchar *id)
{
  GnomeVFSHandle *fh;
  gchar buffer[1025];
  GString *info = g_string_new ("");
  gchar *token_start;
  gchar *token_end;
  gchar *token;
  GnomeVFSResult result;
  GnomeVFSFileSize bytes_read;

  gchar *get_info_url = 
    g_strdup_printf ("http://www.youtube.com/get_video_info?video_id=%s", id);

  result = gnome_vfs_open (&fh, get_info_url, GNOME_VFS_OPEN_READ);
  if (result != GNOME_VFS_OK) {
    g_error ("could not open video_info URL");
  }

  do {
    gnome_vfs_read (fh, buffer, 1024, &bytes_read);
    buffer[bytes_read] = '\0';
    g_string_append (info, buffer);
  } while (bytes_read);

  token_start = g_strrstr (info->str, "&token=");
  if (!token_start) 
    return NULL;
  token_start += 7;

  token_end = strstr (token_start, "&");
  
  token = g_strndup (token_start, token_end - token_start);

  return g_strdup_printf ("http://www.youtube.com/get_video?video_id=%s&t=%s",
			  id, token);
}

static void
youtube_parse_response (gchar **xml,
			gchar **id,
			gchar **title,
			gchar **author,
			gchar **description,
			gchar **url)
{
    /* ID */
    *xml = strstr (*xml, "<id>");
    gchar *d_start = *xml + 4;
    gchar *d_end = strstr (d_start, "</id>");
    *id = g_strrstr (g_strndup (d_start, d_end - d_start), "/") + 1;

    *xml = d_end + 5;

    /* Title */
    *xml = strstr (*xml, "<title type='text'>");
    d_start = *xml + 19;
    d_end = strstr (d_start, "</title>");
    *title = g_strndup (d_start, d_end - d_start);

    *xml = d_end + 8;

    /* Author */
    *xml = strstr (*xml, "<author><name>");
    d_start = *xml + 14;
    d_end = strstr (d_start, "</name>");
    *author = g_strndup (d_start, d_end - d_start);

    *xml = d_end + 7;

    /* Description */
    *xml = strstr (*xml, "<media:description type='plain'>");
    d_start = *xml + 31;
    d_end = strstr (d_start, "</media:description>");
    *description = g_strndup (d_start, d_end - d_start);

    *xml = d_end + 20;

    /* URL */
    *url = youtube_get_video_url (*id);
}
			
static guint
youtube_source_browse (MediaSource *source, 
		       const gchar *container_id,
		       const KeyID *keys,
		       guint skip,
		       guint count,
		       MediaSourceResultCb callback,
		       gpointer user_data)
{
  g_print ("youtube_source_browse\n");

  gchar buffer[1025];
  GnomeVFSFileSize bytes_read;
  GnomeVFSResult result;
  GString *xmlinfo = g_string_new ("");
  gint n;

  /* Read most_viewed feed */
  GnomeVFSHandle *fh;
  result = gnome_vfs_open (&fh, 
			   "http://gdata.youtube.com/feeds/standardfeeds/most_viewed", 
			   GNOME_VFS_OPEN_READ);
  if (result != GNOME_VFS_OK) {
    g_error ("could not open most_viewed URL");
  }

  do {
    gnome_vfs_read (fh, buffer, 1024, &bytes_read);
    buffer[bytes_read] = '\0';
    g_string_append (xmlinfo, buffer);
  } while (bytes_read);

  gchar *xml = xmlinfo->str;

  n = 0;
  while (((xml = strstr (xml, "<entry>")) != NULL)) {
    gchar *id, *title, *author, *description, *url;
    youtube_parse_response (&xml, &id, &title, &author, &description, &url);
    ContentMedia *content = content_media_new ();
    content_media_set_source (content, PLUGIN_ID);
    content_media_set_id (content, id);
    content_media_set_url (content, url);
    content_media_set_title (content, title);
    content_media_set_author (content, author);
    content_media_set_description(content, description);
    n++;
    callback (source, 0, CONTENT(content), 25 - n, user_data, NULL);
  }

  return 0;
}

static guint
youtube_source_search (MediaSource *source,
		       const gchar *text,
		       const gchar *filter,
		       guint skip,
		       guint count,
		       MediaSourceResultCb callback,
		       gpointer user_data)
{
  g_print ("youtube_source_search\n");

  gchar buffer[1025];
  GnomeVFSFileSize bytes_read;
  GnomeVFSResult result;
  GString *xmlinfo = g_string_new ("");
  gint n;
  gchar *url;

  url = g_strdup_printf ("http://gdata.youtube.com/feeds/api/videos?vq=%s&start-index=1&max-results=10",
			 text);

  GnomeVFSHandle *fh;
  result = gnome_vfs_open (&fh, url, GNOME_VFS_OPEN_READ);
  if (result != GNOME_VFS_OK) {
    g_error ("could not open query URL");
  }

  do {
    gnome_vfs_read (fh, buffer, 1024, &bytes_read);
    buffer[bytes_read] = '\0';
    g_string_append (xmlinfo, buffer);
  } while (bytes_read);

  gchar *xml = xmlinfo->str;

  n = 0;
  while (((xml = strstr (xml, "<entry>")) != NULL)) {
    gchar *id, *title, *author, *description, *url;
    youtube_parse_response (&xml, &id, &title, &author, &description, &url);
    ContentMedia *content = content_media_new ();
    content_media_set_source (content, PLUGIN_ID);
    content_media_set_id (content, id);
    content_media_set_url (content, url);
    content_media_set_title (content, title);
    content_media_set_author (content, author);
    content_media_set_description(content, description);
    n++;
    callback (source, 0, CONTENT(content), 10 - n, user_data, NULL);
  }

  return 0;
}

static void
youtube_source_metadata (MetadataSource *source,
			 const gchar *object_id,
			 const KeyID *keys,
			 MetadataSourceResultCb callback,
			 gpointer user_data)
{
  g_print ("youtube_source_metadata\n");

  gchar buffer[1025];
  GnomeVFSFileSize bytes_read;
  GnomeVFSResult result;
  GString *xmlinfo = g_string_new ("");
  gchar *info_url;

  info_url = 
    g_strdup_printf ("http://gdata.youtube.com/feeds/api/videos?vq=%s&start-index=1&max-results=1",
		     object_id);
  
  GnomeVFSHandle *fh;
  result = gnome_vfs_open (&fh, info_url, GNOME_VFS_OPEN_READ);
  if (result != GNOME_VFS_OK) {
    g_error ("could not open query URL");
  }
  
  do {
    gnome_vfs_read (fh, buffer, 1024, &bytes_read);
    buffer[bytes_read] = '\0';
    g_string_append (xmlinfo, buffer);
  } while (bytes_read);
  
  gchar *xml = xmlinfo->str;
  
  xml = strstr (xml, "<entry>");
  gchar *id, *title, *author, *description, *url;
  youtube_parse_response (&xml, &id, &title, &author, &description, &url);

  GHashTable *table = g_hash_table_new (g_direct_hash, g_direct_equal);
  g_hash_table_insert (table, GINT_TO_POINTER (METADATA_KEY_URL), url);
  g_hash_table_insert (table, GINT_TO_POINTER (METADATA_KEY_TITLE), title);

  callback (source, id, table , user_data, NULL);
}

static KeyID *
youtube_source_key_depends (MetadataSource *source, KeyID key_id)
{
  return NULL;
}

static const KeyID *
youtube_source_supported_keys (MetadataSource *source)
{
  static const KeyID keys[] = { METADATA_KEY_TITLE, 
				METADATA_KEY_URL, 0 };
  return keys;
}

static void
youtube_source_class_init (YoutubeSourceClass * klass)
{
  MediaSourceClass *source_class = MEDIA_SOURCE_CLASS (klass);
  MetadataSourceClass *metadata_class = METADATA_SOURCE_CLASS (klass);
  source_class->browse = youtube_source_browse;
  source_class->search = youtube_source_search;
  metadata_class->metadata = youtube_source_metadata;
  metadata_class->supported_keys = youtube_source_supported_keys;
  metadata_class->key_depends = youtube_source_key_depends;
}

static void
youtube_source_init (YoutubeSource *source)
{
  gnome_vfs_init ();
}

G_DEFINE_TYPE (YoutubeSource, youtube_source, MEDIA_SOURCE_TYPE);

YoutubeSource *
youtube_source_new (void)
{
  return g_object_new (YOUTUBE_SOURCE_TYPE,
		       "source-id", "YoutubeSourceId",
		       "source-name", "Youtube Source",
		       "source-desc", "A youtube source",
		       NULL);
}
