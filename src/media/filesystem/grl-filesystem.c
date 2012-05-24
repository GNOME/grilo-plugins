/*
 * Copyright (C) 2010, 2011 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
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

/* --- Grilo Filesystem Private --- */

#define GRL_FILESYSTEM_SOURCE_GET_PRIVATE(object)         \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                  \
			     GRL_FILESYSTEM_SOURCE_TYPE,  \
			     GrlFilesystemSourcePrivate))

struct _GrlFilesystemSourcePrivate {
  GList *chosen_paths;
  guint max_search_depth;
  /* a mapping operation_id -> GCancellable to cancel this operation */
  GHashTable *cancellables;
  /* Monitors for changes in directories */
  GList *monitors;
  GCancellable *cancellable_monitors;
};

/* --- Data types --- */

typedef struct _RecursiveOperation RecursiveOperation;

typedef gboolean (*RecursiveOperationCb) (GFileInfo *file_info,
                                          RecursiveOperation *operation);

typedef struct {
  GrlMediaSourceBrowseSpec *spec;
  GList *entries;
  GList *current;
  const gchar *path;
  guint remaining;
  GCancellable *cancellable;
  guint id;
}  BrowseIdleData;

struct _RecursiveOperation {
  RecursiveOperationCb on_cancel;
  RecursiveOperationCb on_finish;
  RecursiveOperationCb on_dir;
  RecursiveOperationCb on_file;
  gpointer on_dir_data;
  gpointer on_file_data;
  GCancellable *cancellable;
  GQueue *directories;
  guint max_depth;
};

typedef struct {
  guint depth;
  GFile *directory;
} RecursiveEntry;


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

static void grl_filesystem_source_search (GrlMediaSource *source,
                                          GrlMediaSourceSearchSpec *ss);


static gboolean grl_filesystem_test_media_from_uri (GrlMediaSource *source,
                                                    const gchar *uri);

static void grl_filesystem_get_media_from_uri (GrlMediaSource *source,
                                               GrlMediaSourceMediaFromUriSpec *mfus);

static void grl_filesystem_source_cancel (GrlMetadataSource *source,
                                          guint operation_id);

static gboolean grl_filesystem_source_notify_change_start (GrlMediaSource *source,
                                                           GError **error);

static gboolean grl_filesystem_source_notify_change_stop (GrlMediaSource *source,
                                                          GError **error);

/* =================== Filesystem Plugin  =============== */

gboolean
grl_filesystem_plugin_init (GrlPluginRegistry *registry,
                            const GrlPluginInfo *plugin,
                            GList *configs)
{
  GrlConfig *config;
  GList *chosen_paths = NULL;
  guint max_search_depth = GRILO_CONF_MAX_SEARCH_DEPTH_DEFAULT;

  GRL_LOG_DOMAIN_INIT (filesystem_log_domain, "filesystem");

  GRL_DEBUG ("filesystem_plugin_init");

  GrlFilesystemSource *source = grl_filesystem_source_new ();

  for (; configs; configs = g_list_next (configs)) {
    gchar *path;
    config = GRL_CONFIG (configs->data);
    path = grl_config_get_string (config, GRILO_CONF_CHOSEN_PATH);
    if (path) {
      chosen_paths = g_list_append (chosen_paths, path);
    }
    if (grl_config_has_param (config, GRILO_CONF_MAX_SEARCH_DEPTH)) {
      max_search_depth = (guint)grl_config_get_int (config, GRILO_CONF_MAX_SEARCH_DEPTH);
    }
  }
  source->priv->chosen_paths = chosen_paths;
  source->priv->max_search_depth = max_search_depth;

  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source),
                                       NULL);

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
  source_class->search = grl_filesystem_source_search;
  source_class->notify_change_start = grl_filesystem_source_notify_change_start;
  source_class->notify_change_stop = grl_filesystem_source_notify_change_stop;
  source_class->metadata = grl_filesystem_source_metadata;
  source_class->test_media_from_uri = grl_filesystem_test_media_from_uri;
  source_class->media_from_uri = grl_filesystem_get_media_from_uri;
  G_OBJECT_CLASS (source_class)->finalize = grl_filesystem_source_finalize;
  metadata_class->supported_keys = grl_filesystem_source_supported_keys;
  metadata_class->cancel = grl_filesystem_source_cancel;
  g_type_class_add_private (klass, sizeof (GrlFilesystemSourcePrivate));
}

static void
grl_filesystem_source_init (GrlFilesystemSource *source)
{
  source->priv = GRL_FILESYSTEM_SOURCE_GET_PRIVATE (source);
  source->priv->cancellables = g_hash_table_new (NULL, NULL);
}

static void
grl_filesystem_source_finalize (GObject *object)
{
  GrlFilesystemSource *filesystem_source = GRL_FILESYSTEM_SOURCE (object);
  g_list_free_full (filesystem_source->priv->chosen_paths, g_free);
  g_hash_table_unref (filesystem_source->priv->cancellables);
  G_OBJECT_CLASS (grl_filesystem_source_parent_class)->finalize (object);
}

/* ======================= Utilities ==================== */

static void recursive_operation_next_entry (RecursiveOperation *operation);
static void add_monitor (GrlFilesystemSource *fs_source, GFile *dir);
static void cancel_monitors (GrlFilesystemSource *fs_source);

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
    GRL_DEBUG ("Failed to get attributes for file '%s': %s",
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
    GRL_DEBUG ("Failed to open directory '%s': %s", path, error->message);
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
  gchar *extension;
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
    GRL_DEBUG ("Failed to get info for file '%s': %s", path,
               error->message);
    if (!media) {
      media = grl_media_new ();
      grl_media_set_id (media,  root_dir ? NULL : path);
    }

    /* Title */
    str = g_strdup (g_strrstr (path, G_DIR_SEPARATOR_S));
    if (!str) {
      str = g_strdup (path);
    }

    /* Remove file extension */
    extension = g_strrstr (str, ".");
    if (extension) {
      *extension = '\0';
    }

    grl_media_set_title (media, str);
    g_error_free (error);
    g_free (str);
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
      grl_media_set_id (media,  root_dir ? NULL : path);
    }

    if (!GRL_IS_MEDIA_BOX (media)) {
      grl_media_set_mime (media, mime);
    }

    /* Title */
    str = g_strdup (g_file_info_get_display_name (info));

    /* Remove file extension */
    extension = g_strrstr (str, ".");
    if (extension) {
      *extension = '\0';
    }

    grl_media_set_title (media, str);
    g_free (str);

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

  /* URL */
  str = g_file_get_uri (file);
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
  GrlFilesystemSource *fs_source;

  GRL_DEBUG ("browse_emit_idle");

  idle_data = (BrowseIdleData *) user_data;
  fs_source = GRL_FILESYSTEM_SOURCE (idle_data->spec->source);

  if (g_cancellable_is_cancelled (idle_data->cancellable)) {
    GRL_DEBUG ("Browse operation %d (\"%s\") has been cancelled",
               idle_data->id, idle_data->path);
    idle_data->spec->callback(idle_data->spec->source,
                              idle_data->id, NULL, 0,
                              idle_data->spec->user_data, NULL);
    goto finish;
  }

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

  if (!idle_data->current)
    goto finish;

  return TRUE;

finish:
    g_list_free (idle_data->entries);
    g_hash_table_remove (fs_source->priv->cancellables,
                         GUINT_TO_POINTER (idle_data->id));
    g_object_unref (idle_data->cancellable);
    g_slice_free (BrowseIdleData, idle_data);
    return FALSE;
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
    GRL_DEBUG ("Failed to open directory '%s': %s", path, error->message);
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
    idle_data->cancellable = g_cancellable_new ();
    idle_data->id = bs->browse_id;
    g_hash_table_insert (GRL_FILESYSTEM_SOURCE (bs->source)->priv->cancellables,
                         GUINT_TO_POINTER (bs->browse_id),
                         idle_data->cancellable);

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

static RecursiveEntry *
recursive_entry_new (guint depth, GFile *directory)
{
  RecursiveEntry *entry;

  entry = g_slice_new(RecursiveEntry);
  entry->depth = depth;
  entry->directory = g_object_ref (directory);

  return entry;
}

static void
recursive_entry_free (RecursiveEntry *entry)
{
  g_object_unref (entry->directory);
  g_slice_free (RecursiveEntry, entry);
}

static RecursiveOperation *
recursive_operation_new ()
{
  RecursiveOperation *operation;

  operation = g_slice_new0 (RecursiveOperation);
  operation->directories = g_queue_new ();
  operation->cancellable = g_cancellable_new ();

  return operation;
}

static void
recursive_operation_free (RecursiveOperation *operation)
{
  g_queue_foreach (operation->directories, (GFunc) recursive_entry_free, NULL);
  g_queue_free (operation->directories);
  g_object_unref (operation->cancellable);
  g_slice_free (RecursiveOperation, operation);
}

static void
recursive_operation_got_file (GFileEnumerator *enumerator, GAsyncResult *res, RecursiveOperation *operation)
{
  GList *files;
  GError *error = NULL;
  gboolean continue_operation = TRUE;

  GRL_DEBUG (__func__);

  files = g_file_enumerator_next_files_finish (enumerator, res, &error);
  if (error) {
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      GRL_WARNING ("Got error: %s", error->message);
    g_error_free (error);
    goto finished;
  }

  if (files) {
    GFileInfo *file_info;
    RecursiveEntry *entry;

    /* we assume there is only one GFileInfo in the list since that's what we ask
     * for when calling g_file_enumerator_next_files_async() */
    file_info = (GFileInfo *)files->data;
    g_list_free (files);
    /* Get the entry we are running now */
    entry = g_queue_peek_head (operation->directories);
    switch (g_file_info_get_file_type (file_info)) {
    case G_FILE_TYPE_SYMBOLIC_LINK:
      /* we're too afraid of infinite recursion to touch this for now */
      break;
    case G_FILE_TYPE_DIRECTORY:
        {
          if (entry->depth < operation->max_depth) {
            GFile *subdir;
            RecursiveEntry *subentry;

            if (operation->on_dir) {
              continue_operation = operation->on_dir(file_info, operation);
            }

            if (continue_operation) {
              subdir = g_file_get_child (entry->directory,
                                         g_file_info_get_name (file_info));
              subentry = recursive_entry_new (entry->depth + 1, subdir);
              g_queue_push_tail (operation->directories, subentry);
              g_object_unref (subdir);
            } else {
              g_object_unref (file_info);
              goto finished;
            }
          }
        }
      break;
    case G_FILE_TYPE_REGULAR:
      if (operation->on_file) {
        continue_operation = operation->on_file(file_info, operation);
        if (!continue_operation) {
          g_object_unref (file_info);
          goto finished;
        }
      }
      break;
    default:
      /* this file is a weirdo, we ignore it */
      break;
    }
    g_object_unref (file_info);
  } else {    /* end of enumerator */
    goto finished;
  }

  g_file_enumerator_next_files_async (enumerator, 1, G_PRIORITY_DEFAULT,
                                      operation->cancellable,
                                      (GAsyncReadyCallback)recursive_operation_got_file,
                                      operation);

  return;

finished:
  /* we're done with this dir/enumerator, let's treat the next one */
  g_object_unref (enumerator);
  recursive_entry_free (g_queue_pop_head (operation->directories));
  if (continue_operation) {
    recursive_operation_next_entry (operation);
  } else {
    recursive_operation_free (operation);
  }
}

static void
recursive_operation_got_entry (GFile *directory, GAsyncResult *res, RecursiveOperation *operation)
{
  GError *error = NULL;
  GFileEnumerator *enumerator;

  GRL_DEBUG (__func__);

  enumerator = g_file_enumerate_children_finish (directory, res, &error);
  if (error) {
    GRL_WARNING ("Got error: %s", error->message);
    g_error_free (error);
    g_object_unref (enumerator);
    /* we couldn't get the children of this directory, but we probably have
     * other directories to try */
    recursive_entry_free (g_queue_pop_head (operation->directories));
    recursive_operation_next_entry (operation);
    return;
  }

  g_file_enumerator_next_files_async (enumerator, 1, G_PRIORITY_DEFAULT,
                                      operation->cancellable,
                                      (GAsyncReadyCallback)recursive_operation_got_file,
                                      operation);
}

static void
recursive_operation_next_entry (RecursiveOperation *operation)
{
  RecursiveEntry *entry;

  GRL_DEBUG (__func__);

  if (g_cancellable_is_cancelled (operation->cancellable)) {
    /* We've been cancelled! */
    GRL_DEBUG ("Operation has been cancelled");
    if (operation->on_cancel) {
      operation->on_cancel(NULL, operation);
    }
    goto finished;
  }

  entry = g_queue_peek_head (operation->directories);
  if (!entry) { /* We've crawled everything, before reaching count */
    if (operation->on_finish) {
      operation->on_finish (NULL, operation);
    }
    goto finished;
  }

  g_file_enumerate_children_async (entry->directory, G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                   G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                   G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                   G_PRIORITY_DEFAULT,
                                   operation->cancellable,
                                   (GAsyncReadyCallback)recursive_operation_got_entry,
                                   operation);

  return;

finished:
  recursive_operation_free (operation);
}

static void
recursive_operation_initialize (RecursiveOperation *operation, GrlFilesystemSource *source)
{
  GList *chosen_paths, *path;

  chosen_paths = source->priv->chosen_paths;
  if (chosen_paths) {
    for (path = chosen_paths; path; path = g_list_next (path)) {
      GFile *directory = g_file_new_for_path (path->data);
      g_queue_push_tail (operation->directories,
                         recursive_entry_new (0, directory));
      g_object_unref (directory);
    }
  } else {
    const gchar *home;
    GFile *directory;

    home = g_getenv ("HOME");
    directory = g_file_new_for_path (home);
    g_queue_push_tail (operation->directories,
                       recursive_entry_new (0, directory));
    g_object_unref (directory);
  }
}

static gboolean
cancel_cb (GFileInfo *file_info, RecursiveOperation *operation)
{
  GrlFilesystemSource *fs_source;

  if (operation->on_file_data) {
    GrlMediaSourceSearchSpec *ss =
      (GrlMediaSourceSearchSpec *) operation->on_file_data;
    fs_source = GRL_FILESYSTEM_SOURCE (ss->source);
    g_hash_table_remove (fs_source->priv->cancellables,
                         GUINT_TO_POINTER (ss->search_id));
    ss->callback (ss->source, ss->search_id, NULL, 0, ss->user_data, NULL);
  }

  if (operation->on_dir_data) {
    /* Remove all monitors */
    fs_source = GRL_FILESYSTEM_SOURCE (operation->on_dir_data);
    cancel_monitors (fs_source);
  }
  return FALSE;
}

static gboolean
finish_cb (GFileInfo *file_info, RecursiveOperation *operation)
{
  if (operation->on_file_data) {
    GrlMediaSourceSearchSpec *ss =
      (GrlMediaSourceSearchSpec *) operation->on_file_data;
    g_hash_table_remove (GRL_FILESYSTEM_SOURCE (ss->source)->priv->cancellables,
                         GUINT_TO_POINTER (ss->search_id));
    ss->callback (ss->source, ss->search_id, NULL, 0, ss->user_data, NULL);
  }

  if (operation->on_dir_data) {
    GRL_FILESYSTEM_SOURCE (operation->on_dir_data)->priv->cancellable_monitors = NULL;
  }

  return FALSE;
}

/* return TRUE if more files need to be returned, FALSE if we sent the count */
static gboolean
file_cb (GFileInfo *file_info, RecursiveOperation *operation)
{
  gchar *needle = NULL;
  gchar *haystack = NULL;
  gchar *normalized_needle = NULL;
  gchar *normalized_haystack = NULL;
  GrlMediaSourceSearchSpec *ss = operation->on_file_data;
  gint remaining = -1;

  GRL_DEBUG (__func__);

  if (ss == NULL)
    return FALSE;

  if (ss->text) {
    haystack = g_utf8_casefold (g_file_info_get_display_name (file_info), -1);
    normalized_haystack = g_utf8_normalize (haystack, -1, G_NORMALIZE_ALL);

    needle = g_utf8_casefold (ss->text, -1);
    normalized_needle = g_utf8_normalize (needle, -1, G_NORMALIZE_ALL);
  }

  if (!ss->text ||
      strstr (normalized_haystack, normalized_needle)) {
    GrlMedia *media = NULL;
    RecursiveEntry *entry;
    GFile *file;
    gchar *path;

    entry = g_queue_peek_head (operation->directories);
    file = g_file_get_child (entry->directory,
                             g_file_info_get_name (file_info));
    path = g_file_get_path (file);

    /* FIXME: both file_is_valid_content() and create_content() are likely to block */
    if (file_is_valid_content (path, FALSE)) {
      if (ss->skip) {
        ss->skip--;
      } else {
        media = create_content (NULL, path, ss->flags & GRL_RESOLVE_FAST_ONLY, FALSE);
      }
    }

    g_free (path);
    g_object_unref (file);

    if (media) {
      ss->count--;
      if (ss->count == 0) {
        remaining = 0;
      }
      ss->callback (ss->source, ss->search_id, media, remaining, ss->user_data, NULL);
    }
  }

  g_free (haystack);
  g_free (normalized_haystack);
  g_free (needle);
  g_free (normalized_needle);
  return remaining == -1;
}

static void
notify_parent_change (GrlMediaSource *source, GFile *child, GrlMediaSourceChangeType change)
{
  GFile *parent;
  GrlMedia *media;
  gchar *parent_path;

  parent = g_file_get_parent (child);
  if (parent) {
    parent_path = g_file_get_path (parent);
  } else {
    parent_path = g_strdup ("/");
  }

  media = create_content (NULL, parent_path, GRL_RESOLVE_FAST_ONLY, parent == NULL);
  grl_media_source_notify_change (source, media, change, FALSE);
  g_object_unref (media);

  if (parent) {
    g_object_unref (parent);
  }
  g_free (parent_path);
}

static void
directory_changed (GFileMonitor *monitor,
                   GFile *file,
                   GFile *other_file,
                   GFileMonitorEvent event,
                   gpointer data)
{
  GrlMediaSource *source = GRL_MEDIA_SOURCE (data);
  gchar *file_path, *other_file_path;
  gchar *file_parent_path = NULL;
  gchar *other_file_parent_path = NULL;
  GFile *file_parent, *other_file_parent;
  GFileInfo *file_info;

  if (event == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT ||
      event == G_FILE_MONITOR_EVENT_CREATED) {
    file_path = g_file_get_path (file);
    if (file_is_valid_content (file_path, TRUE)) {
      notify_parent_change (source,
                            file,
                            (event == G_FILE_MONITOR_EVENT_CREATED)? GRL_CONTENT_ADDED: GRL_CONTENT_CHANGED);
      if (event == G_FILE_MONITOR_EVENT_CREATED) {
        file_info = g_file_query_info (file,
                                       G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                       NULL,
                                       NULL);
        if (file_info) {
          if (g_file_info_get_file_type (file_info) == G_FILE_TYPE_DIRECTORY) {
            add_monitor (GRL_FILESYSTEM_SOURCE (source), file);
          }
          g_object_unref (file_info);
        }
      }
    }
    g_free (file_path);
  } else if (event == G_FILE_MONITOR_EVENT_DELETED) {
    notify_parent_change (source, file, GRL_CONTENT_REMOVED);
  } else if (event == G_FILE_MONITOR_EVENT_MOVED) {
    other_file_path = g_file_get_path (other_file);
    if (file_is_valid_content (other_file_path, TRUE)) {
      file_parent = g_file_get_parent (file);
      if (file_parent) {
        file_parent_path = g_file_get_path (file_parent);
        g_object_unref (file_parent);
      } else {
        file_parent_path = NULL;
      }
      other_file_parent = g_file_get_parent (other_file);
      if (other_file_parent) {
        other_file_parent_path = g_file_get_path (other_file_parent);
        g_object_unref (other_file_parent);
      } else {
        other_file_parent_path = NULL;
      }

      if (g_strcmp0 (file_parent_path, other_file_parent_path) == 0) {
        notify_parent_change (source, file, GRL_CONTENT_CHANGED);
      } else {
        notify_parent_change (source, file, GRL_CONTENT_REMOVED);
        notify_parent_change (source, other_file, GRL_CONTENT_ADDED);
      }
    }
    g_free (file_parent_path);
    g_free (other_file_parent_path);
  }
}

static void
cancel_monitors (GrlFilesystemSource *fs_source)
{
  g_list_foreach (fs_source->priv->monitors,
                  (GFunc) g_file_monitor_cancel,
                  NULL);
  g_list_free_full (fs_source->priv->monitors, g_object_unref);
  fs_source->priv->monitors = NULL;
}

static void
add_monitor (GrlFilesystemSource *fs_source, GFile *dir)
{
  GFileMonitor *monitor;

  monitor = g_file_monitor_directory (dir, G_FILE_MONITOR_SEND_MOVED, NULL, NULL);
  if (monitor) {
    fs_source->priv->monitors = g_list_prepend (fs_source->priv->monitors,
                                                monitor);
    g_signal_connect (monitor,
                      "changed",
                      G_CALLBACK (directory_changed),
                      fs_source);
  } else {
    GRL_DEBUG ("Unable to set up monitor in %s\n", g_file_get_path (dir));
  }
}

static gboolean
directory_cb (GFileInfo *dir_info, RecursiveOperation *operation)
{
  RecursiveEntry *entry;
  GFile *dir;
  GrlFilesystemSource *fs_source;

  fs_source = GRL_FILESYSTEM_SOURCE (operation->on_dir_data);
  entry = g_queue_peek_head (operation->directories);
  dir = g_file_get_child (entry->directory,
                          g_file_info_get_name (dir_info));

  add_monitor (fs_source, dir);
  g_object_unref (dir);

  return TRUE;
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

static void grl_filesystem_source_search (GrlMediaSource *source,
                                          GrlMediaSourceSearchSpec *ss)
{
  RecursiveOperation *operation;
  GrlFilesystemSource *fs_source;

  GRL_DEBUG ("grl_filesystem_source_search");

  fs_source = GRL_FILESYSTEM_SOURCE (source);

  operation = recursive_operation_new ();
  operation->on_cancel = cancel_cb;
  operation->on_finish = finish_cb;
  operation->on_file = file_cb;
  operation->on_file_data = ss;
  operation->max_depth = fs_source->priv->max_search_depth;
  g_hash_table_insert (GRL_FILESYSTEM_SOURCE (source)->priv->cancellables,
                       GUINT_TO_POINTER (ss->search_id),
                       operation->cancellable);

  recursive_operation_initialize (operation, fs_source);
  recursive_operation_next_entry (operation);
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
    ms->callback (ms->source, ms->metadata_id, ms->media, ms->user_data, NULL);
  } else {
    GError *error = g_error_new (GRL_CORE_ERROR,
				 GRL_CORE_ERROR_METADATA_FAILED,
				 "File '%s' does not exist",
				 path);
    ms->callback (ms->source, ms->metadata_id, ms->media, ms->user_data, error);
    g_error_free (error);
  }
}

static gboolean
grl_filesystem_test_media_from_uri (GrlMediaSource *source,
                                    const gchar *uri)
{
  gchar *path, *scheme;
  GError *error = NULL;
  gboolean ret = FALSE;

  GRL_DEBUG ("grl_filesystem_test_media_from_uri");

  scheme = g_uri_parse_scheme (uri);
  ret = (g_strcmp0(scheme, "file") == 0);
  g_free (scheme);
  if (!ret)
    return ret;

  path = g_filename_from_uri (uri, NULL, &error);
  if (error != NULL) {
    g_error_free (error);
    return FALSE;
  }

  ret = file_is_valid_content (path, TRUE);

  g_free (path);
  return ret;
}

static void grl_filesystem_get_media_from_uri (GrlMediaSource *source,
                                               GrlMediaSourceMediaFromUriSpec *mfus)
{
  gchar *path, *scheme;
  GError *error = NULL;
  gboolean ret = FALSE;
  GrlMedia *media;

  GRL_DEBUG ("grl_filesystem_get_media_from_uri");

  scheme = g_uri_parse_scheme (mfus->uri);
  ret = (g_strcmp0(scheme, "file") == 0);
  g_free (scheme);
  if (!ret) {
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_MEDIA_FROM_URI_FAILED,
                         "Cannot create media from '%s'", mfus->uri);
    mfus->callback (source, mfus->media_from_uri_id, NULL, mfus->user_data, error);
    g_clear_error (&error);
    return;
  }

  path = g_filename_from_uri (mfus->uri, NULL, &error);
  if (error != NULL) {
    GError *new_error;
    new_error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_MEDIA_FROM_URI_FAILED,
                         "Cannot create media from '%s', error message: %s",
                         mfus->uri, error->message);
    g_clear_error (&error);
    mfus->callback (source, mfus->media_from_uri_id, NULL, mfus->user_data, new_error);
    g_clear_error (&new_error);
    goto beach;
  }

  /* FIXME: this is a blocking call, not sure we want that in here */
  /* Note: we assume create_content() never returns NULL, which seems to be true */
  media = create_content (NULL, path, mfus->flags & GRL_RESOLVE_FAST_ONLY,
                          FALSE);
  mfus->callback (source, mfus->media_from_uri_id, media, mfus->user_data, NULL);

beach:
  g_free (path);
}

static void
grl_filesystem_source_cancel (GrlMetadataSource *source, guint operation_id)
{
  GCancellable *cancellable;
  GrlFilesystemSourcePrivate *priv;

  priv = GRL_FILESYSTEM_SOURCE (source)->priv;

  cancellable =
      G_CANCELLABLE (g_hash_table_lookup (priv->cancellables,
                                          GUINT_TO_POINTER (operation_id)));
  if (cancellable)
    g_cancellable_cancel (cancellable);
}

static gboolean
grl_filesystem_source_notify_change_start (GrlMediaSource *source,
                                           GError **error)
{
  GrlFilesystemSource *fs_source;
  RecursiveOperation *operation;

  GRL_DEBUG (__func__);

  fs_source = GRL_FILESYSTEM_SOURCE (source);
  operation = recursive_operation_new ();
  operation->on_cancel = cancel_cb;
  operation->on_finish = finish_cb;
  operation->on_dir = directory_cb;
  operation->on_dir_data = fs_source;
  operation->max_depth = fs_source->priv->max_search_depth;

  fs_source->priv->cancellable_monitors = operation->cancellable;

  recursive_operation_initialize (operation, fs_source);
  recursive_operation_next_entry (operation);

  return TRUE;
}

static gboolean
grl_filesystem_source_notify_change_stop (GrlMediaSource *source,
                                          GError **error)
{
  GrlFilesystemSource *fs_source = GRL_FILESYSTEM_SOURCE (source);

  /* Check if notifying is being initialized */
  if (fs_source->priv->cancellable_monitors) {
    g_cancellable_cancel (fs_source->priv->cancellable_monitors);
    fs_source->priv->cancellable_monitors = NULL;
  } else {
    /* Cancel and remove all monitors */
    cancel_monitors (fs_source);
  }

  return TRUE;
}
