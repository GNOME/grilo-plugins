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
#include <stdlib.h>

#include <grilo.h>
#include <net/grl-net.h>
#include <json-glib/json-glib.h>
#include <glib/gi18n-lib.h>

#include "grl-tmdb.h"
#include "grl-tmdb-request.h"

#define GRL_LOG_DOMAIN_DEFAULT tmdb_log_domain
GRL_LOG_DOMAIN(tmdb_log_domain);

#define SOURCE_ID   "grl-tmdb"
#define SOURCE_NAME "TMDb Metadata Provider"
#define SOURCE_DESC "A source for movie metadata from themoviedb.org"

#define SHOULD_RESOLVE(key) \
    g_hash_table_contains (closure->keys, GRLKEYID_TO_POINTER ((key)))

enum {
  PROP_0,
  PROP_API_KEY
};

static GrlKeyID GRL_TMDB_METADATA_KEY_BACKDROP = GRL_METADATA_KEY_INVALID;
static GrlKeyID GRL_TMDB_METADATA_KEY_POSTER = GRL_METADATA_KEY_INVALID;
static GrlKeyID GRL_TMDB_METADATA_KEY_IMDB_ID = GRL_METADATA_KEY_INVALID;
static GrlKeyID GRL_TMDB_METADATA_KEY_TMDB_ID = GRL_METADATA_KEY_INVALID;

struct _GrlTmdbSourcePrivate {
  char *api_key;
  GHashTable *supported_keys;
  GHashTable *slow_keys;
  GrlNetWc *wc;
  GrlTmdbRequest *configuration;
  gboolean config_pending;
  GQueue *pending_resolves;
  GUri *image_base_uri;
};

struct _ResolveClosure {
  GrlTmdbSource *self;
  GrlSourceResolveSpec *rs;
  GQueue *pending_requests;
  guint64 id;
  GHashTable *keys;
  gboolean slow;
};

struct _PendingRequest {
  GrlTmdbRequest *request;
  GAsyncReadyCallback callback;
};

typedef struct _ResolveClosure ResolveClosure;
typedef struct _PendingRequest PendingRequest;

static GrlTmdbSource *grl_tmdb_source_new (const char *api_key);

static void grl_tmdb_source_resolve (GrlSource *source,
                                     GrlSourceResolveSpec *rs);

static const
GList *grl_tmdb_source_supported_keys (GrlSource *source);

static const GList *
grl_tmdb_source_slow_keys (GrlSource *source);

static gboolean grl_tmdb_source_may_resolve (GrlSource *source,
                                             GrlMedia *media,
                                             GrlKeyID key_id,
                                             GList **missing_keys);

gboolean grl_tmdb_source_plugin_init (GrlRegistry *registry,
                                      GrlPlugin *plugin,
                                      GList *configs);

static void grl_tmdb_source_set_property (GObject *object,
                                          guint property_id,
                                          const GValue *value,
                                          GParamSpec *pspec);

static void grl_tmdb_source_finalize (GObject *object);

static GrlKeyID
register_metadata_key (GrlRegistry *registry,
                       GrlKeyID bind_key,
                       const char *name,
                       const char *nick,
                       const char *blurb);

static void resolve_closure_free (ResolveClosure *closure);
static void resolve_slow_details (ResolveClosure *closure);

/* =================== GrlTmdbMetadata Plugin  =============== */


gboolean
grl_tmdb_source_plugin_init (GrlRegistry *registry,
                             GrlPlugin *plugin,
                             GList *configs)
{
  GrlConfig *config;
  char *api_key;

  GRL_LOG_DOMAIN_INIT (tmdb_log_domain, "tmdb");

  GRL_DEBUG ("grl_tmdb_source_plugin_init");

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  if (!configs) {
    GRL_INFO ("No configuration provided. Will not load plugin");
    return FALSE;
  }

  config = GRL_CONFIG (configs->data);
  api_key = grl_config_get_api_key (config);
  if (!api_key) {
    GRL_INFO ("Missing API Key, cannot load plugin");
    return FALSE;
  }

  GrlTmdbSource *source = grl_tmdb_source_new (api_key);
  grl_registry_register_source (registry,
                                       plugin,
                                       GRL_SOURCE (source),
                                       NULL);
  g_free (api_key);
  return TRUE;
}

static void
grl_tmdb_source_plugin_register_keys (GrlRegistry *registry,
                                      GrlPlugin   *plugin)
{
  GRL_TMDB_METADATA_KEY_BACKDROP =
    register_metadata_key (registry,
                           GRL_METADATA_KEY_INVALID,
                           "tmdb-backdrop",
                           "tmdb-backdrop",
                           "A list of URLs for movie backdrops");

  GRL_TMDB_METADATA_KEY_POSTER =
    register_metadata_key (registry,
                           GRL_METADATA_KEY_INVALID,
                           "tmdb-poster",
                           "tmdb-poster",
                           "A list of URLs for movie posters");

  GRL_TMDB_METADATA_KEY_IMDB_ID =
    register_metadata_key (registry,
                           GRL_METADATA_KEY_INVALID,
                           "tmdb-imdb-id",
                           "tmdb-imdb-id",
                           "ID of this movie at imdb.org");

  GRL_TMDB_METADATA_KEY_TMDB_ID =
    register_metadata_key (registry,
                           GRL_METADATA_KEY_INVALID,
                           "tmdb-id",
                           "tmdb-id",
                           "ID of this movie at tmdb.org");
}

GRL_PLUGIN_DEFINE (GRL_MAJOR,
                   GRL_MINOR,
                   TMDB_PLUGIN_ID,
                   "TMDb Metadata Provider",
                   "A plugin to retrieve movie metadata from themoviedb.org",
                   "Canonical Ltd.",
                   VERSION,
                   "LGPL-2.1-or-later",
                   "http://www.canonical.com",
                   grl_tmdb_source_plugin_init,
                   NULL,
                   grl_tmdb_source_plugin_register_keys);

/* ================== GrlTmdbMetadata GObject ================ */

G_DEFINE_TYPE_WITH_PRIVATE (GrlTmdbSource, grl_tmdb_source, GRL_TYPE_SOURCE)

static GrlTmdbSource *
grl_tmdb_source_new (const char *api_key)
{
  const char *tags[] = {
    "cinema",
    "net:internet",
    NULL
  };
  GRL_DEBUG ("grl_tmdb_source_new");
  return g_object_new (GRL_TMDB_SOURCE_TYPE,
                       "source-id", SOURCE_ID,
                       "source-name", SOURCE_NAME,
                       "source-desc", SOURCE_DESC,
                       "api-key", api_key,
                       "source-tags", tags,
                       NULL);
}

static void
grl_tmdb_source_class_init (GrlTmdbSourceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GrlSourceClass *metadata_class = GRL_SOURCE_CLASS (klass);

  metadata_class->supported_keys = grl_tmdb_source_supported_keys;
  metadata_class->slow_keys = grl_tmdb_source_slow_keys;
  metadata_class->may_resolve = grl_tmdb_source_may_resolve;
  metadata_class->resolve = grl_tmdb_source_resolve;

  gobject_class->set_property = grl_tmdb_source_set_property;
  gobject_class->finalize = grl_tmdb_source_finalize;

  g_object_class_install_property (gobject_class,
                                   PROP_API_KEY,
                                   g_param_spec_string ("api-key",
                                                        "api-key",
                                                        "TMDb API key",
                                                        NULL,
                                                        G_PARAM_WRITABLE
                                                        | G_PARAM_CONSTRUCT_ONLY
                                                        | G_PARAM_STATIC_STRINGS));
}

static void
grl_tmdb_source_init (GrlTmdbSource *self)
{
  self->priv = grl_tmdb_source_get_instance_private (self);
  self->priv->supported_keys = g_hash_table_new (g_direct_hash, g_direct_equal);
  self->priv->slow_keys = g_hash_table_new (g_direct_hash, g_direct_equal);

  /* Fast keys */
  g_hash_table_add (self->priv->supported_keys,
                    GRLKEYID_TO_POINTER (GRL_METADATA_KEY_TITLE));
  g_hash_table_add (self->priv->supported_keys,
                    GRLKEYID_TO_POINTER (GRL_METADATA_KEY_THUMBNAIL));
  g_hash_table_add (self->priv->supported_keys,
                    GRLKEYID_TO_POINTER (GRL_TMDB_METADATA_KEY_BACKDROP));
  g_hash_table_add (self->priv->supported_keys,
                    GRLKEYID_TO_POINTER (GRL_TMDB_METADATA_KEY_POSTER));
  g_hash_table_add (self->priv->supported_keys,
                    GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ORIGINAL_TITLE));
  g_hash_table_add (self->priv->supported_keys,
                    GRLKEYID_TO_POINTER (GRL_METADATA_KEY_RATING));
  g_hash_table_add (self->priv->supported_keys,
                    GRLKEYID_TO_POINTER (GRL_TMDB_METADATA_KEY_TMDB_ID));

  /* Slow keys */
  g_hash_table_add (self->priv->slow_keys,
                    GRLKEYID_TO_POINTER (GRL_METADATA_KEY_SITE));
  g_hash_table_add (self->priv->slow_keys,
                    GRLKEYID_TO_POINTER (GRL_METADATA_KEY_GENRE));
  g_hash_table_add (self->priv->slow_keys,
                    GRLKEYID_TO_POINTER (GRL_METADATA_KEY_STUDIO));
  g_hash_table_add (self->priv->slow_keys,
                    GRLKEYID_TO_POINTER (GRL_METADATA_KEY_DESCRIPTION));
  g_hash_table_add (self->priv->slow_keys,
                    GRLKEYID_TO_POINTER (GRL_METADATA_KEY_CERTIFICATE));
  g_hash_table_add (self->priv->slow_keys,
                    GRLKEYID_TO_POINTER (GRL_METADATA_KEY_REGION));
  g_hash_table_add (self->priv->slow_keys,
                    GRLKEYID_TO_POINTER (GRL_TMDB_METADATA_KEY_IMDB_ID));
  g_hash_table_add (self->priv->slow_keys,
                    GRLKEYID_TO_POINTER (GRL_METADATA_KEY_KEYWORD));
  g_hash_table_add (self->priv->slow_keys,
                    GRLKEYID_TO_POINTER (GRL_METADATA_KEY_PERFORMER));
  g_hash_table_add (self->priv->slow_keys,
                    GRLKEYID_TO_POINTER (GRL_METADATA_KEY_PRODUCER));
  g_hash_table_add (self->priv->slow_keys,
                    GRLKEYID_TO_POINTER (GRL_METADATA_KEY_DIRECTOR));
  g_hash_table_add (self->priv->slow_keys,
                    GRLKEYID_TO_POINTER (GRL_METADATA_KEY_AUTHOR));

  /* The publication date is both available as fast key in the movie details,
   * but also as more detailed information as regional release date. To avoid
   * confusion in clients that do a fast resolve first and merge slow data
   * later we hide the fast version.
   */
  g_hash_table_add (self->priv->slow_keys,
                    GRLKEYID_TO_POINTER (GRL_METADATA_KEY_PUBLICATION_DATE));

  self->priv->wc = grl_net_wc_new ();
  grl_net_wc_set_throttling (self->priv->wc, 1);

  self->priv->config_pending = FALSE;
  self->priv->pending_resolves = g_queue_new ();
}

/* GObject vfuncs */
static void
grl_tmdb_source_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
  GrlTmdbSource *self = GRL_TMDB_SOURCE (object);

  switch (property_id) {
    case PROP_API_KEY:
      self->priv->api_key = g_value_dup_string (value);
      GRL_DEBUG ("Using API key %s", self->priv->api_key);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
grl_tmdb_source_finalize (GObject *object)
{
  GrlTmdbSource *self = GRL_TMDB_SOURCE (object);

  if (self->priv->supported_keys != NULL) {
    g_hash_table_unref (self->priv->supported_keys);
    self->priv->supported_keys = NULL;
  }

  if (self->priv->slow_keys != NULL) {
    g_hash_table_unref (self->priv->slow_keys);
    self->priv->slow_keys = NULL;
  }

  if (self->priv->api_key != NULL) {
    g_free (self->priv->api_key);
    self->priv->api_key = NULL;
  }

  g_clear_pointer (&self->priv->image_base_uri, g_uri_unref);

  if (self->priv->configuration != NULL) {
    g_object_unref (self->priv->configuration);
    self->priv->configuration = NULL;
  }

  if (self->priv->wc != NULL) {
    g_object_unref (self->priv->wc);
    self->priv->wc = NULL;
  }

  if (self->priv->pending_resolves != NULL) {
      g_queue_free_full (self->priv->pending_resolves,
                         (GDestroyNotify) resolve_closure_free);
      self->priv->pending_resolves = NULL;
  }

  G_OBJECT_CLASS (grl_tmdb_source_parent_class)->finalize (object);
}

/* Private functions */

static GrlKeyID
register_metadata_key (GrlRegistry *registry,
                       GrlKeyID bind_key,
                       const char *name,
                       const char *nick,
                       const char *blurb)
{
  GParamSpec *spec;
  GrlKeyID key;

  spec = g_param_spec_string (name,
                              nick,
                              blurb,
                              NULL,
                              G_PARAM_READWRITE
                              | G_PARAM_STATIC_STRINGS);

  key = grl_registry_register_metadata_key (registry, spec, bind_key, NULL);

  if (key == GRL_METADATA_KEY_INVALID) {
    key = grl_registry_lookup_metadata_key (registry, name);
    if (grl_metadata_key_get_type (key) != G_TYPE_STRING) {
      key = GRL_METADATA_KEY_INVALID;
    }
  }

  return key;
}

static void
resolve_closure_callback (ResolveClosure *closure,
                          const GError *outer_error)
{
  GError *error = NULL;

  if (outer_error && outer_error->domain != GRL_CORE_ERROR) {
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_RESOLVE_FAILED,
                                 outer_error->message);
  }

  closure->rs->callback (GRL_SOURCE (closure->self),
                         closure->rs->operation_id,
                         closure->rs->media,
                         closure->rs->user_data,
                         error);
  if (error)
    g_error_free (error);
}

static void queue_request (ResolveClosure *closure,
                           GrlTmdbRequest *request,
                           GAsyncReadyCallback callback)
{
  PendingRequest *pending_request;

  pending_request = g_slice_new0 (PendingRequest);
  pending_request->request = request;
  pending_request->callback = callback;

  g_queue_push_tail (closure->pending_requests, pending_request);
}

static int run_pending_requests (ResolveClosure *closure,
                                 int max_num_request)
{
  int num_requests = 0;
  GList *it;

  for (it = closure->pending_requests->head; it; it = it->next) {
    if (num_requests >= max_num_request)
      break;

    PendingRequest *const pending_request = it->data;

    grl_tmdb_request_run_async (pending_request->request,
                                closure->self->priv->wc,
                                pending_request->callback,
                                NULL,
                                closure);

    ++num_requests;
  }

  return num_requests;
}

static void pending_request_free (gpointer data)
{
  PendingRequest *const pending_request = data;
  g_object_unref (pending_request->request);
  g_slice_free (PendingRequest, pending_request);
}

static void remove_request (ResolveClosure *closure,
                            GrlTmdbRequest *request)
{
  GList *it;

  for (it = closure->pending_requests->head; it; it = it->next) {
    PendingRequest *const pending_request = it->data;

    if (pending_request->request == request) {
      g_queue_delete_link (closure->pending_requests, it);
      pending_request_free (pending_request);
      break;
    }
  }
}

static void
resolve_closure_free (ResolveClosure *closure)
{
  g_object_unref (closure->self);
  g_queue_free_full (closure->pending_requests, pending_request_free);
  g_hash_table_destroy (closure->keys);
  g_slice_free (ResolveClosure, closure);
}

static char *
producer_filter (JsonNode *element)
{
  JsonObject *object;
  const char *department;

  if (!JSON_NODE_HOLDS_OBJECT (element)) {
    return NULL;
  }

  object = json_node_get_object (element);

  department = json_object_get_string_member (object, "department");
  if (g_ascii_strcasecmp (department, "Production") != 0) {
    return NULL;
  }

  return g_strdup (json_object_get_string_member (object, "name"));
}

static char *
director_filter (JsonNode *element)
{
  JsonObject *object;
  const char *department;

  if (!JSON_NODE_HOLDS_OBJECT (element)) {
    return NULL;
  }

  object = json_node_get_object (element);

  department = json_object_get_string_member (object, "department");
  if (g_ascii_strcasecmp (department, "Directing") != 0) {
    return NULL;
  }

  return g_strdup (json_object_get_string_member (object, "name"));
}

static char *
writer_filter (JsonNode *element)
{
  JsonObject *object;
  const char *department;

  if (!JSON_NODE_HOLDS_OBJECT (element)) {
    return NULL;
  }

  object = json_node_get_object (element);

  department = json_object_get_string_member (object, "department");
  if (g_ascii_strcasecmp (department, "Writing") != 0) {
    return NULL;
  }

  return g_strdup (json_object_get_string_member (object, "name"));
}

static GDateTime *
parse_date (const gchar *iso8601_string)
{
  char *padded_date;
  GDateTime *date;
  GTimeVal tv;

  padded_date = g_strconcat (iso8601_string, "T00:00:00Z", NULL);
  g_time_val_from_iso8601 (padded_date, &tv);
  date = g_date_time_new_from_timeval_utc (&tv);
  g_free (padded_date);
  return date;
}

static char *
neutral_backdrop_filter (JsonNode *node)
{
  JsonObject *object;
  const char *language;

  if (!JSON_NODE_HOLDS_OBJECT (node)) {
    return NULL;
  }

  object = json_node_get_object (node);
  language = json_object_get_string_member (object, "iso_639_1");

  /* Language-neutral backdrops only */
  if (language == NULL) {
    return g_strdup (json_object_get_string_member (object, "file_path"));
  }

  return NULL;
}

static void
add_image (GrlTmdbSource *self,
           GrlMedia      *media,
           GrlKeyID       detail_key,
           const char    *image_path)
{
  g_autoptr(GUri) uri = NULL;
  GrlRelatedKeys *related_keys;
  char *str;
  int i, l;

  str = g_strconcat ("original", image_path, NULL);
  uri = g_uri_parse_relative (self->priv->image_base_uri, str, G_URI_FLAGS_NONE, NULL);
  g_free (str);

  str = g_uri_to_string (uri);

  l = grl_data_length (GRL_DATA (media), detail_key);

  for (i = 0; i < l; ++i) {
    related_keys = grl_data_get_related_keys (GRL_DATA (media), detail_key, i);
    if (g_strcmp0 (grl_related_keys_get_string (related_keys, detail_key), str) == 0)
      break;
  }

  if (i == l) {
    grl_data_add_string (GRL_DATA (media), detail_key, str);
  }

  g_free (str);
}

static void
on_request_ready (GObject *source,
                  GAsyncResult *result,
                  gpointer user_data) {
  ResolveClosure *closure = (ResolveClosure *) user_data;
  GrlTmdbRequest *request = GRL_TMDB_REQUEST (source);
  const GrlTmdbRequestDetail detail = grl_tmdb_request_get_detail (request);
  GError *error = NULL;
  GList *values, *iter;
  GValue *value;

  if (detail != GRL_TMDB_REQUEST_DETAIL_COUNT) {
    GRL_DEBUG ("Detail request (%s) ready for movie #%" G_GUINT64_FORMAT "...",
               grl_tmdb_request_detail_to_string (detail), closure->id);
  } else {
    GRL_DEBUG ("Detail request (aggregated) ready for movie #%" G_GUINT64_FORMAT "...",
               closure->id);
  }

  if (!grl_tmdb_request_run_finish (GRL_TMDB_REQUEST (source),
                                    result,
                                    &error)) {
    /* Just remove the request and hope the others have some data */
    GRL_WARNING ("Failed to get %s: %s",
                 grl_tmdb_request_get_uri (request),
                 error->message);
    goto out;
  }

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_GENRE)) {
    iter = values = grl_tmdb_request_get_string_list (request, "$.genres..name");
    while (iter != NULL) {
      grl_data_add_string (GRL_DATA (closure->rs->media),
                           GRL_METADATA_KEY_GENRE, iter->data);
      iter = iter->next;
    }
    g_list_free_full (values, g_free);
  }

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_STUDIO)) {
    iter = values = grl_tmdb_request_get_string_list (request, "$.production_companies..name");
    while (iter != NULL) {
      grl_data_add_string (GRL_DATA (closure->rs->media),
                           GRL_METADATA_KEY_STUDIO, iter->data);
      iter = iter->next;
    }
    g_list_free_full (values, g_free);
  }

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_SITE)) {
    value = grl_tmdb_request_get (request, "$.homepage");
    if (value != NULL) {
      grl_media_set_site (closure->rs->media, g_value_get_string (value));
      g_value_unset (value);
      g_free (value);
    }
  }

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_DESCRIPTION)) {
    value = grl_tmdb_request_get (request, "$.overview");
    if (value != NULL) {
      grl_media_set_description (closure->rs->media,
                                 g_value_get_string (value));
      g_value_unset (value);
      g_free (value);
    }
  }

  if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_IMDB_ID)) {
    value = grl_tmdb_request_get (request, "$.imdb_id");
    if (value != NULL) {
      grl_data_set_string (GRL_DATA (closure->rs->media),
                           GRL_TMDB_METADATA_KEY_IMDB_ID,
                           g_value_get_string (value));
      g_value_unset (value);
      g_free (value);
    }
  }

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_RATING)) {
    value = grl_tmdb_request_get (request, "$.vote_average");
    if (value != NULL) {
      grl_media_set_rating (closure->rs->media,
                            (float) g_value_get_double (value),
                            10.0f);
      g_value_unset (value);
      g_free (value);
    }
  }

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_ORIGINAL_TITLE)) {
    value = grl_tmdb_request_get (request, "$.original_title");
    if (value != NULL) {
      grl_media_set_original_title (closure->rs->media, g_value_get_string (value));
      g_value_unset (value);
      g_free (value);
    }
  }

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_TITLE)) {
    value = grl_tmdb_request_get (request, "$.title");
    if (value != NULL) {
      grl_media_set_title (closure->rs->media, g_value_get_string (value));
      grl_data_set_boolean (GRL_DATA (closure->rs->media), GRL_METADATA_KEY_TITLE_FROM_FILENAME, FALSE);
      g_value_unset (value);
      g_free (value);
    }
  }

  if (!closure->slow) {
    /* Add thumbnails first and poster and backdrops later.
     * Posters more likely make a good thumbnail than backdrops.
     */
    if (SHOULD_RESOLVE (GRL_METADATA_KEY_THUMBNAIL)) {
      value = grl_tmdb_request_get (request, "$.poster_path");
      if (value != NULL) {
          add_image (closure->self, closure->rs->media,
                     GRL_METADATA_KEY_THUMBNAIL,
                     g_value_get_string (value));

          g_value_unset (value);
          g_free (value);
      }
    }

    if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_POSTER)) {
      value = grl_tmdb_request_get (request, "$.poster_path");
      if (value != NULL) {
          add_image (closure->self, closure->rs->media,
                     GRL_TMDB_METADATA_KEY_POSTER,
                     g_value_get_string (value));

          g_value_unset (value);
          g_free (value);
      }
    }

    if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_BACKDROP)) {
      value = grl_tmdb_request_get (request, "$.backdrop_path");
      if (value != NULL) {
        add_image (closure->self, closure->rs->media,
                   GRL_TMDB_METADATA_KEY_BACKDROP,
                   g_value_get_string (value));

        g_value_unset (value);
        g_free (value);
      }
    }
  }

  /* Add thumbnails first, and posters and backdrops later.
   * Posters more likely make a good thumbnail than backdrops.
   */
  if (SHOULD_RESOLVE (GRL_METADATA_KEY_THUMBNAIL)) {
    values = grl_tmdb_request_get_string_list_with_filter (request,
                                                           "$.posters",
                                                           neutral_backdrop_filter);
    if (!values)
      values = grl_tmdb_request_get_string_list_with_filter (request,
                                                             "$.images.posters",
                                                             neutral_backdrop_filter);
    iter = values;
    while (iter != NULL) {
      add_image (closure->self, closure->rs->media,
                 GRL_METADATA_KEY_THUMBNAIL,
                 iter->data);

      iter = iter->next;
    }
    g_list_free_full (values, g_free);
  }

  if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_POSTER)) {
    values = grl_tmdb_request_get_string_list_with_filter (request,
                                                           "$.posters",
                                                           neutral_backdrop_filter);
    if (!values)
      values = grl_tmdb_request_get_string_list_with_filter (request,
                                                             "$.images.posters",
                                                             neutral_backdrop_filter);
    iter = values;
    while (iter != NULL) {
      add_image (closure->self, closure->rs->media,
                 GRL_TMDB_METADATA_KEY_POSTER,
                 iter->data);

      iter = iter->next;
    }
    g_list_free_full (values, g_free);
  }

  if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_BACKDROP)) {
    values = grl_tmdb_request_get_string_list_with_filter (request,
                                                           "$.backdrops",
                                                           neutral_backdrop_filter);
    if (!values)
      values = grl_tmdb_request_get_string_list_with_filter (request,
                                                             "$.images.backdrops",
                                                             neutral_backdrop_filter);
    iter = values;
    while (iter != NULL) {
      add_image (closure->self, closure->rs->media,
                 GRL_TMDB_METADATA_KEY_BACKDROP,
                 iter->data);

      iter = iter->next;
    }
    g_list_free_full (values, g_free);
  }

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_KEYWORD)) {
    values = grl_tmdb_request_get_string_list (request,
                                               "$.keywords..name");
    if (!values)
      values = grl_tmdb_request_get_string_list (request,
                                                 "$.keywords.keywords..name");
    iter = values;
    while (iter != NULL) {
      grl_media_add_keyword (closure->rs->media, iter->data);
      iter = iter->next;
    }
    g_list_free_full (values, g_free);
  }

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_PERFORMER)) {
    values = grl_tmdb_request_get_string_list (request, "$.cast..name");
    if (!values)
      values = grl_tmdb_request_get_string_list (request, "$.casts.cast..name");
    iter = values;
    while (iter != NULL) {
      grl_media_add_performer (closure->rs->media, iter->data);
      iter = iter->next;
    }
    g_list_free_full (values, g_free);
  }

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_PRODUCER)) {
    values = grl_tmdb_request_get_string_list_with_filter (request,
                                                           "$.crew[*]",
                                                           producer_filter);
    if (!values)
      values = grl_tmdb_request_get_string_list_with_filter (request,
                                                             "$.casts.crew[*]",
                                                             producer_filter);
    iter = values;
    while (iter != NULL) {
        grl_media_add_producer (closure->rs->media, iter->data);
      iter = iter->next;
    }
    g_list_free_full (values, g_free);
  }

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_DIRECTOR)) {
    values = grl_tmdb_request_get_string_list_with_filter (request,
                                                           "$.crew[*]",
                                                           director_filter);
    if (!values)
      values = grl_tmdb_request_get_string_list_with_filter (request,
                                                             "$.casts.crew[*]",
                                                             director_filter);
    iter = values;
    while (iter != NULL) {
      grl_media_add_director (closure->rs->media, iter->data);
      iter = iter->next;
    }
    g_list_free_full (values, g_free);
  }

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_AUTHOR)) {
    values = grl_tmdb_request_get_string_list_with_filter (request,
                                                           "$.crew[*]",
                                                           writer_filter);
    if (!values)
      values = grl_tmdb_request_get_string_list_with_filter (request,
                                                             "$.casts.crew[*]",
                                                             writer_filter);
    iter = values;
    while (iter != NULL) {
      grl_media_add_author (GRL_MEDIA (closure->rs->media),
                            iter->data);
      iter = iter->next;
    }
    g_list_free_full (values, g_free);
  }

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_REGION) ||
          SHOULD_RESOLVE (GRL_METADATA_KEY_CERTIFICATE) ||
          SHOULD_RESOLVE (GRL_METADATA_KEY_PUBLICATION_DATE)) {
    values = grl_tmdb_request_get_list_with_filter (request,
                                                    "$.countries[*]",
                                                    NULL);
    if (!values)
      values = grl_tmdb_request_get_list_with_filter (request,
                                                      "$.releases.countries[*]",
                                                      NULL);

    for (iter = values; iter != NULL; iter = iter->next) {
      const char *region, *cert, *date;
      GDateTime *pubdate;
      JsonObject *object;

      object = json_node_get_object (iter->data);
      region = json_object_get_string_member (object, "iso_3166_1");
      cert = json_object_get_string_member (object, "certification");
      date = json_object_get_string_member (object, "release_date");
      pubdate = parse_date (date);

      grl_media_add_region_data (closure->rs->media, region, pubdate, cert);

      g_date_time_unref (pubdate);
    }

    g_list_free_full (values, (GDestroyNotify) json_node_free);
  }

out:
  if (error != NULL) {
    g_error_free (error);
  }

  remove_request (closure, request);

  if (g_queue_is_empty (closure->pending_requests)) {
    resolve_closure_callback (closure, NULL);
    resolve_closure_free (closure);
  }
}

static void
on_search_ready (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
  ResolveClosure *closure = (ResolveClosure *) user_data;
  GrlTmdbRequest *request = GRL_TMDB_REQUEST (source);
  GValue *value;
  GError *error = NULL;

  GRL_DEBUG ("Initial search ready...");
  if (!grl_tmdb_request_run_finish (GRL_TMDB_REQUEST (source),
                                    result,
                                    &error)) {
    resolve_closure_callback (closure, error);
    resolve_closure_free (closure);
    g_error_free (error);
    return;
  }

  value = grl_tmdb_request_get (request, "$.total_results");
  if (g_value_get_int64 (value) == 0) {
    /* Nothing found */
    resolve_closure_callback (closure, NULL);
    resolve_closure_free (closure);
    g_value_unset (value);
    g_free (value);
    return;
  }
  g_value_unset (value);
  g_free (value);

  value = grl_tmdb_request_get (request, "$.results[0].id");
  if (value == NULL) {
    /* Cannot continue without id */
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_RESOLVE_FAILED,
                                 _("Remote data does not contain valid identifier"));
    resolve_closure_callback (closure, error);
    resolve_closure_free (closure);
    g_error_free (error);
    return;
  }

  if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_TMDB_ID)) {
    char *tmdb_id = g_strdup_printf ("%" G_GINT64_FORMAT,
                                     g_value_get_int64 (value));
    grl_data_set_string (GRL_DATA (closure->rs->media),
                         GRL_TMDB_METADATA_KEY_TMDB_ID, tmdb_id);
    g_free (tmdb_id);
  }

  closure->id = g_value_get_int64 (value);
  g_value_unset (value);
  g_free (value);

  if (grl_data_get_boolean (GRL_DATA (closure->rs->media), GRL_METADATA_KEY_TITLE_FROM_FILENAME)) {
    value = grl_tmdb_request_get (request, "$.results[0].title");
    if (value) {
      grl_media_set_title (closure->rs->media, g_value_get_string (value));
      grl_data_set_boolean (GRL_DATA (closure->rs->media), GRL_METADATA_KEY_TITLE_FROM_FILENAME, FALSE);
      g_value_unset (value);
      g_free (value);
    }
  }

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_RATING)) {
    value = grl_tmdb_request_get (request, "$.results[0].vote_average");
    if (value != NULL) {
      grl_media_set_rating (closure->rs->media,
                            (float) g_value_get_double (value),
                            10.0f);
      g_value_unset (value);
      g_free (value);
    }
    g_hash_table_remove (closure->keys, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_RATING));
  }

  /* Add thumbnails first, and posters and backdrops later.
   * Posters more likely make a good thumbnail than backdrops.
   */
  if (SHOULD_RESOLVE (GRL_METADATA_KEY_THUMBNAIL)) {
    value = grl_tmdb_request_get (request, "$.results[0].poster_path");
    if (value != NULL) {
        add_image (closure->self, closure->rs->media,
                   GRL_METADATA_KEY_THUMBNAIL,
                   g_value_get_string (value));

        g_value_unset (value);
        g_free (value);
    }
  }

  if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_POSTER)) {
    value = grl_tmdb_request_get (request, "$.results[0].poster_path");
    if (value != NULL) {
        add_image (closure->self, closure->rs->media,
                   GRL_TMDB_METADATA_KEY_POSTER,
                   g_value_get_string (value));

        g_value_unset (value);
        g_free (value);
    }
  }

  if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_BACKDROP)) {
    value = grl_tmdb_request_get (request, "$.results[0].backdrop_path");
    if (value != NULL) {
      add_image (closure->self, closure->rs->media,
                 GRL_TMDB_METADATA_KEY_BACKDROP,
                 g_value_get_string (value));

      g_value_unset (value);
      g_free (value);
    }
  }

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_ORIGINAL_TITLE)) {
    value = grl_tmdb_request_get (request, "$.results[0].original_title");
    if (value != NULL) {
      grl_media_set_original_title (closure->rs->media, g_value_get_string (value));
      g_value_unset (value);
      g_free (value);
    }
    g_hash_table_remove (closure->keys, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ORIGINAL_TITLE));
  }

  remove_request (closure, request);

  /* Check if we need to do additional requests. */
  if (closure->slow) {
    resolve_slow_details (closure);

    if (run_pending_requests (closure, G_MAXINT) > 0)
      return;
  }

  resolve_closure_callback (closure, NULL);
  resolve_closure_free (closure);
}

static void queue_detail_request (ResolveClosure *closure,
                                  GrlTmdbRequestDetail detail)
{
  GrlTmdbRequest *request;

  GRL_DEBUG ("Requesting %s for movie #%" G_GUINT64_FORMAT "...",
             grl_tmdb_request_detail_to_string (detail), closure->id);

  request = grl_tmdb_request_new_details (closure->self->priv->api_key,
                                          detail, closure->id);

  queue_request (closure, request, on_request_ready);
}

static void resolve_slow_details (ResolveClosure *closure)
{
  GList *details = NULL;
  GrlTmdbRequest *request;

  if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_BACKDROP) ||
      SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_POSTER))
    details = g_list_prepend (details, GUINT_TO_POINTER (GRL_TMDB_REQUEST_DETAIL_MOVIE_IMAGES));

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_RATING) ||
      SHOULD_RESOLVE (GRL_METADATA_KEY_ORIGINAL_TITLE) ||
      SHOULD_RESOLVE (GRL_METADATA_KEY_TITLE) ||
      SHOULD_RESOLVE (GRL_METADATA_KEY_GENRE) ||
      SHOULD_RESOLVE (GRL_METADATA_KEY_STUDIO) ||
      SHOULD_RESOLVE (GRL_METADATA_KEY_SITE) ||
      SHOULD_RESOLVE (GRL_METADATA_KEY_DESCRIPTION) ||
      SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_IMDB_ID))
    details = g_list_prepend (details, GUINT_TO_POINTER (GRL_TMDB_REQUEST_DETAIL_MOVIE));

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_KEYWORD))
    details = g_list_prepend (details, GUINT_TO_POINTER (GRL_TMDB_REQUEST_DETAIL_MOVIE_KEYWORDS));

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_PERFORMER) ||
      SHOULD_RESOLVE (GRL_METADATA_KEY_PRODUCER) ||
      SHOULD_RESOLVE (GRL_METADATA_KEY_DIRECTOR) ||
      SHOULD_RESOLVE (GRL_METADATA_KEY_AUTHOR))
    details = g_list_prepend (details, GUINT_TO_POINTER (GRL_TMDB_REQUEST_DETAIL_MOVIE_CAST));

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_REGION) ||
          SHOULD_RESOLVE (GRL_METADATA_KEY_CERTIFICATE) ||
          SHOULD_RESOLVE (GRL_METADATA_KEY_PUBLICATION_DATE))
    details = g_list_prepend (details, GUINT_TO_POINTER (GRL_TMDB_REQUEST_DETAIL_MOVIE_RELEASE_INFO));

  if (details == NULL)
    return;

  if (g_list_length (details) == 1) {
    queue_detail_request (closure, GPOINTER_TO_UINT (details->data));
    return;
  }

  GRL_DEBUG ("Requesting aggregated info for movie #%" G_GUINT64_FORMAT "...",
             closure->id);

  request = grl_tmdb_request_new_details_list (closure->self->priv->api_key,
                                               details, closure->id);
  g_list_free (details);

  queue_request (closure, request, on_request_ready);
}

static void
on_configuration_ready (GObject *source,
                        GAsyncResult *result,
                        gpointer user_data)
{
  ResolveClosure *closure = (ResolveClosure *) user_data;
  GrlTmdbRequest *request = GRL_TMDB_REQUEST (source);
  GrlTmdbSource *self = closure->self;
  GError *error = NULL;
  GValue *value;

  GRL_DEBUG ("Configuration request ready...");

  if (!grl_tmdb_request_run_finish (GRL_TMDB_REQUEST (source),
                                    result,
                                    &error)) {
    resolve_closure_callback (closure, error);
    resolve_closure_free (closure);

    /* Notify pending requests about failure */
    while (!g_queue_is_empty (self->priv->pending_resolves)) {
      ResolveClosure *pending_closure;

      pending_closure = g_queue_pop_head (self->priv->pending_resolves);

      resolve_closure_callback (pending_closure, error);
      resolve_closure_free (pending_closure);
    }

    g_error_free (error);
    return;
  }

  self->priv->configuration = g_object_ref (request);
  remove_request (closure, request);

  value = grl_tmdb_request_get (request, "$.images.base_url");
  if (value != NULL) {
    GRL_DEBUG ("Got TMDb configuration.");
    self->priv->image_base_uri = g_uri_parse (g_value_get_string (value), G_URI_FLAGS_NONE, NULL);
    g_value_unset (value);
    g_free (value);
  }

  g_queue_push_head (self->priv->pending_resolves, closure);

  /* Flush queue. GrlNetWc will take care of throttling */
  while (!g_queue_is_empty (self->priv->pending_resolves)) {
    ResolveClosure *pending_closure;

    pending_closure = g_queue_pop_head (self->priv->pending_resolves);
    run_pending_requests (pending_closure, G_MAXINT);
  }
}

/* ================== API Implementation ================ */

/* GrlSource vfuncs */
static const GList *
grl_tmdb_source_supported_keys (GrlSource *source)
{
  GrlTmdbSource *self = GRL_TMDB_SOURCE (source);
  static GList *supported_keys = NULL;

  if (supported_keys == NULL) {
    const GList *it;

    supported_keys = g_hash_table_get_keys (self->priv->supported_keys);
    it = grl_tmdb_source_slow_keys (source);
    while (it) {
      supported_keys = g_list_prepend (supported_keys, it->data);
      it = it->next;
    }
  }

  return supported_keys;
}

static const GList *
grl_tmdb_source_slow_keys (GrlSource *source)
{
  GrlTmdbSource *self = GRL_TMDB_SOURCE (source);
  static GList *slow_keys = NULL;

  if (slow_keys == NULL) {
    slow_keys = g_hash_table_get_keys (self->priv->slow_keys);
  }

  return slow_keys;
}

static gboolean
grl_tmdb_source_may_resolve (GrlSource *source,
                             GrlMedia *media,
                             GrlKeyID key_id,
                             GList **missing_keys)
{
  GrlTmdbSource *self = GRL_TMDB_SOURCE (source);

  if (!g_hash_table_contains (self->priv->supported_keys,
                             GRLKEYID_TO_POINTER (key_id)) &&
      !g_hash_table_contains (self->priv->slow_keys,
                             GRLKEYID_TO_POINTER (key_id)))
    return FALSE;

  /* We can only entertain videos */
  if (media && !grl_media_is_video (media))
    return FALSE;

  /* Caller wants to check what's needed to resolve */
  if (!media) {
    if (missing_keys)
      *missing_keys = grl_metadata_key_list_new (GRL_METADATA_KEY_TITLE, NULL);

    return FALSE;
  }

  /* We can do nothing without a title or the movie-id */
  if (!grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_TITLE) &&
          !grl_data_has_key (GRL_DATA (media), GRL_TMDB_METADATA_KEY_TMDB_ID)) {
    if (missing_keys)
      *missing_keys = grl_metadata_key_list_new (GRL_METADATA_KEY_TITLE, NULL);

    return FALSE;
  }

  return TRUE;
}

static void
grl_tmdb_source_resolve (GrlSource *source,
                                  GrlSourceResolveSpec *rs)
{
  ResolveClosure *closure;
  GrlTmdbRequest *request;
  const char *title = NULL;
  const char *str_movie_id;
  GrlTmdbSource *self = GRL_TMDB_SOURCE (source);
  guint64 movie_id = 0;
  GList *it;

  if (!grl_media_is_video (rs->media)) {
    /* We only entertain videos */
    rs->callback (source, rs->operation_id, rs->media, rs->user_data, NULL);
    return;
  }

  /* If the media is a TV show, don't handle it */
  if (grl_media_get_show (rs->media) != NULL) {
    rs->callback (source, rs->operation_id, rs->media, rs->user_data, NULL);
    return;
  }

  /* Prefer resolving by movie-id: This is more reliable and saves the search query. */
  str_movie_id = grl_data_get_string (GRL_DATA (rs->media),
                                      GRL_TMDB_METADATA_KEY_TMDB_ID);

  if (str_movie_id)
    movie_id = strtoull (str_movie_id, NULL, 10);

  /* Try title if no movie-id could be found. */
  if (movie_id == 0)
    title = grl_media_get_title (rs->media);

  if (movie_id == 0 && title == NULL) {
    /* Can't search for anything without a title or the movie-id ... */
    rs->callback (source, rs->operation_id, rs->media, rs->user_data, NULL);
    return;
  }

  GRL_DEBUG ("grl_tmdb_source_resolve");

  closure = g_slice_new0 (ResolveClosure);
  closure->self = g_object_ref (self);
  closure->rs = rs;
  closure->pending_requests = g_queue_new ();
  closure->keys = g_hash_table_new (g_direct_hash, g_direct_equal);
  closure->slow = FALSE;
  closure->id = movie_id;

  it = rs->keys;

  /* Copy keys to list for faster lookup */
  while (it) {
    g_hash_table_add (closure->keys, it->data);

    /* Enable slow resolution if slow keys are requested */
    closure->slow |= g_hash_table_contains (self->priv->slow_keys,
                                            it->data);
    it = it->next;
  }

  /* Disable slow resolution if slow keys where requested, but the operation
   * options explicitly ask for fast resolving only. */
  if (grl_operation_options_get_resolution_flags (rs->options) & GRL_RESOLVE_FAST_ONLY)
    closure->slow = FALSE;

  /* We did not receive the config yet, queue request. Config callback will
   * take care of flushing the queue when ready.
   */
  if (self->priv->configuration == NULL && self->priv->config_pending) {
    g_queue_push_tail (self->priv->pending_resolves, closure);
    return;
  }

  if (self->priv->configuration == NULL) {
    GRL_DEBUG ("Fetching TMDb configuration...");
    /* We need to fetch TMDb's configuration for the image paths */
    request = grl_tmdb_request_new_configuration (closure->self->priv->api_key);
    g_assert (g_queue_is_empty (closure->pending_requests));
    queue_request (closure, request, on_configuration_ready);
    self->priv->config_pending = TRUE;
  }

  if (title) {
    GRL_DEBUG ("Running initial search for title \"%s\"...", title);
    request = grl_tmdb_request_new_search (closure->self->priv->api_key, title);
    queue_request (closure, request, on_search_ready);
  } else {
    GRL_DEBUG ("Running %s lookup for movie #%" G_GUINT64_FORMAT "...",
               closure->slow ? "slow" : "fast", movie_id);

    if (closure->slow) {
      resolve_slow_details (closure);
    } else {
      queue_detail_request (closure, GRL_TMDB_REQUEST_DETAIL_MOVIE);
    }
  }

  if (self->priv->config_pending || title == NULL) {
    run_pending_requests (closure, 1);
  } else {
    run_pending_requests (closure, G_MAXINT);
  }
}
