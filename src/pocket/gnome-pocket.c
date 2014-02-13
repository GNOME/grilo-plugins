/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 *  Copyright © 2013 Bastien Nocera <hadess@hadess.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any pocket version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Implementation of:
 * http://getpocket.com/developer/docs/overview
 */

//#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>
#define GOA_API_IS_SUBJECT_TO_CHANGE 1
#include <goa/goa.h>
#include <rest/rest-proxy.h>
#include <rest/rest-proxy-call.h>
#include <json-glib/json-glib.h>

#include "gnome-pocket.h"

static gpointer
gnome_pocket_item_copy (gpointer boxed)
{
  GnomePocketItem *src = boxed;

  GnomePocketItem *dest = g_new (GnomePocketItem, 1);
  dest->id = g_strdup (src->id);
  dest->url = g_strdup (src->url);
  dest->title = g_strdup (src->title);
  dest->favorite = src->favorite;
  dest->status = src->status;
  dest->is_article = src->is_article;
  dest->has_image = src->has_image;
  dest->has_video = src->has_video;
  dest->time_added = src->time_added;
  dest->tags = g_strdupv (src->tags);

  return dest;
}

static void
gnome_pocket_item_free (gpointer boxed)
{
  GnomePocketItem *item = boxed;

  g_free (item->id);
  g_free (item->url);
  g_free (item->title);
  g_strfreev (item->tags);
  g_free (item);
}

static char *
get_string_for_element (JsonReader *reader,
                        const char *element)
{
  char *ret;

  if (!json_reader_read_member (reader, element)) {
    json_reader_end_member (reader);
    return NULL;
  }
  ret = g_strdup (json_reader_get_string_value (reader));
  if (ret && *ret == '\0')
    g_clear_pointer (&ret, g_free);
  json_reader_end_member (reader);

  return ret;
}

static int
get_int_for_element (JsonReader *reader,
                     const char *element)
{
  int ret;

  if (!json_reader_read_member (reader, element)) {
    json_reader_end_member (reader);
    return -1;
  }
  ret = atoi (json_reader_get_string_value (reader));
  json_reader_end_member (reader);

  return ret;
}

static gint64
get_time_added (JsonReader *reader)
{
  gint64 ret;

  if (!json_reader_read_member (reader, "time_added")) {
    json_reader_end_member (reader);
    return -1;
  }
  ret = g_ascii_strtoll (json_reader_get_string_value (reader), NULL, 0);
  json_reader_end_member (reader);

  return ret;
}

static GnomePocketItem *
parse_item (JsonReader *reader)
{
  GnomePocketItem *item;

  item = g_new0 (GnomePocketItem, 1);
  item->id = g_strdup (json_reader_get_member_name (reader));
  if (!item->id)
    goto bail;

  /* If the item is archived or deleted, we don't need
   * anything more here */
  item->status = get_int_for_element (reader, "status");
  if (item->status != POCKET_STATUS_NORMAL)
    goto end;

  item->url = get_string_for_element (reader, "resolved_url");
  if (!item->url)
    item->url = get_string_for_element (reader, "given_url");

  item->title = get_string_for_element (reader, "resolved_title");
  if (!item->title)
    item->title = get_string_for_element (reader, "given_title");
  if (!item->title)
    item->title = g_strdup ("PLACEHOLDER"); /* FIXME generate from URL */

  item->favorite = get_int_for_element (reader, "favorite");
  item->is_article = get_int_for_element (reader, "is_article");
  if (item->is_article == -1)
    item->is_article = FALSE;
  item->has_image = get_int_for_element (reader, "has_image");
  if (item->has_image == -1)
    item->has_image = POCKET_HAS_MEDIA_FALSE;
  item->has_video = get_int_for_element (reader, "has_video");
  if (item->has_video == -1)
    item->has_video = POCKET_HAS_MEDIA_FALSE;

  item->time_added = get_time_added (reader);

  if (json_reader_read_member (reader, "tags"))
    item->tags = json_reader_list_members (reader);
  json_reader_end_member (reader);

  if (json_reader_read_member (reader, "image"))
    item->thumbnail_url = get_string_for_element (reader, "src");
  json_reader_end_member (reader);

  goto end;

bail:
  g_clear_pointer (&item, gnome_pocket_item_free);

end:
  return item;
}

GnomePocketItem *
gnome_pocket_item_from_string (const char *str)
{
  JsonParser *parser;
  JsonReader *reader;
  GnomePocketItem *item = NULL;
  char **members = NULL;

  parser = json_parser_new ();
  if (!json_parser_load_from_data (parser, str, -1, NULL))
    return NULL;

  reader = json_reader_new (json_parser_get_root (parser));
  members = json_reader_list_members (reader);
  if (!members)
    goto bail;

  if (!json_reader_read_member (reader, members[0]))
    goto bail;

  item = parse_item (reader);

bail:
  g_clear_pointer (&members, g_strfreev);
  g_clear_object (&reader);
  g_clear_object (&parser);

  return item;
}

static void
builder_add_int_as_str (JsonBuilder *builder,
                        const char  *name,
                        int          value)
{
  char *tmp;

  json_builder_set_member_name (builder, name);
  tmp = g_strdup_printf ("%d", value);
  json_builder_add_string_value (builder, tmp);
  g_free (tmp);
}

static void
builder_add_int64_as_str (JsonBuilder *builder,
                          const char  *name,
                          gint64       value)
{
  char *tmp;

  json_builder_set_member_name (builder, name);
  tmp = g_strdup_printf ("%" G_GINT64_FORMAT, value);
  json_builder_add_string_value (builder, tmp);
  g_free (tmp);
}

char *
gnome_pocket_item_to_string (GnomePocketItem *item)
{
  char *ret = NULL;
  JsonBuilder *builder;
  JsonGenerator *gen;
  JsonNode *root;

  builder = json_builder_new ();

  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, item->id);

  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "item_id");
  json_builder_add_string_value (builder, item->id);

  json_builder_set_member_name (builder, "resolved_url");
  json_builder_add_string_value (builder, item->url);

  json_builder_set_member_name (builder, "resolved_title");
  json_builder_add_string_value (builder, item->title);

  builder_add_int_as_str (builder, "favorite", item->favorite);
  builder_add_int_as_str (builder, "status", item->status);
  builder_add_int_as_str (builder, "is_article", item->is_article);
  builder_add_int_as_str (builder, "has_image", item->has_image);
  builder_add_int_as_str (builder, "has_video", item->has_video);
  builder_add_int64_as_str (builder, "time_added", item->time_added);

  if (item->thumbnail_url) {
    json_builder_set_member_name (builder, "image");
    json_builder_begin_object (builder);

    json_builder_set_member_name (builder, "item_id");
    json_builder_add_string_value (builder, item->id);
    json_builder_set_member_name (builder, "src");
    json_builder_add_string_value (builder, item->thumbnail_url);

    json_builder_end_object (builder);
  }

  json_builder_end_object (builder);
  json_builder_end_object (builder);

  gen = json_generator_new ();
  root = json_builder_get_root (builder);
  json_generator_set_root (gen, root);
  ret = json_generator_to_data (gen, NULL);

  json_node_free (root);
  g_object_unref (gen);
  g_object_unref (builder);

  return ret;
}

G_DEFINE_BOXED_TYPE(GnomePocketItem, gnome_pocket_item, gnome_pocket_item_copy, gnome_pocket_item_free)

enum {
  PROP_0,
  PROP_AVAILABLE
};

G_DEFINE_TYPE (GnomePocket, gnome_pocket, G_TYPE_OBJECT);

#define GNOME_POCKET_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), GNOME_TYPE_POCKET, GnomePocketPrivate))

struct _GnomePocketPrivate {
  GCancellable   *cancellable;
  GoaClient      *client;
  GoaOAuth2Based *oauth2;
  char           *access_token;
  char           *consumer_key;
  RestProxy      *proxy;

  /* List data */
  gboolean       cache_loaded;
  gint64         since;
  GList         *items; /* GnomePocketItem */
};

gboolean
gnome_pocket_refresh_finish (GnomePocket       *self,
                             GAsyncResult        *res,
                             GError             **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  gboolean ret = FALSE;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == gnome_pocket_refresh);

  if (!g_simple_async_result_propagate_error (simple, error))
    ret = g_simple_async_result_get_op_res_gboolean (simple);

  return ret;
}

/* FIXME */
static char *cache_path = NULL;

static char *
gnome_pocket_item_get_path (GnomePocketItem *item)
{
  g_return_val_if_fail (item != NULL, NULL);
  return g_build_filename (cache_path, item->id, NULL);
}

static void
gnome_pocket_item_save (GnomePocketItem *item)
{
  char *path;
  char *str;

  g_return_if_fail (item != NULL);

  path = gnome_pocket_item_get_path (item);
  str = gnome_pocket_item_to_string (item);
  g_file_set_contents (path, str, -1, NULL);
  g_free (str);
  g_free (path);
}

static void
gnome_pocket_item_remove (GnomePocketItem *item)
{
  char *path;

  g_return_if_fail (item != NULL);

  path = gnome_pocket_item_get_path (item);
  g_unlink (path);
  g_free (path);
}

static void
update_list (GnomePocket *self,
             GList       *updated_items)
{
  GHashTable *removed; /* key=id, value=gboolean */
  GList *added;
  GList *l;

  if (updated_items == NULL)
    return;

  removed = g_hash_table_new_full (g_str_hash, g_str_equal,
                                   g_free, NULL);

  added = NULL;
  for (l = updated_items; l != NULL; l = l->next) {
    GnomePocketItem *item = l->data;

    if (item->status != POCKET_STATUS_NORMAL) {
      g_hash_table_insert (removed,
                           g_strdup (item->id),
                           GINT_TO_POINTER (1));
      gnome_pocket_item_free (item);
    } else {
      added = g_list_prepend (added, item);
      gnome_pocket_item_save (item);
    }
  }

  added = g_list_reverse (added);
  self->priv->items = g_list_concat (added, self->priv->items);

  /* And remove the old items */
  for (l = self->priv->items; l != NULL; l = l->next) {
    GnomePocketItem *item = l->data;

    if (g_hash_table_lookup (removed, item->id)) {
      /* Item got removed */
      self->priv->items = g_list_delete_link (self->priv->items, l);

      gnome_pocket_item_remove (item);
      gnome_pocket_item_free (item);
    }
  }

  g_hash_table_destroy (removed);
}

static gint64
load_since (GnomePocket *self)
{
  char *path;
  char *contents = NULL;
  gint64 since = 0;

  path = g_build_filename (cache_path, "since", NULL);
  g_file_get_contents (path, &contents, NULL, NULL);
  g_free (path);

  if (contents != NULL) {
    since = g_ascii_strtoll (contents, NULL, 0);
    g_free (contents);
  }

  return since;
}

static void
save_since (GnomePocket *self)
{
  char *str;
  char *path;

  if (self->priv->since == 0)
    return;

  str = g_strdup_printf ("%" G_GINT64_FORMAT, self->priv->since);
  path = g_build_filename (cache_path, "since", NULL);
  g_file_set_contents (path, str, -1, NULL);
  g_free (path);
  g_free (str);
}

static int
sort_items (gconstpointer a,
	    gconstpointer b)
{
  GnomePocketItem *item_a = (gpointer) a;
  GnomePocketItem *item_b = (gpointer) b;

  /* We sort newest first */
  if (item_a->time_added < item_b->time_added)
    return 1;
  if (item_b->time_added < item_a->time_added)
    return -1;
  return 0;
}

static GList *
parse_json (JsonParser *parser,
            gint64     *since)
{
  JsonReader *reader;
  GList *ret;
  int num;

  reader = json_reader_new (json_parser_get_root (parser));
  *since = 0;
  ret = NULL;

  num = json_reader_count_members (reader);
  if (num < 0)
    goto bail;

  /* Grab the since */
  if (json_reader_read_member (reader, "since"))
    *since = json_reader_get_int_value (reader);
  json_reader_end_member (reader);

  /* Grab the list */
  if (json_reader_read_member (reader, "list")) {
    char **members;
    guint i;

    members = json_reader_list_members (reader);
    if (members != NULL) {
      for (i = 0; members[i] != NULL; i++) {
        GnomePocketItem *item;

        if (!json_reader_read_member (reader, members[i])) {
          json_reader_end_member (reader);
          continue;
        }
        item = parse_item (reader);
        if (item)
          ret = g_list_prepend (ret, item);
        json_reader_end_member (reader);
      }
    }
    g_strfreev (members);
  }
  json_reader_end_member (reader);

  ret = g_list_sort (ret, sort_items);

bail:
  g_clear_object (&reader);
  return ret;
}

static void
refresh_cb (GObject      *object,
            GAsyncResult *res,
            gpointer      user_data)
{
  GError *error = NULL;
  GSimpleAsyncResult *simple = user_data;
  gboolean ret;

  ret = rest_proxy_call_invoke_finish (REST_PROXY_CALL (object), res, &error);
  if (!ret) {
    g_simple_async_result_set_from_error (simple, error);
  } else {
    JsonParser *parser;

    parser = json_parser_new ();
    if (json_parser_load_from_data (parser,
                                    rest_proxy_call_get_payload (REST_PROXY_CALL (object)),
                                    rest_proxy_call_get_payload_length (REST_PROXY_CALL (object)),
                                    NULL)) {
      GList *updated_items;
      GnomePocket *self;

      self = GNOME_POCKET (g_async_result_get_source_object (G_ASYNC_RESULT (simple)));
      updated_items = parse_json (parser, &self->priv->since);
      if (self->priv->since != 0)
        save_since (self);
      update_list (self, updated_items);
    }
    g_object_unref (parser);
  }
  g_simple_async_result_set_op_res_gboolean (simple, ret);

  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
  g_clear_error (&error);
}

void
gnome_pocket_refresh (GnomePocket         *self,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  RestProxyCall *call;
  GSimpleAsyncResult *simple;

  g_return_if_fail (GNOME_IS_POCKET (self));
  g_return_if_fail (self->priv->consumer_key && self->priv->access_token);

  simple = g_simple_async_result_new (G_OBJECT (self),
                                      callback,
                                      user_data,
                                      gnome_pocket_refresh);

  g_simple_async_result_set_check_cancellable (simple, cancellable);

  call = rest_proxy_new_call (self->priv->proxy);
  rest_proxy_call_set_method (call, "POST");
  rest_proxy_call_set_function (call, "v3/get");
  rest_proxy_call_add_param (call, "consumer_key", self->priv->consumer_key);
  rest_proxy_call_add_param (call, "access_token", self->priv->access_token);

  if (self->priv->since > 0) {
    char *since;
    since = g_strdup_printf ("%" G_GINT64_FORMAT, self->priv->since);
    rest_proxy_call_add_param (call, "since", since);
    g_free (since);
  }

  /* To get the image/images/authors/videos item details */
  rest_proxy_call_add_param (call, "detailType", "complete");
  rest_proxy_call_add_param (call, "tags", "1");

  rest_proxy_call_invoke_async (call, cancellable, refresh_cb, simple);
}

gboolean
gnome_pocket_add_url_finish (GnomePocket   *self,
                             GAsyncResult  *res,
                             GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  gboolean ret = FALSE;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == gnome_pocket_add_url);

  if (!g_simple_async_result_propagate_error (simple, error))
    ret = g_simple_async_result_get_op_res_gboolean (simple);

  return ret;
}

static void
add_url_cb (GObject      *object,
            GAsyncResult *res,
            gpointer      user_data)
{
  GError *error = NULL;
  GSimpleAsyncResult *simple = user_data;
  gboolean ret;

  ret = rest_proxy_call_invoke_finish (REST_PROXY_CALL (object), res, &error);
  if (!ret)
    g_simple_async_result_set_from_error (simple, error);
  g_simple_async_result_set_op_res_gboolean (simple, ret);

  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
  g_clear_error (&error);
}

void
gnome_pocket_add_url (GnomePocket         *self,
                      const char          *url,
                      const char          *tweet_id,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  RestProxyCall *call;
  GSimpleAsyncResult *simple;

  g_return_if_fail (GNOME_IS_POCKET (self));
  g_return_if_fail (url);
  g_return_if_fail (self->priv->consumer_key && self->priv->access_token);

  simple = g_simple_async_result_new (G_OBJECT (self),
                                      callback,
                                      user_data,
                                      gnome_pocket_add_url);

  g_simple_async_result_set_check_cancellable (simple, cancellable);

  call = rest_proxy_new_call (self->priv->proxy);
  rest_proxy_call_set_method (call, "POST");
  rest_proxy_call_set_function (call, "v3/add");
  rest_proxy_call_add_param (call, "consumer_key", self->priv->consumer_key);
  rest_proxy_call_add_param (call, "access_token", self->priv->access_token);
  rest_proxy_call_add_param (call, "url", url);
  if (tweet_id)
    rest_proxy_call_add_param (call, "tweet_id", tweet_id);

  rest_proxy_call_invoke_async (call, cancellable, add_url_cb, simple);
}

static void
parse_cached_data (GnomePocket *self)
{
  GDir *dir;
  const char *name;

  dir = g_dir_open (cache_path, 0, NULL);
  if (!dir)
    return;

  self->priv->since = load_since (self);

  name = g_dir_read_name (dir);
  while (name) {
    JsonParser *parser;
    JsonReader *reader;
    char *full_path = NULL;
    char **members;
    GnomePocketItem *item;

    if (g_strcmp0 (name, "since") == 0)
      goto next;

    full_path = g_build_filename (cache_path, name, NULL);
    parser = json_parser_new ();
    if (!json_parser_load_from_file (parser, full_path, NULL))
      goto next;

    reader = json_reader_new (json_parser_get_root (parser));
    members = json_reader_list_members (reader);
    if (!members)
      goto next;

    if (!json_reader_read_member (reader, members[0]))
      goto next;

    item = parse_item (reader);
    if (!item)
      g_warning ("Could not parse cached file '%s'", full_path);
    else
      self->priv->items = g_list_prepend (self->priv->items, item);

next:
    g_clear_object (&reader);
    g_clear_object (&parser);
    g_free (full_path);

    name = g_dir_read_name (dir);
  }
  g_dir_close (dir);

  self->priv->items = g_list_sort (self->priv->items, sort_items);
}

static void
load_cached_thread (GTask           *task,
                    gpointer         source_object,
                    gpointer         task_data,
                    GCancellable    *cancellable)
{
  GnomePocket *self = GNOME_POCKET (source_object);

  parse_cached_data (self);
  self->priv->cache_loaded = TRUE;
  g_task_return_boolean (task, TRUE);
}

void
gnome_pocket_load_cached (GnomePocket         *self,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  GTask *task;

  g_return_if_fail (GNOME_IS_POCKET (self));
  g_return_if_fail (!self->priv->cache_loaded);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_run_in_thread (task, load_cached_thread);
  g_object_unref (task);
}

gboolean
gnome_pocket_load_cached_finish (GnomePocket         *self,
                                 GAsyncResult        *res,
                                 GError             **error)
{
  GTask *task = G_TASK (res);

  g_return_val_if_fail (g_task_is_valid (res, self), NULL);

  return g_task_propagate_boolean (task, error);
}

GList *
gnome_pocket_load_from_file (GnomePocket   *self,
                             const char    *filename,
                             GError       **error)
{
  GList *ret;
  gint64 since;
  JsonParser *parser;

  parser = json_parser_new ();
  if (!json_parser_load_from_file (parser, filename, error)) {
    g_object_unref (parser);
    return NULL;
  }

  ret = parse_json (parser, &since);
  g_object_unref (parser);

  return ret;
}

static const char *
bool_to_str (gboolean b)
{
  return b ? "True" : "False";
}

static const char *
inclusion_to_str (PocketMediaInclusion inc)
{
  switch (inc) {
  case POCKET_HAS_MEDIA_FALSE:
    return "False";
  case POCKET_HAS_MEDIA_INCLUDED:
    return "Included";
  case POCKET_IS_MEDIA:
    return "Is media";
  default:
    g_assert_not_reached ();
  }
}

void
gnome_pocket_print_item (GnomePocketItem *item)
{
  GDateTime *date;
  char *date_str;

  g_return_if_fail (item != NULL);

  date = g_date_time_new_from_unix_utc (item->time_added);
  date_str = g_date_time_format (date, "%F %R");
  g_date_time_unref (date);

  g_print ("Item: %s\n", item->id);
  g_print ("\tTime added: %s\n", date_str);
  g_print ("\tURL: %s\n", item->url);
  if (item->thumbnail_url)
    g_print ("\tThumbnail URL: %s\n", item->thumbnail_url);
  g_print ("\tTitle: %s\n", item->title);
  g_print ("\tFavorite: %s\n", bool_to_str (item->favorite));
  g_print ("\tIs article: %s\n", bool_to_str (item->is_article));
  g_print ("\tHas Image: %s\n", inclusion_to_str (item->has_image));
  g_print ("\tHas Video: %s\n", inclusion_to_str (item->has_video));
  if (item->tags != NULL) {
    guint i;
    g_print ("\tTags: ");
    for (i = 0; item->tags[i] != NULL; i++)
      g_print ("%s, ", item->tags[i]);
    g_print ("\n");
  }

  g_free (date_str);
}

GList *
gnome_pocket_get_items (GnomePocket *self)
{
  g_return_val_if_fail (self->priv->cache_loaded, NULL);
  return self->priv->items;
}

static void
gnome_pocket_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
  GnomePocket *self = GNOME_POCKET (object);

  switch (property_id) {
  case PROP_AVAILABLE:
    g_value_set_boolean (value, self->priv->access_token && self->priv->consumer_key);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (self, property_id, pspec);
    break;
  }
}

static void
gnome_pocket_finalize (GObject *object)
{
  GnomePocketPrivate *priv = GNOME_POCKET (object)->priv;

  g_cancellable_cancel (priv->cancellable);
  g_clear_object (&priv->cancellable);

  g_clear_object (&priv->proxy);
  g_clear_object (&priv->oauth2);
  g_clear_object (&priv->client);
  g_clear_pointer (&priv->access_token, g_free);
  g_clear_pointer (&priv->consumer_key, g_free);

  G_OBJECT_CLASS (gnome_pocket_parent_class)->finalize (object);
}

static void
gnome_pocket_class_init (GnomePocketClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  cache_path = g_build_filename (g_get_user_cache_dir (), "pocket", NULL);
  g_mkdir_with_parents (cache_path, 0700);

  object_class->get_property = gnome_pocket_get_property;
  object_class->finalize = gnome_pocket_finalize;

  g_object_class_install_property (object_class,
                                   PROP_AVAILABLE,
                                   g_param_spec_boolean ("available",
                                                         "Available",
                                                         "If Read Pocket is available",
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  g_type_class_add_private (object_class, sizeof (GnomePocketPrivate));
}

static void
got_access_token (GObject       *object,
                  GAsyncResult  *res,
                  GnomePocket *self)
{
  GError *error = NULL;
  char *access_token;

  if (!goa_oauth2_based_call_get_access_token_finish (GOA_OAUTH2_BASED (object),
                                                      &access_token,
                                                      NULL,
                                                      res,
                                                      &error)) {
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      g_error_free (error);
      return;
    }
    g_warning ("Failed to get access token: %s", error->message);
    g_error_free (error);
    return;
  }

  self->priv->access_token = access_token;
  self->priv->consumer_key = goa_oauth2_based_dup_client_id (GOA_OAUTH2_BASED (object));

  g_object_notify (G_OBJECT (self), "available");
}

static void
handle_accounts (GnomePocket *self)
{
  GList *accounts, *l;
  GoaOAuth2Based *oauth2 = NULL;

  g_clear_object (&self->priv->oauth2);
  g_clear_pointer (&self->priv->access_token, g_free);
  g_clear_pointer (&self->priv->consumer_key, g_free);

  accounts = goa_client_get_accounts (self->priv->client);

  for (l = accounts; l != NULL; l = l->next) {
    GoaObject *object = GOA_OBJECT (l->data);
    GoaAccount *account;

    account = goa_object_peek_account (object);

    /* Find a Pocket account that doesn't have "Read Pocket" disabled */
    if (g_strcmp0 (goa_account_get_provider_type (account), "pocket") == 0 &&
        !goa_account_get_read_later_disabled (account)) {
      oauth2 = goa_object_get_oauth2_based (object);
      break;
    }
  }

  g_list_free_full (accounts, (GDestroyNotify) g_object_unref);

  if (!oauth2) {
    g_object_notify (G_OBJECT (self), "available");
    g_debug ("Could not find a Pocket account");
    return;
  }

  self->priv->oauth2 = oauth2;

  goa_oauth2_based_call_get_access_token (oauth2,
                                          self->priv->cancellable,
                                          (GAsyncReadyCallback) got_access_token,
                                          self);
}

static void
account_added_cb (GoaClient     *client,
                  GoaObject     *object,
                  GnomePocket *self)
{
  if (self->priv->oauth2 != NULL) {
    /* Don't care, already have an account */
    return;
  }

  handle_accounts (self);
}

static void
account_changed_cb (GoaClient     *client,
                    GoaObject     *object,
                    GnomePocket *self)
{
  GoaOAuth2Based *oauth2;

  oauth2 = goa_object_get_oauth2_based (object);
  if (oauth2 == self->priv->oauth2)
    handle_accounts (self);

  g_object_unref (oauth2);
}

static void
account_removed_cb (GoaClient     *client,
                    GoaObject     *object,
                    GnomePocket *self)
{
  GoaOAuth2Based *oauth2;

  oauth2 = goa_object_get_oauth2_based (object);
  if (oauth2 == self->priv->oauth2)
    handle_accounts (self);

  g_object_unref (oauth2);
}

static void
client_ready_cb (GObject       *source_object,
                 GAsyncResult  *res,
                 GnomePocket *self)
{
  GoaClient *client;
  GError *error = NULL;

  client = goa_client_new_finish (res, &error);
  if (client == NULL) {
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      g_error_free (error);
      return;
    }
    g_warning ("Failed to get GoaClient: %s", error->message);
    g_error_free (error);
    return;
  }

  self->priv->client = client;
  g_signal_connect (self->priv->client, "account-added",
                    G_CALLBACK (account_added_cb), self);
  g_signal_connect (self->priv->client, "account-changed",
                    G_CALLBACK (account_changed_cb), self);
  g_signal_connect (self->priv->client, "account-removed",
                    G_CALLBACK (account_removed_cb), self);

  handle_accounts (self);
}

static void
gnome_pocket_init (GnomePocket *self)
{
  self->priv = GNOME_POCKET_GET_PRIVATE (self);
  self->priv->cancellable = g_cancellable_new ();
  self->priv->proxy = rest_proxy_new ("https://getpocket.com/", FALSE);

  goa_client_new (self->priv->cancellable,
                  (GAsyncReadyCallback) client_ready_cb, self);
}

GnomePocket *
gnome_pocket_new (void)
{
  return g_object_new (GNOME_TYPE_POCKET, NULL);
}

/*
 * vim: sw=2 ts=8 cindent noai bs=2
 */
