/*
 * Copyright (C) 2012 Canonical Ltd.
 *
 * Author: Jens Georg <jensg@openismus.com>
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

#include <string.h>

#include <grilo.h>
#include <net/grl-net.h>
#include <json-glib/json-glib.h>

#include "grl-tmdb-request.h"

#define GRL_LOG_DOMAIN_DEFAULT tmdb_log_domain
GRL_LOG_DOMAIN_EXTERN(tmdb_log_domain);

/* URIs for TMDb.org API V3 */
#define TMDB_BASE_URI "https://api.themoviedb.org/3/"
#define TMDB_API_CALL_CONFIGURATION "configuration"
#define TMDB_API_CALL_SEARCH_MOVIE "search/movie"
#define TMDB_API_CALL_MOVIE_INFO "movie/%"G_GUINT64_FORMAT
#define TMDB_API_CALL_MOVIE_CAST TMDB_API_CALL_MOVIE_INFO"/casts"
#define TMDB_API_CALL_MOVIE_IMAGES TMDB_API_CALL_MOVIE_INFO"/images"
#define TMDB_API_CALL_MOVIE_KEYWORDS TMDB_API_CALL_MOVIE_INFO"/keywords"
#define TMDB_API_CALL_MOVIE_RELEASE_INFO TMDB_API_CALL_MOVIE_INFO"/releases"

#define TMDB_API_PARAM_MOVIE_CAST "casts"
#define TMDB_API_PARAM_MOVIE_IMAGES "images"
#define TMDB_API_PARAM_MOVIE_KEYWORDS "keywords"
#define TMDB_API_PARAM_MOVIE_RELEASE_INFO "releases"

struct _FilterClosure {
  JsonArrayForeach callback;
  GrlTmdbRequestFilterFunc filter;
  GrlTmdbRequestStringFilterFunc string_filter;
  GList *list;
};

typedef struct _FilterClosure FilterClosure;

/* GObject setup functions */
static void grl_tmdb_request_class_init (GrlTmdbRequestClass * klass);
static void grl_tmdb_request_init (GrlTmdbRequest *self);

/* GObject vfuncs */
static void grl_tmdb_request_set_property (GObject *object,
                                           guint property_id,
                                           const GValue *value,
                                           GParamSpec *pspec);
static void grl_tmdb_request_finalize (GObject *object);

static void grl_tmdb_request_constructed (GObject *object);

enum {
  PROP_0,
  PROP_URI,
  PROP_API_KEY,
  PROP_ARGS
};

struct _GrlTmdbRequestPrivate {
  char *uri;
  char *api_key;
  GHashTable *args;
  GUri *base;
  GTask *task;
  JsonParser *parser;
  GrlTmdbRequestDetail detail;
  GList *details;
};

G_DEFINE_TYPE_WITH_PRIVATE (GrlTmdbRequest, grl_tmdb_request, G_TYPE_OBJECT)

/* Implementation */

/* GObject functions */

/* GObject setup functions */

static void
grl_tmdb_request_class_init (GrlTmdbRequestClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = grl_tmdb_request_set_property;
  gobject_class->finalize = grl_tmdb_request_finalize;
  gobject_class->constructed = grl_tmdb_request_constructed;

  g_object_class_install_property (gobject_class,
                                   PROP_URI,
                                   g_param_spec_string ("uri",
                                                        "uri",
                                                        "URI used for the request",
                                                        NULL,
                                                        G_PARAM_WRITABLE
                                                        | G_PARAM_CONSTRUCT_ONLY
                                                        | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_API_KEY,
                                   g_param_spec_string ("api-key",
                                                        "api-key",
                                                        "TMDb API key",
                                                        NULL,
                                                        G_PARAM_WRITABLE
                                                        | G_PARAM_CONSTRUCT_ONLY
                                                        | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
                                   PROP_ARGS,
                                   g_param_spec_boxed ("args",
                                                       "args",
                                                       "HTTP GET arguments",
                                                       G_TYPE_HASH_TABLE,
                                                       G_PARAM_WRITABLE
                                                       | G_PARAM_CONSTRUCT_ONLY
                                                       | G_PARAM_STATIC_STRINGS));
}

static void
grl_tmdb_request_init (GrlTmdbRequest *self)
{
  self->priv = grl_tmdb_request_get_instance_private (self);
  self->priv->base = g_uri_parse (TMDB_BASE_URI, G_URI_FLAGS_NONE, NULL);
  self->priv->parser = json_parser_new ();
  self->priv->detail = GRL_TMDB_REQUEST_DETAIL_COUNT;
}

/* GObject vfuncs */
static void
grl_tmdb_request_set_property (GObject *object,
                               guint property_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
  GrlTmdbRequest *self = GRL_TMDB_REQUEST (object);

  switch (property_id) {
    case PROP_API_KEY:
      self->priv->api_key = g_value_dup_string (value);
      break;
    case PROP_URI:
      self->priv->uri = g_value_dup_string (value);
      break;
    case PROP_ARGS:
      self->priv->args = g_value_dup_boxed (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
grl_tmdb_request_finalize (GObject *object)
{
  GrlTmdbRequest *self = GRL_TMDB_REQUEST (object);

  g_list_free (self->priv->details);
  g_clear_pointer (&self->priv->api_key, g_free);
  g_clear_pointer (&self->priv->uri, g_free);
  g_clear_pointer (&self->priv->args, g_hash_table_unref);
  g_clear_pointer (&self->priv->base, g_uri_unref);
  g_clear_object (&self->priv->parser);

  G_OBJECT_CLASS (grl_tmdb_request_parent_class)->finalize (object);
}

static void
grl_tmdb_request_constructed (GObject *object)
{
  GrlTmdbRequest *self = GRL_TMDB_REQUEST (object);

  if (self->priv->args == NULL) {
    self->priv->args = g_hash_table_new_full (g_str_hash,
                                              g_str_equal,
                                              NULL,
                                              g_free);
  }
  g_hash_table_insert (self->priv->args, "api_key", g_strdup (self->priv->api_key));

  G_OBJECT_CLASS (grl_tmdb_request_parent_class)->constructed (object);
}


/* Private functions */
static void
fill_list_filtered (JsonArray *array,
                    guint index_,
                    JsonNode *element,
                    gpointer user_data)
{
  FilterClosure *closure = (FilterClosure *) user_data;

  if (closure->filter == NULL || closure->filter (element)) {
    closure->list = g_list_prepend (closure->list, json_node_copy (element));
  }
}

static void
fill_string_list_filtered (JsonArray *array,
                           guint index_,
                           JsonNode *element,
                           gpointer user_data)
{
  FilterClosure *closure = (FilterClosure *) user_data;
  char *result;

  if (closure->string_filter == NULL) {
    closure->list = g_list_prepend (closure->list,
                                    g_strdup (json_node_get_string (element)));
    return;
  }

  result = closure->string_filter (element);
  if (result != NULL) {
    closure->list = g_list_prepend (closure->list, result);
  }
}

static GList *
get_list_with_filter (GrlTmdbRequest *self,
                      const char *path,
                      FilterClosure *closure)
{
  JsonNode *node, *element;
  GError *error = NULL;
  JsonArray *values;

  node = json_path_query (path,
                          json_parser_get_root (self->priv->parser),
                          &error);
  if (error != NULL) {
    GRL_DEBUG ("Failed to get %s: %s", path, error->message);
    g_error_free (error);
    return NULL;
  }

  if (!JSON_NODE_HOLDS_ARRAY (node)) {
    json_node_free (node);
    return NULL;
  }

  values = json_node_get_array (node);
  if (json_array_get_length (values) == 0) {
    json_node_free (node);
    return NULL;
  }

  /* Check if we have array in array */
  element = json_array_get_element (values, 0);
  if (JSON_NODE_HOLDS_ARRAY (element)) {
    values = json_node_get_array (element);
  }

  closure->list = NULL;

  json_array_foreach_element (values, closure->callback, closure);

  json_node_free (node);

  return closure->list;
}

/* Callbacks */
static void
on_wc_request (GrlNetWc *wc,
               GAsyncResult *res,
               gpointer user_data)
{
  GrlTmdbRequest *self = GRL_TMDB_REQUEST (user_data);
  char *content;
  gsize length = 0;
  GError *error = NULL;

  if (!grl_net_wc_request_finish (wc, res, &content, &length, &error)) {
    g_task_return_error (self->priv->task, error);

    goto out;
  }

  if (!json_parser_load_from_data (self->priv->parser, content, length, &error)) {
    GRL_WARNING ("Could not parse JSON: %s", error->message);
    g_task_return_error (self->priv->task, error);

    goto out;
  }

  g_task_return_boolean (self->priv->task, TRUE);

out:
  g_object_unref (self->priv->task);
}

/* Public functions */
/**
 * grl_tmdb_request_new:
 * @api_key: TMDb.org API key to use for this request
 * @uri: URI fragment for API call, i.e. /configuration or /movie/11
 * @args: (allow-none) (element-type utf8 utf8): Optional arguments to pass to
 * the function call
 * Returns: (transfer full): A new instance of GrlTmdbRequest
 *
 * Generic constructor for the convenience class to handle the async API HTTP
 * requests and JSON parsing of the result.
 */
GrlTmdbRequest *
grl_tmdb_request_new (const char *api_key, const char *uri, GHashTable *args)
{
  return g_object_new (GRL_TMDB_REQUEST_TYPE,
                       "api-key", api_key,
                       "uri", uri,
                       "args", args,
                       NULL);
}


/**
 * grl_tmdb_request_new_search:
 * @api_key: TMDb.org API key to use for this request
 * @needle: The term to search for
 * Returns: (transfer full): A new instance of #GrlTmdbRequest
 *
 * Convenience function to create a #GrlTmdbRequest that performs a movie search
 */
GrlTmdbRequest *
grl_tmdb_request_new_search (const char *api_key, const char *needle)
{
  GHashTable *args;
  GrlTmdbRequest *result;

  args = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
  g_hash_table_insert (args, "query", g_strdup (needle));

  result = g_object_new (GRL_TMDB_REQUEST_TYPE,
                         "api-key", api_key,
                         "uri", TMDB_API_CALL_SEARCH_MOVIE,
                         "args", args,
                         NULL);
  g_hash_table_unref (args);

  return result;
}

/**
 * grl_tmdb_request_new_details:
 * @api_key: TMDb.org API key to use for this request
 * @detail: The detailed information to request for the movie @id
 * @id: TMDb.org identifier of the movie.
 * Returns: (transfer full): A new instance of #GrlTmdbRequest
 *
 * Convenience function to create a #GrlTmdbRequest that gets detailed
 * information about a movie.
 */
GrlTmdbRequest *
grl_tmdb_request_new_details (const char *api_key,
                              GrlTmdbRequestDetail detail,
                              guint64 id)
{
  GrlTmdbRequest *result;
  char *uri;

  switch (detail) {
    case GRL_TMDB_REQUEST_DETAIL_MOVIE:
      uri = g_strdup_printf (TMDB_API_CALL_MOVIE_INFO, id);
      break;
    case GRL_TMDB_REQUEST_DETAIL_MOVIE_CAST:
      uri = g_strdup_printf (TMDB_API_CALL_MOVIE_CAST, id);
      break;
    case GRL_TMDB_REQUEST_DETAIL_MOVIE_IMAGES:
      uri = g_strdup_printf (TMDB_API_CALL_MOVIE_IMAGES, id);
      break;
    case GRL_TMDB_REQUEST_DETAIL_MOVIE_KEYWORDS:
      uri = g_strdup_printf (TMDB_API_CALL_MOVIE_KEYWORDS, id);
      break;
    case GRL_TMDB_REQUEST_DETAIL_MOVIE_RELEASE_INFO:
      uri = g_strdup_printf (TMDB_API_CALL_MOVIE_RELEASE_INFO, id);
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  result = g_object_new (GRL_TMDB_REQUEST_TYPE,
                         "api-key", api_key,
                         "uri", uri,
                         "args", NULL,
                         NULL);
  result->priv->detail = detail;
  g_free (uri);

  return result;
}

/**
 * grl_tmdb_request_new_details_list:
 * @api_key: TMDb.org API key to use for this request
 * @details: A list of #GrlTmdbRequestDetail to request
 * @id: TMDb.org identifier of the movie.
 * Returns: (transfer full): A new instance of #GrlTmdbRequest
 *
 * Convenience function to create a #GrlTmdbRequest that gets detailed
 * information about a movie.
 */
GrlTmdbRequest *
grl_tmdb_request_new_details_list (const char *api_key,
                                   GList *details,
                                   guint64 id)
{
  GrlTmdbRequest *result;
  char *uri;

  g_return_val_if_fail (details != NULL, NULL);

  uri = g_strdup_printf (TMDB_API_CALL_MOVIE_INFO, id);
  result = g_object_new (GRL_TMDB_REQUEST_TYPE,
                         "api-key", api_key,
                         "uri", uri,
                         "args", NULL,
                         NULL);
  g_free (uri);

  result->priv->details = g_list_copy (details);


  return result;
}

GrlTmdbRequest *
grl_tmdb_request_new_configuration (const char *api_key)
{
  return g_object_new (GRL_TMDB_REQUEST_TYPE,
                       "api-key", api_key,
                       "uri", TMDB_API_CALL_CONFIGURATION,
                       "args", NULL,
                       NULL);
}

static const char *
id_to_param (GrlTmdbRequestDetail detail)
{
  switch (detail) {
    case GRL_TMDB_REQUEST_DETAIL_MOVIE_CAST:
      return TMDB_API_PARAM_MOVIE_CAST;
    case GRL_TMDB_REQUEST_DETAIL_MOVIE_IMAGES:
      return TMDB_API_PARAM_MOVIE_IMAGES;
    case GRL_TMDB_REQUEST_DETAIL_MOVIE_KEYWORDS:
      return TMDB_API_PARAM_MOVIE_KEYWORDS;
    case GRL_TMDB_REQUEST_DETAIL_MOVIE_RELEASE_INFO:
      return TMDB_API_PARAM_MOVIE_RELEASE_INFO;
    default:
      return NULL;
  }
}

static char *
append_details_list (GrlTmdbRequest *self,
                     const char *call)
{
  GString *c;
  GList *l;
  gboolean added_comma = FALSE;

  if (self->priv->details == NULL)
    return NULL;

  c = g_string_new (call);
  g_string_append (c, "&append_to_response=");

  for (l = self->priv->details; l != NULL; l = l->next) {
    const char *param;

    param = id_to_param (GPOINTER_TO_UINT (l->data));
    if (!param)
      continue;
    g_string_append_printf (c, "%s,", id_to_param (GPOINTER_TO_UINT (l->data)));
    added_comma = TRUE;
  }

  /* Remove trailing comma */
  if (added_comma) {
    g_string_truncate (c, c->len - 1);
    return g_string_free (c, FALSE);
  }

  g_string_free (c, TRUE);
  return NULL;
}

static char *
plus_escape (const char *orig)
{
  GString *s = g_string_new (orig);
  g_string_replace (s, " ", "+", 0);
  return g_string_free (s, FALSE);
}

static char *
args_to_string (GHashTable *args)
{
  GHashTableIter iter;
  const char *key, *value;
  GString *s = NULL;

  s = g_string_new (NULL);
  g_hash_table_iter_init (&iter, args);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *)&value)) {
    g_autofree char *plus_escaped = NULL;
    if (s->len > 0)
      g_string_append_c (s, '&');
    g_string_append_uri_escaped (s, key, G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS, FALSE);
    g_string_append_c (s, '=');
    plus_escaped = plus_escape (value);
    g_string_append_uri_escaped (s, plus_escaped, G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS, FALSE);
  }
  return g_string_free (s, !(s->len > 0));
}

/**
 * grl_tmdb_request_run_async:
 * @self: Instance of GrlTmdbRequest
 * @callback: Callback to notify after the request is complete
 * @cancellable: (allow-none): An optional cancellable to cancel this operation
 * @user_data: User data to pass on to @callback.
 *
 * Schedule the request for execution.
 */
void
grl_tmdb_request_run_async (GrlTmdbRequest *self,
                            GrlNetWc *wc,
                            GAsyncReadyCallback callback,
                            GCancellable *cancellable,
                            gpointer user_data)
{
  g_autoptr(GUri) absolute_uri = NULL;
  g_autoptr(GUri) uri = NULL;
  g_autofree char *query = NULL;
  char *call, *new_call;
  GHashTable *headers;

  absolute_uri = g_uri_parse_relative (self->priv->base, self->priv->uri, G_URI_FLAGS_NONE, NULL);
  query = args_to_string (self->priv->args);
  uri = g_uri_build (G_URI_FLAGS_NONE,
                     g_uri_get_scheme (absolute_uri),
                     g_uri_get_userinfo (absolute_uri),
                     g_uri_get_host (absolute_uri),
                     g_uri_get_port (absolute_uri),
                     g_uri_get_path (absolute_uri),
                     query,
                     g_uri_get_fragment (absolute_uri));
  call = g_uri_to_string (uri);

  new_call = append_details_list (self, call);
  if (new_call != NULL) {
    g_free (call);
    call = new_call;
  }

  if (self->priv->task != NULL) {
      GRL_WARNING("Request %p to %s is already in progress", self, call);
      g_free (call);
      return;
  }

  self->priv->task = g_task_new (G_OBJECT (self),
                                 cancellable,
                                 callback,
                                 user_data);

  GRL_DEBUG ("Requesting %s", call);

  headers = g_hash_table_new (g_str_hash, g_str_equal);
  g_hash_table_insert (headers, "Accept", "application/json");

  grl_net_wc_request_with_headers_hash_async (wc,
                                              call,
                                              headers,
                                              cancellable,
                                              (GAsyncReadyCallback) on_wc_request,
                                              self);
  g_free (call);
  g_hash_table_unref (headers);
}

/**
 * grl_tmdb_request_run_finish:
 * @self: Instance of GrlTmdbRequest
 * @result: #GAsyncResult of the operation
 * @error: (allow-none): Return location for a possible error
 * @returns: %TRUE if the request succeeded, %FALSE otherwise.
 *
 * Finalize a request previously scheduled with grl_tmdb_request_run_async().
 * Usually called from the call-back passed to this function. After this
 * grl_tmdb_request_run_finish() returned %TRUE, grl_tmdb_request_get() should
 * return proper data.
 */
gboolean
grl_tmdb_request_run_finish (GrlTmdbRequest *self,
                             GAsyncResult *result,
                             GError **error)
{
  GTask *task;

  if (!g_task_is_valid (result, self)) {
    return FALSE;
  }

  task = G_TASK (result);
  return g_task_propagate_boolean(task, error);
}

/**
 * grl_tmdb_request_get:
 * @self: Instance of GrlTmdbRequest
 * @path: JSONPath to get
 * @returns: (transfer full): %NULL if the key cannot be found or
 * the request is otherwise in error or the value of the key.
 *
 * Get a value from the API call represented by this instance.
 */
GValue *
grl_tmdb_request_get (GrlTmdbRequest *self,
                      const char *path)
{
  JsonNode *node;
  JsonNode *element;
  GError *error = NULL;
  GValue *value = NULL;
  JsonArray *values;

  node = json_path_query (path,
                          json_parser_get_root (self->priv->parser),
                          &error);
  if (error != NULL) {
    GRL_DEBUG ("Failed to get %s: %s", path, error->message);
    g_error_free (error);

    return NULL;
  }

  values = json_node_get_array (node);
  element = json_array_get_element (values, 0);

  if (JSON_NODE_HOLDS_VALUE (element)) {
    value = g_new0 (GValue, 1);
    json_node_get_value (element, value);
  }

  json_node_free (node);

  return value;
}

/**
 * grl_tmdb_request_get_string_list:
 * @self: Instance of GrlTmdbRequest
 * @path: JSONPath to get
 * Returns: (transfer full) (element-type utf-8): %NULL if the path cannot be
 * found or a #GList containing strings matching the path.
 */
GList *
grl_tmdb_request_get_string_list (GrlTmdbRequest *self,
                                  const char *path)
{
  return grl_tmdb_request_get_string_list_with_filter (self, path, NULL);
}

/**
 * grl_tmdb_request_get_list_with_filter:
 * @self: Instance of #GrlTmdbRequest
 * @path: JSONPath to get
 * @filter: A #GrlTmdbRequestFilterFunc to match on a #JsonNode
 * Returns: (transfer container) (element-type JsonNode): %NULL if the path
 * cannot be found or no node matched the filter or a #GList containing #JsNode
 * instances matching the path and are accepted by the filter.
 */
GList *
grl_tmdb_request_get_list_with_filter (GrlTmdbRequest *self,
                                       const char *path,
                                       GrlTmdbRequestFilterFunc filter)
{
  FilterClosure closure;

  closure.list = NULL;
  closure.filter = filter;
  closure.callback = fill_list_filtered;

  get_list_with_filter (self, path, &closure);

  return closure.list;
}

/**
 * grl_tmdb_request_get_string_list_with_filter:
 * @self: Instance of #GrlTmdbRequest
 * @path: JSONPath to get
 * @filter: A #GrlTmdbRequestStringFilterFunc to match on a #JsonNode
 * Returns: (transfer full) (element-type utf-8): %NULL if the path cannot be
 * found or no node matched the filter or a #GList containing strings matching
 * the path and are accepted by the filter.
 */
GList *
grl_tmdb_request_get_string_list_with_filter (GrlTmdbRequest *self,
                                              const char *path,
                                              GrlTmdbRequestStringFilterFunc filter)
{
  FilterClosure closure;

  closure.list = NULL;
  closure.string_filter = filter;
  closure.callback = fill_string_list_filtered;

  get_list_with_filter (self, path, &closure);

  return g_list_reverse (closure.list);
}

/**
 * grl_tmdb_request_get_detail:
 * @self: Instance of #GrlTmdbRequest
 * Returns: An id of #GrlTmdbRequestDetail or #GRL_TMDB_REQUEST_DETAIL_NONE if
 * the request is not a detail request.
 */
GrlTmdbRequestDetail
grl_tmdb_request_get_detail (GrlTmdbRequest *self)
{
  return self->priv->detail;
}

/**
 * grl_tmdb_request_get_uri:
 * @self: Instance of #GrlTmdbRequest
 * Returns: The URI for the request. Mostly useful for debugging or error
 * messages
 */
const char *
grl_tmdb_request_get_uri (GrlTmdbRequest *self)
{
  return self->priv->uri;
}

/**
 * grl_tmdb_request_detail_to_string:
 * @detail: A #GrlTmdbRequestDetail
 * Returns: A description of the detail or %NULL for invalid details.
 */
const char *
grl_tmdb_request_detail_to_string (GrlTmdbRequestDetail detail)
{
  switch (detail) {
    case GRL_TMDB_REQUEST_DETAIL_MOVIE:
      return "generic details";
    case GRL_TMDB_REQUEST_DETAIL_MOVIE_CAST:
      return "casts";
    case GRL_TMDB_REQUEST_DETAIL_MOVIE_IMAGES:
      return "images";
    case GRL_TMDB_REQUEST_DETAIL_MOVIE_KEYWORDS:
      return "keywords";
    case GRL_TMDB_REQUEST_DETAIL_MOVIE_RELEASE_INFO:
      return "release information";
    case GRL_TMDB_REQUEST_DETAIL_COUNT:
      break;
  }

  g_warn_if_reached ();
  return NULL;
}
