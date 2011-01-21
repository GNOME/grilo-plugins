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
  guint max_search_depth;
};

/* --- Data types --- */

typedef struct {
  GrlMediaSourceBrowseSpec *spec;
  GList *entries;
  GList *current;
  const gchar *path;
  guint remaining;
}  BrowseIdleData;

/* probably not thread-safe */
typedef struct {
  GrlMediaSource *source;
  GrlMediaSourceSearchSpec *ss;
  guint found_count;
  guint skipped;
  guint max_depth;
  GQueue *trees;
} SearchOperation;

typedef struct {
  SearchOperation *operation;
  guint depth;
  GFile *directory;
} SearchTree;

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
  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source),
                                       NULL);

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
  source_class->metadata = grl_filesystem_source_metadata;
  source_class->test_media_from_uri = grl_filesystem_test_media_from_uri;
  source_class->media_from_uri = grl_filesystem_get_media_from_uri;
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

/*** search stuff ***/

static void search_next_directory (SearchOperation *operation);

static SearchTree *
search_tree_new (SearchOperation *operation, guint depth, GFile *directory)
{
  SearchTree *tree = g_new (SearchTree, 1);
  tree->operation = operation;
  tree->depth = depth;
  tree->directory = g_object_ref (directory);

  return tree;
}

static void
search_tree_free (SearchTree *tree)
{
  g_object_unref (tree->directory);
  g_free (tree);
}

static SearchOperation *
search_operation_new (GrlMediaSource *source, GrlMediaSourceSearchSpec *ss, guint max_depth)
{
  SearchOperation *operation;

  operation = g_new (SearchOperation, 1);
  operation->source = source;
  operation->ss = ss;
  operation->found_count = 0;
  operation->skipped = 0;
  operation->max_depth = max_depth;
  operation->trees = g_queue_new ();

  return operation;
}

static void
search_operation_free (SearchOperation *operation)
{
  g_queue_foreach (operation->trees, (GFunc)search_tree_free, NULL);
  g_queue_free (operation->trees);
  g_free (operation);
}

/* return TRUE if more files need to be returned, FALSE if we sent the count */
static gboolean
compare_and_return_file (SearchOperation *operation,
                         GFileEnumerator *enumerator,
                         GFileInfo *file_info)
{
  gchar *needle, *haystack, *normalized_needle, *normalized_haystack;
  GrlMediaSourceSearchSpec *ss = operation->ss;

  GRL_DEBUG ("compare_and_return_file");

  if (ss == NULL)
    return FALSE;

  haystack = g_utf8_casefold (g_file_info_get_display_name (file_info), -1);
  normalized_haystack = g_utf8_normalize (haystack, -1, G_NORMALIZE_ALL);

  needle = g_utf8_casefold (ss->text, -1);
  normalized_needle = g_utf8_normalize (needle, -1, G_NORMALIZE_ALL);

  if (strstr (normalized_haystack, normalized_needle)) {
    GrlMedia *media = NULL;
    GFile *dir, *file;
    gchar *path;
    gint remaining = -1;

    dir = g_file_enumerator_get_container (enumerator);
    file = g_file_get_child (dir, g_file_info_get_name (file_info));
    path = g_file_get_path (file);

    /* FIXME: both file_is_valid_content() and create_content() are likely to block */
    if (file_is_valid_content (path, FALSE)) {
      if (operation->skipped < ss->skip)
        operation->skipped++;
      else
        media = create_content (NULL, path, ss->flags & GRL_RESOLVE_FAST_ONLY, FALSE);
    }

    g_free (path);
    g_object_unref (file);

    if (media) {
      operation->found_count++;
      if (operation->found_count == ss->count)
        remaining = 0;
      ss->callback (ss->source, ss->search_id, media, remaining, ss->user_data, NULL);
      if (!remaining)
        /* after a call to the callback with remaining=0, the core will free
         * the search spec */
        operation->ss = NULL;
    }
  }

  g_free (haystack);
  g_free (normalized_haystack);
  g_free (needle);
  g_free (normalized_needle);
  return operation->ss != NULL;
}

static void
got_file (GFileEnumerator *enumerator, GAsyncResult *res, SearchTree *tree)
{
  GList *files;
  GError *error = NULL;

  GRL_DEBUG ("got_file");

  files = g_file_enumerator_next_files_finish (enumerator, res, &error);
  if (error) {
    GRL_WARNING ("Got error while searching: %s", error->message);
    g_error_free (error);
    goto finished;
  }

  if (files) {
    GFileInfo *file_info;
    /* we assume there is only one GFileInfo in the list since that's what we ask
     * for when calling g_file_enumerator_next_files_async() */
    file_info = (GFileInfo *)files->data;
    g_list_free (files);
    switch (g_file_info_get_file_type (file_info)) {
    case G_FILE_TYPE_SYMBOLIC_LINK:
      /* we're too afraid of infinite recursion to touch this for now */
      break;
    case G_FILE_TYPE_DIRECTORY:
        {
          if (tree->depth < tree->operation->max_depth) {
            GFile *subdir;
            SearchTree *subtree;

            subdir = g_file_get_child (tree->directory,
                                       g_file_info_get_name (file_info));
            subtree = search_tree_new (tree->operation, tree->depth + 1, subdir);
            g_queue_push_tail (tree->operation->trees, subtree);
            g_object_unref (subdir);
          }
        }
      break;
    case G_FILE_TYPE_REGULAR:
      if (!compare_and_return_file (tree->operation, enumerator, file_info)){
        g_object_unref (file_info);
        goto finished;
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

  g_file_enumerator_next_files_async (enumerator, 1, G_PRIORITY_DEFAULT, NULL,
                                     (GAsyncReadyCallback)got_file, tree);

  return;

finished:
  /* we're done with this dir/enumerator, let's treat the next one */
  g_object_unref (enumerator);
  search_next_directory (tree->operation);
  search_tree_free (tree);
}

static void
got_children (GFile *directory, GAsyncResult *res, SearchTree *tree)
{
  GError *error = NULL;
  GFileEnumerator *enumerator;

  GRL_DEBUG ("got_children");

  enumerator = g_file_enumerate_children_finish (directory, res, &error);
  if (error) {
    GRL_WARNING ("Got error while searching: %s", error->message);
    g_error_free (error);
    g_object_unref (enumerator);
    /* we couldn't get the children of this directory, but we probably have
     * other directories to try */
    search_next_directory (tree->operation);
    search_tree_free (tree);
    return;
  }

  g_file_enumerator_next_files_async (enumerator, 1, G_PRIORITY_DEFAULT, NULL,
                                     (GAsyncReadyCallback)got_file, tree);
}

static void
search_next_directory (SearchOperation *operation)
{
  SearchTree *tree;
  GrlMediaSourceSearchSpec *ss = operation->ss;

  GRL_DEBUG ("search_next_directory");

  if (!ss) {  /* count reached, last callback call done */
    goto finished;
  }

  tree = g_queue_pop_head (operation->trees);
  if (!tree) { /* We've crawled everything, before reaching count */
    ss->callback (ss->source, ss->search_id, NULL, 0, ss->user_data, NULL);
    operation->ss = NULL;
    goto finished;
  }

  g_file_enumerate_children_async (tree->directory, G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                   G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                   G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                   G_PRIORITY_DEFAULT,
                                   NULL,
                                   (GAsyncReadyCallback)got_children,
                                   tree);

  return;

finished:
  search_operation_free (operation);
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
  SearchOperation *operation;
  GList *chosen_paths, *path;
  guint max_search_depth;
  GrlFilesystemSource *fs_source;

  GRL_DEBUG ("grl_filesystem_source_search");

  fs_source = GRL_FILESYSTEM_SOURCE (source);

  max_search_depth = fs_source->priv->max_search_depth;
  operation = search_operation_new (source, ss, max_search_depth);

  chosen_paths = fs_source->priv->chosen_paths;
  if (chosen_paths) {
    for (path = chosen_paths; path; path = g_list_next (path)) {
      GFile *directory = g_file_new_for_path (path->data);
      g_queue_push_tail (operation->trees,
                         search_tree_new (operation, 0, directory));
      g_object_unref (directory);
    }
  } else {
    const gchar *home;
    GFile *directory;

    home = g_getenv ("HOME");
    directory = g_file_new_for_path (home);
    g_queue_push_tail (operation->trees,
                       search_tree_new (operation, 0, directory));
    g_object_unref (directory);
  }

  search_next_directory (operation);
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
    mfus->callback (source, NULL, mfus->user_data, error);
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
    mfus->callback (source, NULL, mfus->user_data, new_error);
    g_clear_error (&new_error);
    goto beach;
  }

  /* FIXME: this is a blocking call, not sure we want that in here */
  /* Note: we assume create_content() never returns NULL, which seems to be true */
  media = create_content (NULL, path, mfus->flags & GRL_RESOLVE_FAST_ONLY,
                          FALSE);
  mfus->callback (source, media, mfus->user_data, NULL);

beach:
  g_free (path);
}

