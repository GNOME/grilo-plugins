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
#include <string.h>
#include <stdlib.h>

#include "grl-filesystem.h"

/* --------- Logging  -------- */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "grl-filesystem"

/* -------- File info ------- */

#define FILE_ATTRIBUTES                         \
  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","    \
  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","    \
  G_FILE_ATTRIBUTE_STANDARD_TYPE ","            \
  G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN ","       \
  G_FILE_ATTRIBUTE_TIME_MODIFIED

#define FILE_ATTRIBUTES_FAST                    \
  G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN

/* ---- Emission chunks ----- */

#define BROWSE_IDLE_CHUNK_SIZE 5

/* --- Plugin information --- */

#define PLUGIN_ID   "grl-filesystem"
#define PLUGIN_NAME "Filesystem"
#define PLUGIN_DESC "A plugin for browsing the filesystem"

#define SOURCE_ID   "grl-filesystem"
#define SOURCE_NAME "Filesystem"
#define SOURCE_DESC "A source for browsing the filesystem"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

/* --- Data types --- */

typedef struct {
  GrlMediaSourceBrowseSpec *spec;
  GList *entries;
  GList *current;
  const gchar *path;
  guint remaining;
}  BrowseIdleData;

static GrlFilesystemSource *grl_filesystem_source_new (void);

gboolean grl_filesystem_plugin_init (GrlPluginRegistry *registry,
                                     const GrlPluginInfo *plugin,
                                     const GrlConfig *config);

static const GList *grl_filesystem_source_supported_keys (GrlMetadataSource *source);

static void grl_filesystem_source_metadata (GrlMediaSource *source,
                                            GrlMediaSourceMetadataSpec *ms);

static void grl_filesystem_source_browse (GrlMediaSource *source,
                                          GrlMediaSourceBrowseSpec *bs);


/* =================== Filesystem Plugin  =============== */

gboolean
grl_filesystem_plugin_init (GrlPluginRegistry *registry,
                            const GrlPluginInfo *plugin,
                            const GrlConfig *config)
{
  g_debug ("filesystem_plugin_init\n");

  GrlFilesystemSource *source = grl_filesystem_source_new ();
  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source));
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_filesystem_plugin_init,
                     NULL,
                     PLUGIN_ID,
                     PLUGIN_NAME,
                     PLUGIN_DESC,
                     PACKAGE_VERSION,
                     AUTHOR,
                     LICENSE,
                     SITE);

/* ================== Filesystem GObject ================ */

static GrlFilesystemSource *
grl_filesystem_source_new (void)
{
  g_debug ("grl_filesystem_source_new");
  return g_object_new (GRL_FILESYSTEM_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       NULL);
}

static void
grl_filesystem_source_class_init (GrlFilesystemSourceClass * klass)
{
  GrlMediaSourceClass *source_class = GRL_MEDIA_SOURCE_CLASS (klass);
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);
  source_class->browse = grl_filesystem_source_browse;
  source_class->metadata = grl_filesystem_source_metadata;
  metadata_class->supported_keys = grl_filesystem_source_supported_keys;
}

static void
grl_filesystem_source_init (GrlFilesystemSource *source)
{
}

G_DEFINE_TYPE (GrlFilesystemSource,
               grl_filesystem_source,
               GRL_TYPE_MEDIA_SOURCE);

/* ======================= Utilities ==================== */

static gboolean
mime_is_video (const gchar *mime)
{
  return strstr (mime, "video") != NULL;
}

static gboolean
mime_is_audio (const gchar *mime)
{
  return strstr (mime, "audio") != NULL;
}

static gboolean
mime_is_image (const gchar *mime)
{
  return strstr (mime, "image") != NULL;
}

static gboolean
mime_is_media (const gchar *mime)
{
  if (!mime)
    return FALSE;
  if (!strcmp (mime, "inode/directory"))
    return TRUE;
  if (mime_is_audio (mime))
    return TRUE;
  if (mime_is_video (mime))
    return TRUE;
  if (mime_is_image (mime))
    return TRUE;
  return FALSE;
}

static gboolean
file_is_valid_content (const gchar *path, gboolean fast)
{
  const gchar *mime;
  GError *error = NULL;
  gboolean is_media;
  GFile *file;
  GFileInfo *info;
  GFileType type;
  const gchar *spec;

  if (fast) {
    spec = FILE_ATTRIBUTES_FAST;
  } else {
    spec = FILE_ATTRIBUTES;
  }

  file = g_file_new_for_path (path);
  info = g_file_query_info (file, spec, 0, NULL, &error);
  if (error) {
    g_warning ("Failed to get attributes for file '%s': %s",
	       path, error->message);
    g_error_free (error);
    g_object_unref (file);
    return FALSE;
  } else {
    if (g_file_info_get_is_hidden (info)) {
      is_media = FALSE;
    } else {
      if (fast) {
	/* In fast mode we do not check mime-types,
	   any non-hidden file is accepted */
	is_media = TRUE;
      } else {
	type = g_file_info_get_file_type (info);
	mime = g_file_info_get_content_type (info);
	if (type == G_FILE_TYPE_DIRECTORY || mime_is_media (mime)) {
	  is_media = TRUE;
	} else {
	  is_media = FALSE;
	}
      }
    }
    g_object_unref (info);
    g_object_unref (file);
    return is_media;
  }
}

static void
set_container_childcount (const gchar *path,
			  GrlDataMedia *media,
			  gboolean fast)
{
  GDir *dir;
  GError *error = NULL;
  gint count;
  const gchar *entry_name;

  /* Open directory */
  g_debug ("Opening directory '%s' for childcount", path);
  dir = g_dir_open (path, 0, &error);
  if (error) {
    g_warning ("Failed to open directory '%s': %s", path, error->message);
    g_error_free (error);
    return;
  }

  /* Count valid entries */
  count = 0;
  while ((entry_name = g_dir_read_name (dir)) != NULL) {
    gchar *entry_path;
    if (strcmp (path, G_DIR_SEPARATOR_S)) {
      entry_path = g_strconcat (path, G_DIR_SEPARATOR_S, entry_name, NULL);
    } else {
      entry_path = g_strconcat (path, entry_name, NULL);
    }
    if (file_is_valid_content (entry_path, fast)) {
      if (fast) {
        /* in fast mode we don't compute  mime-types because it is slow,
           so we can only check if the directory is totally empty (no subdirs,
           and no files), otherwise we just say we do not know the actual
           childcount */
        count = GRL_METADATA_KEY_CHILDCOUNT_UNKNOWN;
        break;
      }
      count++;
    }
    g_free (entry_path);
  }

  g_dir_close (dir);

  grl_data_box_set_childcount (GRL_DATA_BOX (media), count);
}

static GrlDataMedia *
create_content (GrlDataMedia *content,
                const gchar *path,
                gboolean only_fast)
{
  GrlDataMedia *media = NULL;
  gchar *str;
  const gchar *mime;
  GError *error = NULL;

  GFile *file = g_file_new_for_path (path);
  GFileInfo *info = g_file_query_info (file,
				       FILE_ATTRIBUTES,
				       0,
				       NULL,
				       &error);

  /* Update mode */
  if (content) {
    media = content;
  }

  if (error) {
    g_warning ("Failed to get info for file '%s': %s", path, error->message);
    if (!media) {
      media = grl_data_media_new ();
    }

    /* Title */
    str = g_strrstr (path, G_DIR_SEPARATOR_S);
    if (!str) {
      str = (gchar *) path;
    }
    grl_data_media_set_title (media, str);
    g_error_free (error);
  } else {
    mime = g_file_info_get_content_type (info);

    if (!media) {
      if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
	media = GRL_DATA_MEDIA (grl_data_box_new ());
      } else {
	if (mime_is_video (mime)) {
	  media = grl_data_video_new ();
	} else if (mime_is_audio (mime)) {
	  media = grl_data_audio_new ();
	} else if (mime_is_image (mime)) {
	  media = grl_data_image_new ();
	} else {
	  media = grl_data_media_new ();
	}
      }
    }

    if (!GRL_IS_DATA_BOX (media)) {
      grl_data_media_set_mime (GRL_DATA (media), mime);
    }

    /* Title */
    str = (gchar *) g_file_info_get_display_name (info);
    grl_data_media_set_title (media, str);

    /* Date */
    GTimeVal time;
    gchar *time_str;
    g_file_info_get_modification_time (info, &time);
    time_str = g_time_val_to_iso8601 (&time);
    grl_data_media_set_date (GRL_DATA (media), time_str);
    g_free (time_str);

    g_object_unref (info);
  }

  /* ID */
  grl_data_media_set_id (media, path);

  /* URL */
  str = g_strconcat ("file://", path, NULL);
  grl_data_media_set_url (media, str);
  g_free (str);

  /* Childcount */
  if (GRL_IS_DATA_BOX (media)) {
    set_container_childcount (path, media, only_fast);
  }

  g_object_unref (file);

  return media;
}

static gboolean
browse_emit_idle (gpointer user_data)
{
  BrowseIdleData *idle_data;
  guint count;

  g_debug ("browse_emit_idle");

  idle_data = (BrowseIdleData *) user_data;

  count = 0;
  do {
    gchar *entry_path;
    GrlDataMedia *content;

    entry_path = (gchar *) idle_data->current->data;
    content = create_content (NULL,
			      entry_path,
			      idle_data->spec->flags & GRL_RESOLVE_FAST_ONLY);
    g_free (idle_data->current->data);

    idle_data->spec->callback (idle_data->spec->source,
			       idle_data->spec->browse_id,
			       content,
			       idle_data->remaining--,
			       idle_data->spec->user_data,
			       NULL);

    idle_data->current = g_list_next (idle_data->current);
    count++;
  } while (count < BROWSE_IDLE_CHUNK_SIZE && idle_data->current);

  if (!idle_data->current) {
    g_list_free (idle_data->entries);
    g_free (idle_data);
    return FALSE;
  } else {
    return TRUE;
  }
}

static void
produce_from_path (GrlMediaSourceBrowseSpec *bs, const gchar *path)
{
  GDir *dir;
  GError *error = NULL;
  const gchar *entry;
  guint skip, count;
  GList *entries = NULL;
  GList *iter;

  /* Open directory */
  g_debug ("Opening directory '%s'", path);
  dir = g_dir_open (path, 0, &error);
  if (error) {
    g_warning ("Failed to open directory '%s': %s", path, error->message);
    bs->callback (bs->source, bs->browse_id, NULL, 0, bs->user_data, error);
    g_error_free (error);
    return;
  }

  /* Filter out media and directories */
  while ((entry = g_dir_read_name (dir)) != NULL) {
    gchar *file;
    if (strcmp (path, G_DIR_SEPARATOR_S)) {
      file = g_strconcat (path, G_DIR_SEPARATOR_S, entry, NULL);
    } else {
      file = g_strconcat (path, entry, NULL);
    }
    if (file_is_valid_content (file, FALSE)) {
      entries = g_list_prepend (entries, file);
    }
  }

  /* Apply skip and count */
  skip = bs->skip;
  count = bs->count;
  iter = entries;
  while (iter) {
    gboolean remove;
    GList *tmp;
    if (skip > 0)  {
      skip--;
      remove = TRUE;
    } else if (count > 0) {
      count--;
      remove = FALSE;
    } else {
      remove = TRUE;
    }
    if (remove) {
      tmp = iter;
      iter = g_list_next (iter);
      g_free (tmp->data);
      entries = g_list_delete_link (entries, tmp);
    } else {
      iter = g_list_next (iter);
    }
  }

  /* Emit results */
  if (entries) {
    /* Use the idle loop to avoid blocking for too long */
    BrowseIdleData *idle_data = g_new (BrowseIdleData, 1);
    idle_data->spec = bs;
    idle_data->remaining = bs->count - count - 1;
    idle_data->path = path;
    idle_data->entries = entries;
    idle_data->current = entries;
    g_idle_add (browse_emit_idle, idle_data);
  } else {
    /* No results */
    bs->callback (bs->source,
		  bs->browse_id,
		  NULL,
		  0,
		  bs->user_data,
		  NULL);
  }

  g_dir_close (dir);
}

/* ================== API Implementation ================ */

static const GList *
grl_filesystem_source_supported_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_URL,
                                      GRL_METADATA_KEY_MIME,
                                      GRL_METADATA_KEY_DATE,
                                      GRL_METADATA_KEY_CHILDCOUNT,
                                      NULL);
  }
  return keys;
}

static void
grl_filesystem_source_browse (GrlMediaSource *source,
                              GrlMediaSourceBrowseSpec *bs)
{
  const gchar *path;
  const gchar *id;

  g_debug ("grl_filesystem_source_browse");

  id = grl_data_media_get_id (bs->container);
  path = id ? id : G_DIR_SEPARATOR_S;
  produce_from_path (bs, path);
}

static void
grl_filesystem_source_metadata (GrlMediaSource *source,
                                GrlMediaSourceMetadataSpec *ms)
{
  const gchar *path;
  const gchar *id;

  g_debug ("grl_filesystem_source_metadata");

  id = grl_data_media_get_id (ms->media);
  path = id ? id : G_DIR_SEPARATOR_S;

  if (g_file_test (path, G_FILE_TEST_EXISTS)) {
    create_content (ms->media, path,
		    ms->flags & GRL_RESOLVE_FAST_ONLY);
    ms->callback (ms->source, ms->media, ms->user_data, NULL);
  } else {
    GError *error = g_error_new (GRL_ERROR,
				 GRL_ERROR_METADATA_FAILED,
				 "File '%s' does not exist",
				 path);
    ms->callback (ms->source, ms->media, ms->user_data, error);
    g_error_free (error);
  }
}
