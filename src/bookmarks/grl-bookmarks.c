/*
 * Copyright (C) 2010, 2011 Igalia S.L.
 * Copyright (C) 2014 Bastien Nocera <hadess@hadess.net>
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

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <grilo.h>
#include <string.h>
#include <stdlib.h>

#include "grl-bookmarks.h"
#include "bookmarks-resource.h"

#define GRL_ROOT_TITLE "Bookmarks"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT bookmarks_log_domain
GRL_LOG_DOMAIN_STATIC(bookmarks_log_domain);

/* --- Database --- */

#define GRL_SQL_DB "grl-bookmarks.db"

#define GRL_SQL_REMOVE_ORPHAN                        \
  "DELETE FROM bookmarks "                        \
  "WHERE id in ( "                                \
  "  SELECT DISTINCT id FROM bookmarks "        \
  "  WHERE parent NOT IN ( "                        \
  "    SELECT DISTINCT id FROM bookmarks) "        \
  "  and parent <> 0)"

/* --- Plugin information --- */

#define SOURCE_ID   "grl-bookmarks"
#define SOURCE_NAME _("Bookmarks")
#define SOURCE_DESC _("A source for organizing media bookmarks")

GrlKeyID GRL_BOOKMARKS_KEY_BOOKMARK_TIME = 0;

enum {
  BOOKMARK_TYPE_CATEGORY = 0,
  BOOKMARK_TYPE_STREAM,
};

struct _GrlBookmarksPrivate {
  GomAdapter *adapter;
  GomRepository *repository;
  gboolean notify_changes;
};

typedef struct {
  GrlSource *source;
  guint operation_id;
  const gchar *media_id;
  guint skip;
  guint count;
  GrlTypeFilter type_filter;
  GrlSourceResultCb callback;
  guint error_code;
  gpointer user_data;
} OperationSpec;

static GrlBookmarksSource *grl_bookmarks_source_new (void);

static void grl_bookmarks_source_finalize (GObject *plugin);

static const GList *grl_bookmarks_source_supported_keys (GrlSource *source);
static GrlCaps *grl_bookmarks_source_get_caps (GrlSource *source, GrlSupportedOps operation);
static GrlSupportedOps grl_bookmarks_source_supported_operations (GrlSource *source);

static void grl_bookmarks_source_search (GrlSource *source,
                                         GrlSourceSearchSpec *ss);
static void grl_bookmarks_source_query (GrlSource *source,
                                        GrlSourceQuerySpec *qs);
static void grl_bookmarks_source_browse (GrlSource *source,
                                         GrlSourceBrowseSpec *bs);
static void grl_bookmarks_source_resolve (GrlSource *source,
                                          GrlSourceResolveSpec *rs);
static void grl_bookmarks_source_store (GrlSource *source,
                                        GrlSourceStoreSpec *ss);
static void grl_bookmarks_source_remove (GrlSource *source,
                                         GrlSourceRemoveSpec *rs);

static gboolean grl_bookmarks_source_notify_change_start (GrlSource *source,
                                                          GError **error);

static gboolean grl_bookmarks_source_notify_change_stop (GrlSource *source,
                                                         GError **error);

 /* =================== Bookmarks Plugin  =============== */

 static gboolean
 grl_bookmarks_plugin_init (GrlRegistry *registry,
                            GrlPlugin *plugin,
                            GList *configs)
 {
   GRL_LOG_DOMAIN_INIT (bookmarks_log_domain, "bookmarks");

   GRL_DEBUG ("grl_bookmarks_plugin_init");

   /* Initialize i18n */
   bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
   bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

   GrlBookmarksSource *source = grl_bookmarks_source_new ();
   grl_registry_register_source (registry,
                                 plugin,
                                 GRL_SOURCE (source),
                                 NULL);
   return TRUE;
 }

static void
grl_bookmarks_plugin_register_keys (GrlRegistry *registry,
                                    GrlPlugin   *plugin)
{
  GParamSpec *spec;

  spec = g_param_spec_boxed ("bookmark-date",
                             "Bookmark date",
                             "When the media was bookmarked",
                             G_TYPE_DATE_TIME,
                             G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE),
  GRL_BOOKMARKS_KEY_BOOKMARK_TIME =
    grl_registry_register_metadata_key (registry, spec, GRL_METADATA_KEY_INVALID, NULL);
  /* If key was not registered, could be that it is already registered. If so,
     check if type is the expected one, and reuse it */
  if (GRL_BOOKMARKS_KEY_BOOKMARK_TIME == GRL_METADATA_KEY_INVALID) {
    g_param_spec_unref (spec);
    GRL_BOOKMARKS_KEY_BOOKMARK_TIME =
        grl_registry_lookup_metadata_key (registry, "bookmark-date");
    if (grl_metadata_key_get_type (GRL_BOOKMARKS_KEY_BOOKMARK_TIME)
        != G_TYPE_DATE_TIME) {
      GRL_BOOKMARKS_KEY_BOOKMARK_TIME = GRL_METADATA_KEY_INVALID;
    }
  }
}

GRL_PLUGIN_DEFINE (GRL_MAJOR,
                   GRL_MINOR,
                   BOOKMARKS_PLUGIN_ID,
                   "Bookmarks",
                   "A plugin for organizing media bookmarks",
                   "Igalia S.L.",
                   VERSION,
                   "LGPL-2.1-or-later",
                   "http://www.igalia.com",
                   grl_bookmarks_plugin_init,
                   NULL,
                   grl_bookmarks_plugin_register_keys);

 /* ================== Bookmarks GObject ================ */

 static GrlBookmarksSource *
 grl_bookmarks_source_new (void)
 {
   GRL_DEBUG ("grl_bookmarks_source_new");
   return g_object_new (GRL_BOOKMARKS_SOURCE_TYPE,
                        "source-id", SOURCE_ID,
                        "source-name", SOURCE_NAME,
                        "source-desc", SOURCE_DESC,
                        NULL);
 }

 static void
 grl_bookmarks_source_class_init (GrlBookmarksSourceClass * klass)
 {
   GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
   GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

   gobject_class->finalize = grl_bookmarks_source_finalize;

   source_class->supported_operations = grl_bookmarks_source_supported_operations;
   source_class->supported_keys = grl_bookmarks_source_supported_keys;
   source_class->get_caps = grl_bookmarks_source_get_caps;
   source_class->browse = grl_bookmarks_source_browse;
   source_class->search = grl_bookmarks_source_search;
   source_class->query = grl_bookmarks_source_query;
   source_class->store = grl_bookmarks_source_store;
   source_class->remove = grl_bookmarks_source_remove;
   source_class->resolve = grl_bookmarks_source_resolve;
   source_class->notify_change_start = grl_bookmarks_source_notify_change_start;
   source_class->notify_change_stop = grl_bookmarks_source_notify_change_stop;
}

static void
migrate_cb (GObject      *object,
            GAsyncResult *result,
            gpointer      user_data)
{
   gboolean ret;
   GError *error = NULL;

   ret = gom_repository_migrate_finish (GOM_REPOSITORY (object), result, &error);
   if (!ret) {
     GRL_WARNING ("Failed to migrate database: %s", error->message);
     g_error_free (error);
   }
}

G_DEFINE_TYPE_WITH_PRIVATE (GrlBookmarksSource, grl_bookmarks_source, GRL_TYPE_SOURCE)

static void
grl_bookmarks_source_init (GrlBookmarksSource *source)
{
  GError *error = NULL;
  gchar *path;
  gchar *db_path;
  GList *object_types;

  source->priv = grl_bookmarks_source_get_instance_private (source);

  path = g_build_filename (g_get_user_data_dir (), "grilo-plugins", NULL);

  if (!g_file_test (path, G_FILE_TEST_IS_DIR)) {
    g_mkdir_with_parents (path, 0775);
  }

  GRL_DEBUG ("Opening database connection...");
  db_path = g_build_filename (path, GRL_SQL_DB, NULL);
  g_free (path);

  source->priv->adapter = gom_adapter_new ();
  if (!gom_adapter_open_sync (source->priv->adapter, db_path, &error)) {
    GRL_WARNING ("Could not open database '%s': %s", db_path, error->message);
    g_error_free (error);
    g_free (db_path);
    return;
  }
  g_free (db_path);

  source->priv->repository = gom_repository_new (source->priv->adapter);
  object_types = g_list_prepend(NULL, GINT_TO_POINTER(BOOKMARKS_TYPE_RESOURCE));
  gom_repository_automatic_migrate_async (source->priv->repository, 2, object_types, migrate_cb, source);
}

static void
grl_bookmarks_source_finalize (GObject *object)
{
  GrlBookmarksSource *source;

  GRL_DEBUG ("grl_bookmarks_source_finalize");

  source = GRL_BOOKMARKS_SOURCE (object);

  g_clear_object (&source->priv->repository);

  if (source->priv->adapter) {
    gom_adapter_close_sync (source->priv->adapter, NULL);
    g_clear_object (&source->priv->adapter);
  }

  G_OBJECT_CLASS (grl_bookmarks_source_parent_class)->finalize (object);
}

/* ======================= Utilities ==================== */

static gboolean
mime_is_video (const gchar *mime)
{
  return mime && g_str_has_prefix (mime, "video/");
}

static gboolean
mime_is_audio (const gchar *mime)
{
  return mime && g_str_has_prefix (mime, "audio/");
}

static gboolean
mime_is_image (const gchar *mime)
{
  return mime && g_str_has_prefix (mime, "image/");
}

static GrlMedia *
build_media_from_resource (GrlMedia      *content,
                           GomResource   *resource,
                           GrlTypeFilter  type_filter)
{
  GrlMedia *media = NULL;
  gint64 id;
  gchar *str_id;
  gchar *title;
  gchar *url;
  gchar *desc;
  gchar *date;
  gchar *mime;
  gchar *thumb;
  guint type;

  if (content) {
    media = content;
  }

  g_object_get (resource,
                  "id", &id,
                  "title", &title,
                  "url", &url,
                  "desc", &desc,
                  "date", &date,
                  "mime", &mime,
                  "type", &type,
                  "thumbnail-url", &thumb,
                  NULL);

  if (!media) {
    if (type == BOOKMARK_TYPE_CATEGORY) {
      media = grl_media_container_new ();
    } else if (mime_is_audio (mime)) {
      if (type_filter & GRL_TYPE_FILTER_AUDIO)
        media = grl_media_new ();
    } else if (mime_is_video (mime)) {
      if (type_filter & GRL_TYPE_FILTER_VIDEO)
        media = grl_media_new ();
    } else if (mime_is_image (mime)) {
      if (type_filter & GRL_TYPE_FILTER_IMAGE)
        media = grl_media_image_new ();
    } else {
      if (type_filter != GRL_TYPE_FILTER_NONE)
        media = grl_media_new ();
    }
  }

  if (!media)
    return NULL;

  str_id = g_strdup_printf ("%" G_GINT64_FORMAT, id);
  grl_media_set_id (media, str_id);
  g_free (str_id);
  grl_media_set_title (media, title);
  if (url) {
    grl_media_set_url (media, url);
  }
  if (desc) {
    grl_media_set_description (media, desc);
  }

  if (date) {
    GDateTime *date_time = grl_date_time_from_iso8601 (date);
    if (date_time) {
      grl_data_set_boxed (GRL_DATA (media),
                          GRL_BOOKMARKS_KEY_BOOKMARK_TIME,
                          date_time);
      g_date_time_unref (date_time);
    }
  }

  if (thumb) {
    grl_media_set_thumbnail (media, thumb);
  }

  g_free (title);
  g_free (url);
  g_free (desc);
  g_free (date);
  g_free (mime);
  g_free (thumb);

  return media;
}

static void
bookmark_resolve (GrlSourceResolveSpec *rs)
{
  GomRepository *repository;
  GValue value = { 0, };
  GomFilter *filter;
  GomResource *resource;
  GError *error = NULL;
  gint64 id;
  GrlTypeFilter type_filter;

  GRL_DEBUG (__FUNCTION__);

  repository = GRL_BOOKMARKS_SOURCE (rs->source)->priv->repository;

  id = g_ascii_strtoll (grl_media_get_id (rs->media), NULL, 0);
  if (!id) {
    /* Root category: special case */
    grl_media_set_title (rs->media, GRL_ROOT_TITLE);
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
    return;
  }

  g_value_init (&value, G_TYPE_INT64);
  g_value_set_int64 (&value, id);
  filter = gom_filter_new_eq (BOOKMARKS_TYPE_RESOURCE, "id", &value);
  g_value_unset (&value);
  resource = gom_repository_find_one_sync (repository,
                                           BOOKMARKS_TYPE_RESOURCE,
                                           filter,
                                           &error);
  g_object_unref (filter);

  if (!resource) {
    GRL_WARNING ("Failed to get bookmark: %s", error->message);
    g_error_free (error);

    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_RESOLVE_FAILED,
                                 _("Failed to get bookmark metadata"));
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);
    g_error_free (error);
    return;
  }

  type_filter = grl_operation_options_get_type_filter (rs->options);
  build_media_from_resource (rs->media, resource, type_filter);
  g_object_unref (resource);
  rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
}

static void
find_cb (GObject      *object,
         GAsyncResult *res,
         gpointer      user_data)
{
  GomResourceGroup *group;
  OperationSpec *os = user_data;
  GError *local_error = NULL;
  GError *error = NULL;
  guint idx, count, num_left;

  group = gom_repository_find_finish (GOM_REPOSITORY (object),
                                      res,
                                      &local_error);
  if (!group) {
    GRL_WARNING ("Failed to find bookmarks: %s", local_error->message);
    error = g_error_new (GRL_CORE_ERROR,
                         os->error_code,
                         _("Failed to find bookmarks: %s"), local_error->message);
    g_error_free (local_error);
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
    goto out;
  }

  count = gom_resource_group_get_count (group);
  if (os->skip >= count) {
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, NULL);
    goto out;
  }

  if (!gom_resource_group_fetch_sync (group, os->skip, os->count, &local_error)) {
    GRL_WARNING ("Failed to find bookmarks: %s", local_error->message);
    error = g_error_new (GRL_CORE_ERROR,
                         os->error_code,
                         _("Failed to find bookmarks: %s"), local_error->message);
    g_error_free (local_error);
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
    goto out;
  }

  num_left = MIN (count - os->skip, os->count);
  for (idx = os->skip; num_left > 0 ; idx++) {
    GomResource *resource;
    GrlMedia *media;

    resource = gom_resource_group_get_index (group, idx);
    media = build_media_from_resource (NULL, resource, os->type_filter);
    if (media == NULL) {
      num_left--;
      if (num_left == 0)
        os->callback (os->source, os->operation_id, NULL, 0, os->user_data, NULL);
      continue;
    }
    os->callback (os->source,
                  os->operation_id,
                  media,
                  --num_left,
                  os->user_data,
                  NULL);
  }

  g_object_unref (group);

out:
  g_slice_free (OperationSpec, os);
}

static void
produce_bookmarks_from_filter (OperationSpec *os,
                               GomFilter     *filter)
{
  GomRepository *repository;

  GRL_DEBUG ("produce_bookmarks_from_filter");

  repository = GRL_BOOKMARKS_SOURCE (os->source)->priv->repository;
  gom_repository_find_async (repository,
                             BOOKMARKS_TYPE_RESOURCE,
                             filter,
                             find_cb,
                             os);
}

static void
produce_bookmarks_from_category (OperationSpec *os, const gchar *category_id)
{
  GomFilter *filter;
  GValue value = { 0, };
  int parent_id;

  GRL_DEBUG ("produce_bookmarks_from_category");
  parent_id = atoi (category_id);

  g_value_init (&value, G_TYPE_INT64);
  g_value_set_int64 (&value, parent_id);
  filter = gom_filter_new_eq (BOOKMARKS_TYPE_RESOURCE, "parent", &value);
  g_value_unset (&value);

  produce_bookmarks_from_filter (os, filter);
  g_object_unref (filter);
}

static void
produce_bookmarks_from_query (OperationSpec *os, const gchar *query)
{
  GomFilter *filter;
  GArray *array;

  GRL_DEBUG ("produce_bookmarks_from_query");

  array = g_array_new(FALSE, FALSE, sizeof(GValue));
  filter = gom_filter_new_sql (query, array);
  g_array_unref (array);
  produce_bookmarks_from_filter (os, filter);
  g_object_unref (filter);
}

static GomFilter *
substr_filter (const char *column,
               const char *text)
{
  GValue value = { 0, };
  GomFilter *filter;
  char *str;

  g_value_init (&value, G_TYPE_STRING);
  str = g_strdup_printf ("%%%s%%", text);
  g_value_set_string (&value, str);
  g_free (str);

  filter = gom_filter_new_like (BOOKMARKS_TYPE_RESOURCE, column, &value);
  g_value_unset (&value);

  return filter;
}

static void
produce_bookmarks_from_text (OperationSpec *os, const gchar *text)
{
  GomFilter *like1, *like2, *likes, *type1, *filter;
  GValue value = { 0, };

  GRL_DEBUG ("produce_bookmarks_from_text");

  /* WHERE (title LIKE '%text%' OR desc LIKE '%text%') AND type == BOOKMARKS_TYPE_STREAM */

  like1 = substr_filter ("title", text);
  like2 = substr_filter ("desc", text);
  likes = gom_filter_new_or (like1, like2);
  g_object_unref (like1);
  g_object_unref (like2);

  g_value_init (&value, G_TYPE_INT);
  g_value_set_int (&value, BOOKMARKS_TYPE_STREAM);
  type1 = gom_filter_new_eq (BOOKMARKS_TYPE_RESOURCE, "type", &value);
  g_value_unset (&value);

  filter = gom_filter_new_and (likes, type1);
  g_object_unref (likes);
  g_object_unref (type1);

  produce_bookmarks_from_filter (os, filter);
  g_object_unref (filter);
}

static void
remove_bookmark (GrlBookmarksSource *bookmarks_source,
                 const gchar *bookmark_id,
                 GrlMedia *media,
                 GError **error)
{
  GomResource *resource;
  gint64 id;
  GError *local_error = NULL;

  GRL_DEBUG ("remove_bookmark");

  id = g_ascii_strtoll (bookmark_id, NULL, 0);
  resource = g_object_new (BOOKMARKS_TYPE_RESOURCE, "id", id,
                           "repository", bookmarks_source->priv->repository,
                           NULL);
  if (!gom_resource_delete_sync (resource, &local_error)) {
    GRL_WARNING ("Failed to remove bookmark '%s': %s", bookmark_id, local_error->message);
    *error = g_error_new (GRL_CORE_ERROR,
                          GRL_CORE_ERROR_REMOVE_FAILED,
                          _("Failed to remove: %s"),
                          local_error->message);
    g_error_free (local_error);
  }

  g_object_unref (resource);

  if (*error == NULL && bookmarks_source->priv->notify_changes) {
    /* We can improve accuracy computing the parent container of removed
       element */
    grl_source_notify_change (GRL_SOURCE (bookmarks_source),
                              media,
                              GRL_CONTENT_REMOVED,
                              TRUE);
  }
}

static GomResource *
find_resource (const gchar   *id,
	       GomRepository *repository)
{
  GomResource *resource;
  GomFilter *filter;
  GValue value = { 0, };

  if (id == NULL)
    return NULL;

  g_value_init(&value, G_TYPE_INT64);
  g_value_set_int64 (&value, g_ascii_strtoll (id, NULL, 0));
  filter = gom_filter_new_eq (BOOKMARKS_TYPE_RESOURCE, "id", &value);
  g_value_unset(&value);

  resource = gom_repository_find_one_sync (repository,
                                           BOOKMARKS_TYPE_RESOURCE,
                                           filter,
                                           NULL);
  g_object_unref (filter);

  return resource;
}

static void
store_bookmark (GrlBookmarksSource *bookmarks_source,
                GList **keylist,
                GrlMedia *parent,
                GrlMedia *bookmark,
                GError **error)
{
  GomResource *resource;
  const gchar *title;
  const gchar *url;
  const gchar *desc;
  const gchar *thumb;
  GTimeVal now;
  gint64 parent_id;
  const gchar *mime;
  gchar *date;
  guint type;
  gint64 id;
  gchar *str_id;
  GError *local_error = NULL;
  gboolean ret;
  gboolean is_new_bookmark = FALSE;

  GRL_DEBUG ("store_bookmark");

  str_id = (gchar *) grl_media_get_id (bookmark);
  title = grl_media_get_title (bookmark);
  url = grl_media_get_url (bookmark);
  thumb = grl_media_get_thumbnail (bookmark);
  desc = grl_media_get_description (bookmark);
  mime = grl_media_get_mime (bookmark);
  g_get_current_time (&now);
  date = g_time_val_to_iso8601 (&now);

  if (!parent) {
    parent_id = 0;
  } else {
    parent_id = g_ascii_strtoll (grl_media_get_id (GRL_MEDIA (parent)), NULL, 0);
  }
  if (parent_id < 0) {
    parent_id = 0;
  }

  GRL_DEBUG ("URL: '%s'", url);

  if (grl_media_is_container (bookmark)) {
    type = BOOKMARK_TYPE_CATEGORY;
  } else {
    type = BOOKMARK_TYPE_STREAM;
  }

  resource = find_resource (str_id, bookmarks_source->priv->repository);
  if (!resource) {
    resource = g_object_new (BOOKMARKS_TYPE_RESOURCE,
                             "repository", bookmarks_source->priv->repository,
                             "parent", parent_id,
                             "type", type,
                             NULL);
    is_new_bookmark = TRUE;
  }

  if (type == BOOKMARK_TYPE_STREAM) {
    g_object_set (G_OBJECT (resource), "url", url, NULL);
    *keylist = g_list_remove (*keylist,
                              GRLKEYID_TO_POINTER (GRL_METADATA_KEY_URL));
  }
  if (title) {
    g_object_set (G_OBJECT (resource), "title", title, NULL);
    *keylist = g_list_remove (*keylist,
                              GRLKEYID_TO_POINTER (GRL_METADATA_KEY_TITLE));
  } else if (url) {
    g_object_set (G_OBJECT (resource), "title", url, NULL);
  } else {
    g_object_set (G_OBJECT (resource), "title", "(unknown)", NULL);
  }
  if (date) {
    g_object_set (G_OBJECT (resource), "date", date, NULL);
  }
  if (mime) {
    g_object_set (G_OBJECT (resource), "mime", mime, NULL);
    *keylist = g_list_remove (*keylist,
                              GRLKEYID_TO_POINTER (GRL_METADATA_KEY_MIME));
  }
  if (desc) {
    g_object_set (G_OBJECT (resource), "desc", desc, NULL);
    *keylist = g_list_remove (*keylist,
                              GRLKEYID_TO_POINTER (GRL_METADATA_KEY_DESCRIPTION));
  }
  if (thumb) {
    g_object_set (G_OBJECT (resource), "thumbnail-url", thumb, NULL);
    *keylist = g_list_remove (*keylist,
                              GRLKEYID_TO_POINTER (GRL_METADATA_KEY_THUMBNAIL));
  }

  ret = gom_resource_save_sync (resource, &local_error);
  if (!ret) {
    GRL_WARNING ("Failed to store bookmark '%s': %s", title,
                 local_error->message);
    *error = g_error_new (GRL_CORE_ERROR,
                          GRL_CORE_ERROR_STORE_FAILED,
                          _("Failed to store: %s"),
                          local_error->message);
    g_error_free (local_error);
    g_object_unref (resource);
    return;
  }

  g_object_get (resource, "id", &id, NULL);
  str_id = g_strdup_printf ("%" G_GINT64_FORMAT, id);
  grl_media_set_id (bookmark, str_id);
  g_free (str_id);

  g_object_unref (resource);

  if (bookmarks_source->priv->notify_changes) {
    grl_source_notify_change (GRL_SOURCE (bookmarks_source),
                              bookmark,
                              is_new_bookmark ? GRL_CONTENT_ADDED : GRL_CONTENT_CHANGED,
                              FALSE);
  }
}

/* ================== API Implementation ================ */

static const GList *
grl_bookmarks_source_supported_keys (GrlSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_URL,
                                      GRL_METADATA_KEY_CHILDCOUNT,
                                      GRL_METADATA_KEY_DESCRIPTION,
                                      GRL_METADATA_KEY_THUMBNAIL,
                                      GRL_BOOKMARKS_KEY_BOOKMARK_TIME,
                                      NULL);
  }
  return keys;
}

static GrlCaps *
grl_bookmarks_source_get_caps (GrlSource       *source,
                               GrlSupportedOps  operation)
{
  GList *keys;
  static GrlCaps *caps = NULL;

  if (caps == NULL) {
    caps = grl_caps_new ();
    grl_caps_set_type_filter (caps, GRL_TYPE_FILTER_ALL);
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_MIME, NULL);
    grl_caps_set_key_filter (caps, keys);
    g_list_free (keys);
  }

  return caps;
}

static void
grl_bookmarks_source_browse (GrlSource *source,
                             GrlSourceBrowseSpec *bs)
{
  GRL_DEBUG ("grl_bookmarks_source_browse");

  OperationSpec *os;
  GrlBookmarksSource *bookmarks_source;
  GError *error = NULL;

  bookmarks_source = GRL_BOOKMARKS_SOURCE (source);
  if (!bookmarks_source->priv->adapter) {
    GRL_WARNING ("Can't execute operation: no database connection.");
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_BROWSE_FAILED,
                                 _("No database connection"));
    bs->callback (bs->source, bs->operation_id, NULL, 0, bs->user_data, error);
    g_error_free (error);
  }

  /* Configure browse operation */
  os = g_slice_new0 (OperationSpec);
  os->source = bs->source;
  os->operation_id = bs->operation_id;
  os->media_id = grl_media_get_id (bs->container);
  os->count = grl_operation_options_get_count (bs->options);
  os->skip = grl_operation_options_get_skip (bs->options);
  os->type_filter = grl_operation_options_get_type_filter (bs->options);
  os->callback = bs->callback;
  os->user_data = bs->user_data;
  os->error_code = GRL_CORE_ERROR_BROWSE_FAILED;

  produce_bookmarks_from_category (os, os->media_id ? os->media_id : "0");
}

static void
grl_bookmarks_source_search (GrlSource *source,
                             GrlSourceSearchSpec *ss)
{
  GRL_DEBUG ("grl_bookmarks_source_search");

  GrlBookmarksSource *bookmarks_source;
  OperationSpec *os;
  GError *error = NULL;

  bookmarks_source = GRL_BOOKMARKS_SOURCE (source);
  if (!bookmarks_source->priv->adapter) {
    GRL_WARNING ("Can't execute operation: no database connection.");
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_QUERY_FAILED,
                                 _("No database connection"));
    ss->callback (ss->source, ss->operation_id, NULL, 0, ss->user_data, error);
    g_error_free (error);
  }

  os = g_slice_new0 (OperationSpec);
  os->source = ss->source;
  os->operation_id = ss->operation_id;
  os->count = grl_operation_options_get_count (ss->options);
  os->skip = grl_operation_options_get_skip (ss->options);
  os->callback = ss->callback;
  os->user_data = ss->user_data;
  os->error_code = GRL_CORE_ERROR_SEARCH_FAILED;
  produce_bookmarks_from_text (os, ss->text);
}

static void
grl_bookmarks_source_query (GrlSource *source,
                            GrlSourceQuerySpec *qs)
{
  GRL_DEBUG ("grl_bookmarks_source_query");

  GrlBookmarksSource *bookmarks_source;
  OperationSpec *os;
  GError *error = NULL;

  bookmarks_source = GRL_BOOKMARKS_SOURCE (source);
  if (!bookmarks_source->priv->adapter) {
    GRL_WARNING ("Can't execute operation: no database connection.");
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_QUERY_FAILED,
                                 _("No database connection"));
    qs->callback (qs->source, qs->operation_id, NULL, 0, qs->user_data, error);
    g_error_free (error);
  }

  os = g_slice_new0 (OperationSpec);
  os->source = qs->source;
  os->operation_id = qs->operation_id;
  os->count = grl_operation_options_get_count (qs->options);
  os->skip = grl_operation_options_get_skip (qs->options);
  os->type_filter = grl_operation_options_get_type_filter (qs->options);
  os->callback = qs->callback;
  os->user_data = qs->user_data;
  os->error_code = GRL_CORE_ERROR_SEARCH_FAILED;
  produce_bookmarks_from_query (os, qs->query);
}

static void
grl_bookmarks_source_store (GrlSource *source, GrlSourceStoreSpec *ss)
{
  GError *error = NULL;
  GList *keylist;

  GRL_DEBUG ("grl_bookmarks_source_store");

  /* FIXME: Try to guess bookmark mime somehow */
  keylist = grl_data_get_keys (GRL_DATA (ss->media));
  store_bookmark (GRL_BOOKMARKS_SOURCE (ss->source),
                  &keylist, ss->parent, ss->media, &error);
  ss->callback (ss->source, ss->media, keylist, ss->user_data, error);
  g_clear_error (&error);
}

static void grl_bookmarks_source_remove (GrlSource *source,
                                         GrlSourceRemoveSpec *rs)
{
  GRL_DEBUG (__FUNCTION__);
  GError *error = NULL;
  remove_bookmark (GRL_BOOKMARKS_SOURCE (rs->source),
                   rs->media_id, rs->media, &error);
  rs->callback (rs->source, rs->media, rs->user_data, error);
  g_clear_error (&error);
}

static void
grl_bookmarks_source_resolve (GrlSource *source,
                              GrlSourceResolveSpec *rs)
{
  GRL_DEBUG (__FUNCTION__);

  GrlBookmarksSource *bookmarks_source;
  GError *error = NULL;

  bookmarks_source = GRL_BOOKMARKS_SOURCE (source);
  if (!bookmarks_source->priv->repository) {
    GRL_WARNING ("Can't execute operation: no database connection.");
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_RESOLVE_FAILED,
                                 _("No database connection"));
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);
    g_error_free (error);
  }

  bookmark_resolve (rs);
}

static GrlSupportedOps
grl_bookmarks_source_supported_operations (GrlSource *source)
{
  GrlSupportedOps caps;

  caps = GRL_OP_BROWSE | GRL_OP_RESOLVE | GRL_OP_SEARCH | GRL_OP_QUERY |
    GRL_OP_STORE | GRL_OP_STORE_PARENT | GRL_OP_REMOVE | GRL_OP_NOTIFY_CHANGE;

  return caps;
}

static gboolean
grl_bookmarks_source_notify_change_start (GrlSource *source,
                                          GError **error)
{
  GrlBookmarksSource *bookmarks_source = GRL_BOOKMARKS_SOURCE (source);

  bookmarks_source->priv->notify_changes = TRUE;

  return TRUE;
}

static gboolean
grl_bookmarks_source_notify_change_stop (GrlSource *source,
                                         GError **error)
{
  GrlBookmarksSource *bookmarks_source = GRL_BOOKMARKS_SOURCE (source);

  bookmarks_source->priv->notify_changes = FALSE;

  return TRUE;
}
