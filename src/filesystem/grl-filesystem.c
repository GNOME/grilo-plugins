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

#define GRL_LOG_DOMAIN_DEFAULT filesystem_log_domain
GRL_LOG_DOMAIN_STATIC(filesystem_log_domain);

/* -------- File info ------- */

#define FILE_ATTRIBUTES                         \
  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","    \
  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","    \
  G_FILE_ATTRIBUTE_STANDARD_TYPE ","            \
  G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN ","       \
  G_FILE_ATTRIBUTE_TIME_MODIFIED ","            \
  G_FILE_ATTRIBUTE_THUMBNAIL_PATH ","           \
  G_FILE_ATTRIBUTE_THUMBNAILING_FAILED

#define FILE_ATTRIBUTES_FAST                    \
  G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN

/* ---- Emission chunks ----- */

#define BROWSE_IDLE_CHUNK_SIZE 5

/* --- Plugin information --- */

#define PLUGIN_ID   FILESYSTEM_PLUGIN_ID

#define SOURCE_ID   "grl-filesystem"
#define SOURCE_NAME "Filesystem"
#define SOURCE_DESC "A source for browsing the filesystem"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

/* --- Grilo Filesystem Private --- */

#define GRL_FILESYSTEM_SOURCE_GET_PRIVATE(object)         \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                  \
			     GRL_FILESYSTEM_SOURCE_TYPE,  \
			     GrlFilesystemSourcePrivate))

struct _GrlFilesystemSourcePrivate {
  GList *chosen_paths;
};

/* --- Data types --- */

typedef struct {
  GrlMediaSourceBrowseSpec *spec;
  GList *entries;
  GList *current;
  const gchar *path;
  guint remaining;
}  BrowseIdleData;

static GrlFilesystemSource *grl_filesystem_source_new (void);

static void grl_filesystem_source_finalize (GObject *object);

gboolean grl_filesystem_plugin_init (GrlPluginRegistry *registry,
                                     const GrlPluginInfo *plugin,
                                     GList *configs);

static const GList *grl_filesystem_source_supported_keys (GrlMetadataSource *source);

static void grl_filesystem_source_metadata (GrlMediaSource *source,
                                            GrlMediaSourceMetadataSpec *ms);

static void grl_filesystem_source_browse (GrlMediaSource *source,
                                          GrlMediaSourceBrowseSpec *bs);

/* =================== Filesystem Plugin  =============== */

gboolean
grl_filesystem_plugin_init (GrlPluginRegistry *registry,
                            const GrlPluginInfo *plugin,
                            GList *configs)
{
  GrlConfig *config;
  GList *chosen_paths = NULL;

  GRL_LOG_DOMAIN_INIT (filesystem_log_domain, "filesystem");

  GRL_DEBUG ("filesystem_plugin_init");

  GrlFilesystemSource *source = grl_filesystem_source_new ();
  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source),
                                       NULL);

  for (; configs; configs = g_list_next (configs)) {
    const gchar *path;
    config = GRL_CONFIG (configs->data);
    path = grl_config_get_string (config, GRILO_CONF_CHOSEN_PATH);
    if (!path) {
      continue;
    }
    chosen_paths = g_list_append (chosen_paths, g_strdup (path));
  }
  source->priv->chosen_paths = chosen_paths;

  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_filesystem_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== Filesystem GObject ================ */


G_DEFINE_TYPE (GrlFilesystemSource,
               grl_filesystem_source,
               GRL_TYPE_MEDIA_SOURCE);

static GrlFilesystemSource *
grl_filesystem_source_new (void)
{
  GRL_DEBUG ("grl_filesystem_source_new");
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
  G_OBJECT_CLASS (source_class)->finalize = grl_filesystem_source_finalize;
  metadata_class->supported_keys = grl_filesystem_source_supported_keys;
  g_type_class_add_private (klass, sizeof (GrlFilesystemSourcePrivate));
}

static void
grl_filesystem_source_init (GrlFilesystemSource *source)
{
  source->priv = GRL_FILESYSTEM_SOURCE_GET_PRIVATE (source);
}

static void
grl_filesystem_source_finalize (GObject *object)
{
  GrlFilesystemSource *filesystem_source = GRL_FILESYSTEM_SOURCE (object);
  g_list_foreach (filesystem_source->priv->chosen_paths, (GFunc) g_free, NULL);
  g_list_free (filesystem_source->priv->chosen_paths);
  G_OBJECT_CLASS (grl_filesystem_source_parent_class)->finalize (object);
}

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
    GRL_WARNING ("Failed to get attributes for file '%s': %s",
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
			  GrlMedia *media,
			  gboolean fast)
{
  GDir *dir;
  GError *error = NULL;
  gint count;
  const gchar *entry_name;

  /* Open directory */
  GRL_DEBUG ("Opening directory '%s' for childcount", path);
  dir = g_dir_open (path, 0, &error);
  if (error) {
    GRL_WARNING ("Failed to open directory '%s': %s", path, error->message);
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

  grl_media_box_set_childcount (GRL_MEDIA_BOX (media), count);
}

static GrlMedia *
create_content (GrlMedia *content,
                const gchar *path,
                gboolean only_fast,
		gboolean root_dir)
{
  GrlMedia *media = NULL;
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
    GRL_WARNING ("Failed to get info for file '%s': %s", path,
                 error->message);
    if (!media) {
      media = grl_media_new ();
    }

    /* Title */
    str = g_strrstr (path, G_DIR_SEPARATOR_S);
    if (!str) {
      str = (gchar *) path;
    }
    grl_media_set_title (media, str);
    g_error_free (error);
  } else {
    mime = g_file_info_get_content_type (info);

    if (!media) {
      if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
	media = GRL_MEDIA (grl_media_box_new ());
      } else {
	if (mime_is_video (mime)) {
	  media = grl_media_video_new ();
	} else if (mime_is_audio (mime)) {
	  media = grl_media_audio_new ();
	} else if (mime_is_image (mime)) {
	  media = grl_media_image_new ();
	} else {
	  media = grl_media_new ();
	}
      }
    }

    if (!GRL_IS_MEDIA_BOX (media)) {
      grl_media_set_mime (media, mime);
    }

    /* Title */
    str = (gchar *) g_file_info_get_display_name (info);
    grl_media_set_title (media, str);

    /* Date */
    GTimeVal time;
    gchar *time_str;
    g_file_info_get_modification_time (info, &time);
    time_str = g_time_val_to_iso8601 (&time);
    grl_media_set_date (media, time_str);
    g_free (time_str);

    /* Thumbnail */
    gboolean thumb_failed =
      g_file_info_get_attribute_boolean (info,
                                         G_FILE_ATTRIBUTE_THUMBNAILING_FAILED);
    if (!thumb_failed) {
      const gchar *thumb =
        g_file_info_get_attribute_byte_string (info,
                                               G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
      if (thumb) {
	gchar *thumb_uri = g_filename_to_uri (thumb, NULL, NULL);
	if (thumb_uri) {
	  grl_media_set_thumbnail (media, thumb_uri);
	  g_free (thumb_uri);
	}
      }
    }

    g_object_unref (info);
  }

  grl_media_set_id (media,  root_dir ? NULL : path);

  /* URL */
  str = g_strconcat ("file://", path, NULL);
  grl_media_set_url (media, str);
  g_free (str);

  /* Childcount */
  if (GRL_IS_MEDIA_BOX (media)) {
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

  GRL_DEBUG ("browse_emit_idle");

  idle_data = (BrowseIdleData *) user_data;

  count = 0;
  do {
    gchar *entry_path;
    GrlMedia *content;

    entry_path = (gchar *) idle_data->current->data;
    content = create_content (NULL,
			      entry_path,
			      idle_data->spec->flags & GRL_RESOLVE_FAST_ONLY,
			      FALSE);
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
    g_slice_free (BrowseIdleData, idle_data);
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
  GRL_DEBUG ("Opening directory '%s'", path);
  dir = g_dir_open (path, 0, &error);
  if (error) {
    GRL_WARNING ("Failed to open directory '%s': %s", path, error->message);
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
    BrowseIdleData *idle_data = g_slice_new (BrowseIdleData);
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
  const gchar *id;
  GList *chosen_paths;

  GRL_DEBUG ("grl_filesystem_source_browse");

  id = grl_media_get_id (bs->container);
  chosen_paths = GRL_FILESYSTEM_SOURCE(source)->priv->chosen_paths;
  if (!id && chosen_paths) {
    guint remaining = g_list_length (chosen_paths);

    if (remaining == 1) {
        produce_from_path (bs, chosen_paths->data);
    } else {
      for (; chosen_paths; chosen_paths = g_list_next (chosen_paths)) {
        GrlMedia *content = create_content (NULL,
                                            (gchar *) chosen_paths->data,
                                            GRL_RESOLVE_FAST_ONLY,
                                            FALSE);

        bs->callback (source,
                      bs->browse_id,
                      content,
                      --remaining,
                      bs->user_data,
                      NULL);
      }
    }
  } else {
    produce_from_path (bs, id ? id : G_DIR_SEPARATOR_S);
  }
}

static void
grl_filesystem_source_metadata (GrlMediaSource *source,
                                GrlMediaSourceMetadataSpec *ms)
{
  const gchar *path;
  const gchar *id;

  GRL_DEBUG ("grl_filesystem_source_metadata");

  id = grl_media_get_id (ms->media);
  path = id ? id : G_DIR_SEPARATOR_S;

  if (g_file_test (path, G_FILE_TEST_EXISTS)) {
    create_content (ms->media, path,
		    ms->flags & GRL_RESOLVE_FAST_ONLY,
		    !id);
    ms->callback (ms->source, ms->media, ms->user_data, NULL);
  } else {
    GError *error = g_error_new (GRL_CORE_ERROR,
				 GRL_CORE_ERROR_METADATA_FAILED,
				 "File '%s' does not exist",
				 path);
    ms->callback (ms->source, ms->media, ms->user_data, error);
    g_error_free (error);
  }
}
