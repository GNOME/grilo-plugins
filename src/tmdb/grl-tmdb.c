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
#include <libsoup/soup-uri.h>
#include <json-glib/json-glib.h>

#include "grl-tmdb.h"
#include "grl-tmdb-request.h"

#define GRL_LOG_DOMAIN_DEFAULT tmdb_log_domain
GRL_LOG_DOMAIN(tmdb_log_domain);

#define PLUGIN_ID   TMDB_PLUGIN_ID

#define SOURCE_ID   "grl-tmdb"
#define SOURCE_NAME "TMDb Metadata Provider"
#define SOURCE_DESC "A source for movie metadata from themoviedb.org"

#define SHOULD_RESOLVE(key) \
    g_hash_table_contains (closure->keys, GINT_TO_POINTER ((key)))

enum {
  PROP_0,
  PROP_API_KEY
};

static GrlKeyID GRL_TMDB_METADATA_KEY_BACKDROPS = GRL_METADATA_KEY_INVALID;
static GrlKeyID GRL_TMDB_METADATA_KEY_POSTERS = GRL_METADATA_KEY_INVALID;
static GrlKeyID GRL_TMDB_METADATA_KEY_IMDB_ID = GRL_METADATA_KEY_INVALID;
static GrlKeyID GRL_TMDB_METADATA_KEY_TMDB_ID = GRL_METADATA_KEY_INVALID;
static GrlKeyID GRL_TMDB_METADATA_KEY_KEYWORDS = GRL_METADATA_KEY_INVALID;
static GrlKeyID GRL_TMDB_METADATA_KEY_PERFORMER = GRL_METADATA_KEY_INVALID;
static GrlKeyID GRL_TMDB_METADATA_KEY_PRODUCER = GRL_METADATA_KEY_INVALID;
static GrlKeyID GRL_TMDB_METADATA_KEY_DIRECTOR = GRL_METADATA_KEY_INVALID;
static GrlKeyID GRL_TMDB_METADATA_KEY_AGE_CERTIFICATES = GRL_METADATA_KEY_INVALID;
static GrlKeyID GRL_TMDB_METADATA_KEY_ORIGINAL_TITLE = GRL_METADATA_KEY_INVALID;

struct _GrlTmdbSourcePrivate {
  char *api_key;
  GHashTable *supported_keys;
  GHashTable *slow_keys;
  GrlNetWc *wc;
  GrlTmdbRequest *configuration;
  gboolean config_pending;
  GQueue *pending_resolves;
  SoupURI *image_base_uri;
};

struct _ResolveClosure {
  GrlTmdbSource *self;
  GrlSourceResolveSpec *rs;
  GQueue *pending_requests;
  guint64 id;
  GHashTable *keys;
  gboolean slow;
};

typedef struct _ResolveClosure ResolveClosure;

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
                       const char *name,
                       const char *nick,
                       const char *blurb);

static void resolve_closure_free (ResolveClosure *closure);
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

  GRL_TMDB_METADATA_KEY_BACKDROPS =
    register_metadata_key (registry,
                           "tmdb-backdrops",
                           "tmdb-backdrops",
                           "A list of URLs for movie backdrops");

  GRL_TMDB_METADATA_KEY_POSTERS =
    register_metadata_key (registry,
                           "tmdb-poster",
                           "tmdb-poster",
                           "A list of URLs for movie posters");

  GRL_TMDB_METADATA_KEY_IMDB_ID =
    register_metadata_key (registry,
                           "tmdb-imdb-id",
                           "tmdb-imdb-id",
                           "ID of this movie at imdb.org");

  GRL_TMDB_METADATA_KEY_TMDB_ID =
    register_metadata_key (registry,
                           "tmdb-id",
                           "tmdb-id",
                           "ID of this movie at tmdb.org");

  GRL_TMDB_METADATA_KEY_KEYWORDS =
    register_metadata_key (registry,
                           "tmdb-keywords",
                           "tmdb-keywords",
                           "A list of keywords about the movie");

  GRL_TMDB_METADATA_KEY_PERFORMER =
    register_metadata_key (registry,
                           "tmdb-performer",
                           "tmdb-performer",
                           "A list of actors taking part in the movie");

  GRL_TMDB_METADATA_KEY_PRODUCER =
    register_metadata_key (registry,
                           "tmdb-producer",
                           "tmdb-producer",
                           "A list of producers of the movie");

  GRL_TMDB_METADATA_KEY_DIRECTOR =
    register_metadata_key (registry,
                           "tmdb-director",
                           "tmdb-director",
                           "Director of the movie");

  GRL_TMDB_METADATA_KEY_AGE_CERTIFICATES =
    register_metadata_key (registry,
                           "tmdb-age-certificates",
                           "tmdb-age-certificates",
                           "Semicolon-separated list of all age certificates");

  GRL_TMDB_METADATA_KEY_ORIGINAL_TITLE =
    register_metadata_key (registry,
                           "tmdb-original-title",
                           "tmdb-original-title",
                           "Original title of a movie");

  GrlTmdbSource *source = grl_tmdb_source_new (api_key);
  grl_registry_register_source (registry,
                                       plugin,
                                       GRL_SOURCE (source),
                                       NULL);
  g_free (api_key);
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_tmdb_source_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== GrlTmdbMetadata GObject ================ */

static GrlTmdbSource *
grl_tmdb_source_new (const char *api_key)
{
  GRL_DEBUG ("grl_tmdb_source_new");
  return g_object_new (GRL_TMDB_SOURCE_TYPE,
                       "source-id", SOURCE_ID,
                       "source-name", SOURCE_NAME,
                       "source-desc", SOURCE_DESC,
                       "api-key", api_key,
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

  g_type_class_add_private (klass, sizeof (GrlTmdbSourcePrivate));

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
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GRL_TMDB_SOURCE_TYPE,
                                            GrlTmdbSourcePrivate);
  self->priv->supported_keys = g_hash_table_new (g_direct_hash, g_direct_equal);
  self->priv->slow_keys = g_hash_table_new (g_direct_hash, g_direct_equal);

  /* Fast keys */
  g_hash_table_add (self->priv->supported_keys,
                    GINT_TO_POINTER (GRL_TMDB_METADATA_KEY_BACKDROPS));
  g_hash_table_add (self->priv->supported_keys,
                    GINT_TO_POINTER (GRL_TMDB_METADATA_KEY_ORIGINAL_TITLE));
  g_hash_table_add (self->priv->supported_keys,
                    GINT_TO_POINTER (GRL_METADATA_KEY_RATING));
  g_hash_table_add (self->priv->supported_keys,
                    GINT_TO_POINTER (GRL_TMDB_METADATA_KEY_POSTERS));
  g_hash_table_add (self->priv->supported_keys,
                    GINT_TO_POINTER (GRL_METADATA_KEY_PUBLICATION_DATE));
  g_hash_table_add (self->priv->supported_keys,
                    GINT_TO_POINTER (GRL_TMDB_METADATA_KEY_TMDB_ID));

  /* Slow keys */
  g_hash_table_add (self->priv->slow_keys,
                    GINT_TO_POINTER (GRL_METADATA_KEY_SITE));
  g_hash_table_add (self->priv->slow_keys,
                    GINT_TO_POINTER (GRL_METADATA_KEY_GENRE));
  g_hash_table_add (self->priv->slow_keys,
                    GINT_TO_POINTER (GRL_METADATA_KEY_STUDIO));
  g_hash_table_add (self->priv->slow_keys,
                    GINT_TO_POINTER (GRL_METADATA_KEY_DESCRIPTION));
  g_hash_table_add (self->priv->slow_keys,
                    GINT_TO_POINTER (GRL_METADATA_KEY_CERTIFICATE));
  g_hash_table_add (self->priv->slow_keys,
                    GINT_TO_POINTER (GRL_TMDB_METADATA_KEY_IMDB_ID));
  g_hash_table_add (self->priv->slow_keys,
                    GINT_TO_POINTER (GRL_TMDB_METADATA_KEY_KEYWORDS));
  g_hash_table_add (self->priv->slow_keys,
                    GINT_TO_POINTER (GRL_TMDB_METADATA_KEY_PERFORMER));
  g_hash_table_add (self->priv->slow_keys,
                    GINT_TO_POINTER (GRL_TMDB_METADATA_KEY_PRODUCER));
  g_hash_table_add (self->priv->slow_keys,
                    GINT_TO_POINTER (GRL_TMDB_METADATA_KEY_DIRECTOR));
  g_hash_table_add (self->priv->slow_keys,
                    GINT_TO_POINTER (GRL_TMDB_METADATA_KEY_AGE_CERTIFICATES));

  self->priv->wc = grl_net_wc_new ();
  grl_net_wc_set_throttling (self->priv->wc, 1);

  self->priv->config_pending = FALSE;
  self->priv->pending_resolves = g_queue_new ();
}

G_DEFINE_TYPE (GrlTmdbSource,
               grl_tmdb_source,
               GRL_TYPE_SOURCE);

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

  if (self->priv->image_base_uri != NULL) {
    soup_uri_free (self->priv->image_base_uri);
    self->priv->image_base_uri = NULL;
  }

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

  key = grl_registry_register_metadata_key (registry, spec, NULL);
  g_param_spec_unref (spec);

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
                          GError *outer_error)
{
  GError *error = NULL;

  if (outer_error && outer_error->domain != GRL_CORE_ERROR) {
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_RESOLVE_FAILED,
                                 outer_error->message);
    g_error_free (outer_error);
  }

  closure->rs->callback (GRL_SOURCE (closure->self),
                         closure->rs->operation_id,
                         closure->rs->media,
                         closure->rs->user_data,
                         error);
  if (error)
    g_error_free (error);
}

static void
resolve_closure_free (ResolveClosure *closure)
{
  g_object_unref (closure->self);
  g_queue_free_full (closure->pending_requests, g_object_unref);
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
us_release_filter (JsonNode *node)
{
  JsonObject *object;
  const char *country;

  if (!JSON_NODE_HOLDS_OBJECT (node)) {
    return NULL;
  }

  object = json_node_get_object (node);
  country = json_object_get_string_member (object, "iso_3166_1");
  if (g_ascii_strcasecmp (country, "US") == 0) {
    return g_strdup (json_object_get_string_member (object, "certification"));
  }

  return NULL;
}

static char *
all_releases_filter (JsonNode *node)
{
  JsonObject *object;
  const char *country, *cert;

  if (!JSON_NODE_HOLDS_OBJECT (node)) {
    return NULL;
  }

  object = json_node_get_object (node);
  country = json_object_get_string_member (object, "iso_3166_1");
  cert = json_object_get_string_member (object, "certification");
  if (cert == NULL || strlen (cert) == 0) {
    /* This is only a release date, no age cert */
    return NULL;
  }
  return g_strconcat (country, ":", cert, NULL);
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

static void on_request_ready (GObject *source,
                              GAsyncResult *result,
                              gpointer user_data) {
  ResolveClosure *closure = (ResolveClosure *) user_data;
  GrlTmdbRequest *request = GRL_TMDB_REQUEST (source);
  GError *error = NULL;
  GList *values, *iter;
  GValue *value;

  GRL_DEBUG ("Detail request ready...");
  if (!grl_tmdb_request_run_finish (GRL_TMDB_REQUEST (source),
                                    result,
                                    &error)) {
    /* Just remove the request and hope the others have some data */
    GRL_WARNING ("Failed to get %s: %s",
                 grl_tmdb_request_get_uri (request),
                 error->message);
    goto out;
  }

  switch (grl_tmdb_request_get_detail (request)) {
    case GRL_TMDB_REQUEST_DETAIL_MOVIE:
    {
      if (SHOULD_RESOLVE (GRL_METADATA_KEY_GENRE)) {
        iter = values = grl_tmdb_request_get_string_list (request, "$.genres..name");
        while (iter != NULL) {
          grl_data_add_string (GRL_DATA (closure->rs->media),
                               GRL_METADATA_KEY_GENRE,
                               (char *) iter->data);
          iter = iter->next;
        }
        g_list_free_full (values, g_free);
      }

      if (SHOULD_RESOLVE (GRL_METADATA_KEY_STUDIO)) {
        iter = values = grl_tmdb_request_get_string_list (request, "$.production_companies..name");
        while (iter != NULL) {
          grl_data_add_string (GRL_DATA (closure->rs->media),
                               GRL_METADATA_KEY_STUDIO,
                               (char *) iter->data);
          iter = iter->next;
        }
        g_list_free_full (values, g_free);
      }

      if (SHOULD_RESOLVE (GRL_METADATA_KEY_SITE)) {
        value = grl_tmdb_request_get (request, "$.homepage");
        if (value != NULL) {
          grl_media_set_site (closure->rs->media, g_value_get_string (value));
          g_value_unset (value);
        }
      }

      if (SHOULD_RESOLVE (GRL_METADATA_KEY_DESCRIPTION)) {
        value = grl_tmdb_request_get (request, "$.overview");
        if (value != NULL) {
          grl_media_set_description (closure->rs->media,
                                     g_value_get_string (value));
          g_value_unset (value);
        }
      }

      if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_IMDB_ID)) {
        value = grl_tmdb_request_get (request, "$.imdb_id");
        if (value != NULL) {
          grl_data_set_string (GRL_DATA (closure->rs->media),
                               GRL_TMDB_METADATA_KEY_IMDB_ID,
                               g_value_get_string (value));
          g_value_unset (value);
        }
      }
    }
    break;
    case GRL_TMDB_REQUEST_DETAIL_MOVIE_IMAGES:
    {
      if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_BACKDROPS)) {
        iter = values = grl_tmdb_request_get_string_list_with_filter (request,
                                                                      "$.backdrops",
                                                                      neutral_backdrop_filter);
        while (iter != NULL) {
          SoupURI *backdrop_uri;
          char *path;

          path = g_strconcat ("original", (char *) iter->data, NULL);

          backdrop_uri = soup_uri_new_with_base (closure->self->priv->image_base_uri,
                                                 path);
          g_free (path);
          path = soup_uri_to_string (backdrop_uri, FALSE);

          grl_data_add_string (GRL_DATA (closure->rs->media),
                               GRL_TMDB_METADATA_KEY_BACKDROPS,
                               path);
          g_free (path);

          iter = iter->next;
        }
        g_list_free_full (values, g_free);
      }

      if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_POSTERS)) {
        iter = values = grl_tmdb_request_get_string_list_with_filter (request,
                                                                      "$.posters",
                                                                      neutral_backdrop_filter);
        while (iter != NULL) {
          SoupURI *backdrop_uri;
          char *path;

          path = g_strconcat ("original", (char *) iter->data, NULL);

          backdrop_uri = soup_uri_new_with_base (closure->self->priv->image_base_uri,
                                                 path);
          g_free (path);
          path = soup_uri_to_string (backdrop_uri, FALSE);

          grl_data_add_string (GRL_DATA (closure->rs->media),
                               GRL_TMDB_METADATA_KEY_POSTERS,
                               path);
          g_free (path);

          iter = iter->next;
        }
        g_list_free_full (values, g_free);
      }
    }
    break;
    case GRL_TMDB_REQUEST_DETAIL_MOVIE_KEYWORDS:
    {
      if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_KEYWORDS)) {
        iter = values = grl_tmdb_request_get_string_list (request,
                                                          "$.keywords..name");
        while (iter != NULL) {
          grl_data_add_string (GRL_DATA (closure->rs->media),
                               GRL_TMDB_METADATA_KEY_KEYWORDS,
                               (char *) iter->data);
          iter = iter->next;
        }
        g_list_free_full (values, g_free);
      }
    }
    break;
    case GRL_TMDB_REQUEST_DETAIL_MOVIE_CAST:
    {
      if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_PERFORMER)) {
        values = grl_tmdb_request_get_string_list (request, "$.cast..name");
        iter = values;
        while (iter != NULL) {
          grl_data_add_string (GRL_DATA (closure->rs->media),
                               GRL_TMDB_METADATA_KEY_PERFORMER,
                               (char *) iter->data);
          iter = iter->next;
        }
        g_list_free_full (values, g_free);
      }

      if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_PRODUCER)) {
        values = grl_tmdb_request_get_string_list_with_filter (request,
                                                               "$.crew[*]",
                                                               producer_filter);
        iter = values;
        while (iter != NULL) {
          grl_data_add_string (GRL_DATA (closure->rs->media),
                               GRL_TMDB_METADATA_KEY_PRODUCER,
                               (char *) iter->data);
          iter = iter->next;
        }
        g_list_free_full (values, g_free);
      }

      if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_DIRECTOR)) {
        values = grl_tmdb_request_get_string_list_with_filter (request,
                                                               "$.crew[*]",
                                                               director_filter);
        iter = values;
        while (iter != NULL) {
          grl_data_add_string (GRL_DATA (closure->rs->media),
                               GRL_TMDB_METADATA_KEY_DIRECTOR,
                               (char *) iter->data);
          iter = iter->next;
        }
        g_list_free_full (values, g_free);
      }
    }
    break;
    case GRL_TMDB_REQUEST_DETAIL_MOVIE_RELEASE_INFO:
    {
      GString *string;

      if (SHOULD_RESOLVE (GRL_METADATA_KEY_CERTIFICATE)) {
        values = grl_tmdb_request_get_string_list_with_filter (request,
                                                               "$.countries[*]",
                                                               us_release_filter);
        if (values != NULL) {
          grl_media_set_certificate (closure->rs->media,
                                     (char *) values->data);
          g_list_free_full (values, g_free);
        }
      }

      if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_AGE_CERTIFICATES)) {
        values = grl_tmdb_request_get_string_list_with_filter (request,
                                                               "$.countries[*]",
                                                               all_releases_filter);
        iter = values;
        string = g_string_new (NULL);
        while (iter != NULL) {
          g_string_append (string, (char *) iter->data);
          iter = iter->next;
          if (iter != NULL) {
            g_string_append_c (string, ';');
          }
        }
        if (string->str != NULL) {
          grl_data_set_string (GRL_DATA (closure->rs->media),
                               GRL_TMDB_METADATA_KEY_AGE_CERTIFICATES,
                               string->str);
        }
        g_string_free (string, TRUE);
      }
    }
    break;
    default:
      break;
  }

out:
  if (error != NULL) {
    g_error_free (error);
  }
  g_queue_remove (closure->pending_requests, request);
  g_object_unref (request);

  if (g_queue_is_empty (closure->pending_requests)) {
    resolve_closure_callback (closure, NULL);
    resolve_closure_free (closure);
  }
}

static GrlTmdbRequest *
create_and_run_request (GrlTmdbSource *self,
                        ResolveClosure *closure,
                        GrlTmdbRequestDetail id)
{
  GrlTmdbRequest *request;

  request = grl_tmdb_request_new_details (self->priv->api_key,
                                          id,
                                          closure->id);
  grl_tmdb_request_run_async (request,
                              self->priv->wc,
                              on_request_ready,
                              NULL,
                              closure);

  return request;
}

static void
on_search_ready (GObject *source,
                 GAsyncResult *result,
                 gpointer user_data)
{
  ResolveClosure *closure = (ResolveClosure *) user_data;
  GrlTmdbRequest *request = GRL_TMDB_REQUEST (source);
  GrlTmdbSource *self = closure->self;
  GValue *value;
  GError *error = NULL;
  char *padded_date;

  GRL_DEBUG ("Initial search ready...");
  if (!grl_tmdb_request_run_finish (GRL_TMDB_REQUEST (source),
                                    result,
                                    &error)) {
    resolve_closure_callback (closure, error);
    resolve_closure_free (closure);
    return;
  }

  value = grl_tmdb_request_get (request, "$.total_results");
  if (g_value_get_int64 (value) == 0) {
    /* Nothing found */
    resolve_closure_callback (closure, NULL);
    resolve_closure_free (closure);
    return;
  }

  value = grl_tmdb_request_get (request, "$.results[0].id");
  if (value == NULL) {
    /* Cannot continue without id */
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_RESOLVE_FAILED,
                                 "Remote data did not contain valid ID");
    resolve_closure_callback (closure, error);
    resolve_closure_free (closure);
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

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_PUBLICATION_DATE)) {
    value = grl_tmdb_request_get (request, "$.results[0].release_date");
    if (value != NULL) {
      GDateTime *pubdate;
      GTimeVal tv;
      padded_date = g_strconcat (g_value_get_string (value), "T00:00:00Z", NULL);
      g_time_val_from_iso8601 (padded_date, &tv);
      pubdate = g_date_time_new_from_timeval_utc (&tv);

      grl_media_set_publication_date (closure->rs->media,
                                      pubdate);
      g_date_time_unref (pubdate);
      g_free (padded_date);
      g_value_unset (value);
    }
  }

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_RATING)) {
    value = grl_tmdb_request_get (request, "$.results[0].vote_average");
    if (value != NULL) {
      grl_media_set_rating (closure->rs->media,
                            (float) g_value_get_double (value),
                            10.0f);
      g_value_unset (value);
    }
  }

  if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_BACKDROPS)) {
    value = grl_tmdb_request_get (request, "$.results[0].backdrop_path");
    if (value != NULL) {
      SoupURI *backdrop_uri;
      char *path;

      path = g_strconcat ("original", g_value_get_string (value), NULL);

      backdrop_uri = soup_uri_new_with_base (closure->self->priv->image_base_uri,
                                             path);
      g_free (path);
      path = soup_uri_to_string (backdrop_uri, FALSE);

      grl_data_add_string (GRL_DATA (closure->rs->media),
                           GRL_TMDB_METADATA_KEY_BACKDROPS,
                           path);
      g_free (path);
      g_value_unset (value);
    }
  }

  if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_POSTERS)) {
    value = grl_tmdb_request_get (request, "$.results[0].poster_path");
    if (value != NULL) {
      SoupURI *backdrop_uri;
      char *path;

      path = g_strconcat ("original", g_value_get_string (value), NULL);

      backdrop_uri = soup_uri_new_with_base (closure->self->priv->image_base_uri,
                                             path);
      g_free (path);
      path = soup_uri_to_string (backdrop_uri, FALSE);

      grl_data_add_string (GRL_DATA (closure->rs->media),
                           GRL_TMDB_METADATA_KEY_POSTERS,
                           path);
      g_free (path);
    }
  }

  if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_ORIGINAL_TITLE)) {
    value = grl_tmdb_request_get (request, "$.results[0].original_title");
    if (value != NULL) {
      grl_data_set_string (GRL_DATA (closure->rs->media),
                           GRL_TMDB_METADATA_KEY_ORIGINAL_TITLE,
                           g_value_get_string (value));
      g_value_unset (value);
    }
  }

  g_queue_pop_head (closure->pending_requests);
  g_object_unref (source);

  /* No need to do additional requests */
  if (!closure->slow) {
    resolve_closure_callback (closure, NULL);
    resolve_closure_free (closure);
    return;
  }

  /* We need to do additional requests. Check if we should have resolved
   * images and try to get some more */
  if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_BACKDROPS) ||
      SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_POSTERS))
    g_queue_push_tail (closure->pending_requests,
                       create_and_run_request (self,
                                               closure,
                                               GRL_TMDB_REQUEST_DETAIL_MOVIE_IMAGES));

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_GENRE) ||
      SHOULD_RESOLVE (GRL_METADATA_KEY_STUDIO) ||
      SHOULD_RESOLVE (GRL_METADATA_KEY_SITE) ||
      SHOULD_RESOLVE (GRL_METADATA_KEY_DESCRIPTION) ||
      SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_IMDB_ID))
    g_queue_push_tail (closure->pending_requests,
                       create_and_run_request (self,
                                               closure,
                                               GRL_TMDB_REQUEST_DETAIL_MOVIE));

  if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_KEYWORDS))
    g_queue_push_tail (closure->pending_requests,
                       create_and_run_request (self,
                                               closure,
                                               GRL_TMDB_REQUEST_DETAIL_MOVIE_KEYWORDS));

  if (SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_PERFORMER) ||
      SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_PRODUCER) ||
      SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_DIRECTOR))
    g_queue_push_tail (closure->pending_requests,
                       create_and_run_request (self,
                                               closure,
                                               GRL_TMDB_REQUEST_DETAIL_MOVIE_CAST));

  if (SHOULD_RESOLVE (GRL_METADATA_KEY_CERTIFICATE) ||
      SHOULD_RESOLVE (GRL_TMDB_METADATA_KEY_AGE_CERTIFICATES))
    g_queue_push_tail (closure->pending_requests,
                       create_and_run_request (self,
                                               closure,
                                               GRL_TMDB_REQUEST_DETAIL_MOVIE_RELEASE_INFO));
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
    return;
  }

  self->priv->configuration = g_queue_pop_head (closure->pending_requests);

  value = grl_tmdb_request_get (request, "$.images.base_url");
  if (value != NULL) {
    GRL_DEBUG ("Got TMDb configuration.");
    self->priv->image_base_uri = soup_uri_new (g_value_get_string (value));
  }

  g_queue_push_head (self->priv->pending_resolves, closure);

  /* Flush queue. GrlNetWc will take care of throttling */
  while (!g_queue_is_empty (self->priv->pending_resolves)) {
    ResolveClosure *pending_closure;

    pending_closure = g_queue_pop_head (self->priv->pending_resolves);
    grl_tmdb_request_run_async (g_queue_peek_head (pending_closure->pending_requests),
                                self->priv->wc,
                                on_search_ready,
                                NULL,
                                pending_closure);
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
                             GINT_TO_POINTER (key_id)) &&
      !g_hash_table_contains (self->priv->slow_keys,
                             GINT_TO_POINTER (key_id)))
    return FALSE;

  /* We can only entertain videos */
  if (media && !GRL_IS_MEDIA_VIDEO (media))
    return FALSE;

  /* Caller wants to check what's needed to resolve */
  if (!media) {
    if (!missing_keys)
      *missing_keys = grl_metadata_key_list_new (GRL_METADATA_KEY_TITLE, NULL);

    return FALSE;
  }

  /* We can do nothing without a title */
  if (!grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_TITLE)) {
    if (!missing_keys)
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
  GAsyncReadyCallback callback;
  const char *title;
  GrlTmdbSource *self = GRL_TMDB_SOURCE (source);
  GList *it;

  if (!GRL_IS_MEDIA_VIDEO (rs->media)) {
    /* We only entertain videos */
    rs->callback (source, rs->operation_id, rs->media, rs->user_data, NULL);
    return;
  }

  title = grl_media_get_title (rs->media);
  if (title == NULL) {
    /* Can't search for anything without a title ... */
    rs->callback (source, rs->operation_id, rs->media, rs->user_data, NULL);
    return;
  }

  GRL_DEBUG ("grl_tmdb_source_resolve");

  closure = g_slice_new0 (ResolveClosure);
  closure->self = g_object_ref (source);
  closure->rs = rs;
  closure->pending_requests = g_queue_new ();
  closure->keys = g_hash_table_new (g_direct_hash, g_direct_equal);
  closure->slow = FALSE;
  request = grl_tmdb_request_new_search (closure->self->priv->api_key, title);
  g_queue_push_tail (closure->pending_requests, request);
  it = rs->keys;

  /* Copy keys to list for faster lookup */
  while (it) {
    g_hash_table_add (closure->keys, it->data);
    closure->slow |= g_hash_table_contains (self->priv->slow_keys,
                                            it->data);
    it = it->next;
  }

  if (grl_operation_options_get_flags (rs->options) & GRL_RESOLVE_FAST_ONLY)
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
    g_queue_push_head (closure->pending_requests, request);
    callback = on_configuration_ready;
    self->priv->config_pending = TRUE;
  } else {
    GRL_DEBUG ("Running initial search...");
    callback = on_search_ready;
  }

  grl_tmdb_request_run_async (g_queue_peek_head (closure->pending_requests),
                              closure->self->priv->wc,
                              callback,
                              NULL,
                              closure);
}
