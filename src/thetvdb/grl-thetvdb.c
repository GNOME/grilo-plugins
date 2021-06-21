/*
 * Copyright (C) 2014 Victor Toso.
 *
 * Contact: Victor Toso <me@victortoso.com>
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
#include <net/grl-net.h>
#include <libxml/xmlreader.h>
#include <archive.h>
#include <archive_entry.h>

#include "grl-thetvdb.h"
#include "thetvdb-resources.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT thetvdb_log_domain
GRL_LOG_DOMAIN_STATIC (thetvdb_log_domain);

/* --- URLs --- */

#define THETVDB_BASE        "https://thetvdb.com/"
#define THETVDB_BASE_API    THETVDB_BASE "api/"
#define THETVDB_BASE_IMG    THETVDB_BASE "banners/%s"

#define THETVDB_GET_SERIES    THETVDB_BASE_API "GetSeries.php?language=all&seriesname=%s"
#define THETVDB_GET_EPISODES  THETVDB_BASE_API "%s/series/%s/all/%s.zip"

/* --- Files and Path to Images --- */
#define THETVDB_ALL_DATA_XML    "%s.xml"
#define THETVDB_URL_TO_SCREEN   THETVDB_BASE    "banners/%s"

/* --- Helpers --- */
#define THETVDB_STR_DELIMITER   "|"
#define THETVDB_DEFAULT_LANG    "en"
#define GRL_SQL_DB              "grl-thetvdb.db"
#define GOM_DB_VERSION          3

/* --- XML Fields --- */
#define THETVDB_ID              BAD_CAST "id"
#define THETVDB_SERIES_ID       BAD_CAST "SeriesID"
#define THETVDB_GENRE           BAD_CAST "Genre"
#define THETVDB_ACTORS          BAD_CAST "Actors"
#define THETVDB_LANGUAGE        BAD_CAST "Language"
#define THETVDB_STATUS          BAD_CAST "Status"
#define THETVDB_SERIES_NAME     BAD_CAST "SeriesName"
#define THETVDB_OVERVIEW        BAD_CAST "Overview"
#define THETVDB_RATING          BAD_CAST "Rating"
#define THETVDB_FIRST_AIRED     BAD_CAST "FirstAired"
#define THETVDB_IMDB_ID         BAD_CAST "IMDB_ID"
#define THETVDB_ZAP2IT_ID       BAD_CAST "zap2it_id"
#define THETVDB_BANNER          BAD_CAST "banner"
#define THETVDB_POSTER          BAD_CAST "poster"
#define THETVDB_FANART          BAD_CAST "fanart"

#define THETVDB_SEASON_NUMBER   BAD_CAST "SeasonNumber"
#define THETVDB_EPISODE_NUMBER  BAD_CAST "EpisodeNumber"
#define THETVDB_ABSOLUTE_NUMBER BAD_CAST "absolute_number"
#define THETVDB_EPISODE_NAME    BAD_CAST "EpisodeName"
#define THETVDB_DIRECTOR        BAD_CAST "Director"
#define THETVDB_GUEST_STARS     BAD_CAST "GuestStars"
#define THETVDB_FILENAME        BAD_CAST "filename"
#define THETVDB_SEASON_ID       BAD_CAST "seasonid"
#define THETVDB_SERIE_ID        BAD_CAST "seriesid"

/* --- Plugin information --- */

#define SOURCE_ID     "grl-thetvdb"
#define SOURCE_NAME   "TheTVDB"
#define SOURCE_DESC   _("A source for fetching metadata of television shows")

/* --- TheTVDB keys  --- */
static GrlKeyID GRL_THETVDB_METADATA_KEY_THETVDB_ID   = GRL_METADATA_KEY_INVALID;
static GrlKeyID GRL_THETVDB_METADATA_KEY_IMDB_ID      = GRL_METADATA_KEY_INVALID;
static GrlKeyID GRL_THETVDB_METADATA_KEY_ZAP2IT_ID    = GRL_METADATA_KEY_INVALID;
static GrlKeyID GRL_THETVDB_METADATA_KEY_GUEST_STARS  = GRL_METADATA_KEY_INVALID;
static GrlKeyID GRL_THETVDB_METADATA_KEY_FANART       = GRL_METADATA_KEY_INVALID;
static GrlKeyID GRL_THETVDB_METADATA_KEY_BANNER       = GRL_METADATA_KEY_INVALID;
static GrlKeyID GRL_THETVDB_METADATA_KEY_POSTER       = GRL_METADATA_KEY_INVALID;
static GrlKeyID GRL_THETVDB_METADATA_KEY_EPISODE_SS   = GRL_METADATA_KEY_INVALID;

struct _GrlTheTVDBPrivate {
  gchar         *api_key;
  GList         *supported_keys;
  GomAdapter    *adapter;
  GomRepository *repository;

  /* Hash table with a string as key (the show name) and a list as its value
   * (a list of OperationSpec *) */
  GHashTable    *ht_wait_list;
};

typedef struct _OperationSpec {
  GrlSource      *source;
  guint           operation_id;
  GList          *keys;
  GrlMedia       *media;
  gpointer        user_data;
  guint           error_code;
  gchar          *lang;
  gboolean        fetched_web;
  gboolean        cache_only;
  SeriesResource *serie_resource;
  GrlSourceResolveCb callback;
} OperationSpec;

/* All supported languages from
 * thetvdb.com/wiki/index.php?title=Multi_Language#Available_Languages */
static struct {
  gint         lid;
  const gchar *name;
} supported_languages[] = {
  { 7, "en" },
  { 8, "sv" },
  { 9, "no" },
  { 10, "da" },
  { 11, "fi" },
  { 13, "nl" },
  { 14, "de" },
  { 15, "it" },
  { 16, "es" },
  { 17, "fr" },
  { 18, "pl" },
  { 19, "hu" },
  { 20, "el" },
  { 21, "tr" },
  { 22, "ru" },
  { 24, "he" },
  { 25, "ja" },
  { 26, "pt" },
  { 27, "zh" },
  { 28, "cs" },
  { 30, "sl" },
  { 31, "hr" },
  { 32, "ko" },
};

static GrlTheTVDBSource *grl_thetvdb_source_new (const gchar *api_key);

static void grl_thetvdb_source_finalize (GObject *object);

static void grl_thetvdb_source_resolve (GrlSource *source,
                                        GrlSourceResolveSpec *rs);

static gboolean grl_thetvdb_source_may_resolve (GrlSource *source,
                                                GrlMedia *media,
                                                GrlKeyID key_id,
                                                GList **missing_keys);

static const GList *grl_thetvdb_source_supported_keys (GrlSource *source);

static void thetvdb_migrate_db_done (GObject *object,
                                     GAsyncResult *result,
                                     gpointer user_data);

static void cache_find_episode (OperationSpec *os);

/* ================== TheTVDB Plugin  ================= */

static gboolean
grl_thetvdb_plugin_init (GrlRegistry *registry,
                         GrlPlugin *plugin,
                         GList *configs)
{
  GrlTheTVDBSource *source;
  GrlConfig *config;
  char *api_key = NULL;

  GRL_LOG_DOMAIN_INIT (thetvdb_log_domain, "thetvdb");

  GRL_DEBUG ("thetvdb_plugin_init");

  if (configs) {
    config = GRL_CONFIG (configs->data);
    api_key = grl_config_get_api_key (config);
  }

  if (api_key == NULL) {
    GRL_INFO ("Cannot load plugin: missing API Key");
    return FALSE;
  }

  source = grl_thetvdb_source_new (api_key);
  grl_registry_register_source (registry,
                                plugin,
                                GRL_SOURCE (source),
                                NULL);
  g_free (api_key);
  return TRUE;
}

static void
grl_thetvdb_plugin_register_keys (GrlRegistry *registry,
                                  GrlPlugin   *plugin)
{
  GParamSpec *spec;

  spec = g_param_spec_string ("thetvdb-id",
                              "thetvdb-id",
                              "TV Show or episode id for The TVDB source.",
                              NULL,
                              G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE),
  GRL_THETVDB_METADATA_KEY_THETVDB_ID =
    grl_registry_register_metadata_key (registry, spec, GRL_METADATA_KEY_INVALID, NULL);

  spec = g_param_spec_string ("thetvdb-imdb-id",
                              "thetvdb-imdb-id",
                              "TV Show or episode id for IMDB source.",
                              NULL,
                              G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE),
  GRL_THETVDB_METADATA_KEY_IMDB_ID =
    grl_registry_register_metadata_key (registry, spec, GRL_METADATA_KEY_INVALID, NULL);

  spec = g_param_spec_string ("thetvdb-zap2it-id",
                              "thetvdb-zap2it-id",
                              "TV Show or episode id for Zap2it source.",
                              NULL,
                              G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE),
  GRL_THETVDB_METADATA_KEY_ZAP2IT_ID =
    grl_registry_register_metadata_key (registry, spec, GRL_METADATA_KEY_INVALID, NULL);

  spec = g_param_spec_string ("thetvdb-guest-stars",
                              "thetvdb-guest-stars",
                              "Guest stars performing in the episode.",
                              NULL,
                              G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE),
  GRL_THETVDB_METADATA_KEY_GUEST_STARS =
    grl_registry_register_metadata_key (registry, spec, GRL_METADATA_KEY_INVALID, NULL);

  spec = g_param_spec_string ("thetvdb-fanart",
                              "thetvdb-fanart",
                              "The mosted voted fanart of the TV Show.",
                              NULL,
                              G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE),
  GRL_THETVDB_METADATA_KEY_FANART =
    grl_registry_register_metadata_key (registry, spec, GRL_METADATA_KEY_INVALID, NULL);

  spec = g_param_spec_string ("thetvdb-banner",
                              "thetvdb-banner",
                              "The most voted banner of the TV Show.",
                              NULL,
                              G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE),
  GRL_THETVDB_METADATA_KEY_BANNER =
    grl_registry_register_metadata_key (registry, spec, GRL_METADATA_KEY_INVALID, NULL);

  spec = g_param_spec_string ("thetvdb-poster",
                              "thetvdb-poster",
                              "The most voted poster of the TV Show.",
                              NULL,
                              G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE),
  GRL_THETVDB_METADATA_KEY_POSTER =
    grl_registry_register_metadata_key (registry, spec, GRL_METADATA_KEY_INVALID, NULL);

  spec = g_param_spec_string ("thetvdb-episode-screenshot",
                              "thetvdb-episode-screenshot",
                              "One screenshot of the episode.",
                              NULL,
                              G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE),
  GRL_THETVDB_METADATA_KEY_EPISODE_SS =
    grl_registry_register_metadata_key (registry, spec, GRL_METADATA_KEY_INVALID, NULL);
}

GRL_PLUGIN_DEFINE (GRL_MAJOR,
                   GRL_MINOR,
                   THETVDB_PLUGIN_ID,
                   "The TVDB",
                   "A plugin for fetching metadata of television shows",
                   "Victor Toso",
                   VERSION,
                   "LGPL-2.1-or-later",
                   "http://victortoso.com",
                   grl_thetvdb_plugin_init,
                   NULL,
                   grl_thetvdb_plugin_register_keys);

/* ================== TheTVDB GObject ================= */

G_DEFINE_TYPE_WITH_PRIVATE (GrlTheTVDBSource, grl_thetvdb_source, GRL_TYPE_SOURCE)

static GrlTheTVDBSource *
grl_thetvdb_source_new (const gchar *api_key)
{
  GObject *object;
  GrlTheTVDBSource *source;
  const char *tags[] = {
    "tv",
    NULL
  };

  GRL_DEBUG ("thetvdb_source_new");

  object = g_object_new (GRL_THETVDB_SOURCE_TYPE,
                         "source-id", SOURCE_ID,
                         "source-name", SOURCE_NAME,
                         "source-desc", SOURCE_DESC,
                         "supported-media", GRL_SUPPORTED_MEDIA_VIDEO,
                         "source-tags", tags,
                         NULL);

  source = GRL_THETVDB_SOURCE (object);
  source->priv->api_key = g_strdup (api_key);
  return source;
}

static void
grl_thetvdb_source_class_init (GrlTheTVDBSourceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

  source_class->supported_keys = grl_thetvdb_source_supported_keys;
  source_class->may_resolve = grl_thetvdb_source_may_resolve;
  source_class->resolve = grl_thetvdb_source_resolve;
  gobject_class->finalize = grl_thetvdb_source_finalize;
}

static void
grl_thetvdb_source_init (GrlTheTVDBSource *source)
{
  gchar *path;
  gchar *db_path;
  GList *tables;
  GError *error = NULL;

  GRL_DEBUG ("thetvdb_source_init");

  source->priv = grl_thetvdb_source_get_instance_private (source);

  /* All supported keys in a GList */
  source->priv->supported_keys =
    grl_metadata_key_list_new (GRL_METADATA_KEY_SHOW,
                               GRL_METADATA_KEY_SEASON,
                               GRL_METADATA_KEY_EPISODE,
                               GRL_METADATA_KEY_GENRE,
                               GRL_METADATA_KEY_PERFORMER,
                               GRL_METADATA_KEY_DIRECTOR,
                               GRL_METADATA_KEY_PUBLICATION_DATE,
                               GRL_METADATA_KEY_DESCRIPTION,
                               GRL_METADATA_KEY_EPISODE_TITLE,
                               GRL_THETVDB_METADATA_KEY_THETVDB_ID,
                               GRL_THETVDB_METADATA_KEY_IMDB_ID,
                               GRL_THETVDB_METADATA_KEY_ZAP2IT_ID,
                               GRL_THETVDB_METADATA_KEY_GUEST_STARS,
                               GRL_THETVDB_METADATA_KEY_FANART,
                               GRL_THETVDB_METADATA_KEY_BANNER,
                               GRL_THETVDB_METADATA_KEY_POSTER,
                               GRL_THETVDB_METADATA_KEY_EPISODE_SS,
                               GRL_METADATA_KEY_INVALID);

  /* Get database connection */
  path = g_build_filename (g_get_user_data_dir (), "grilo-plugins", NULL);
  if (!g_file_test (path, G_FILE_TEST_IS_DIR))
    g_mkdir_with_parents (path, 0775);

  GRL_DEBUG ("Opening database connection...");
  db_path = g_build_filename (path, GRL_SQL_DB, NULL);
  g_free (path);

  source->priv->adapter = gom_adapter_new ();
  if (!gom_adapter_open_sync (source->priv->adapter, db_path, &error)) {
    GrlRegistry *registry;
    GRL_WARNING ("Could not open database '%s': %s", db_path, error->message);
    g_error_free (error);
    g_free (db_path);

    /* Removes itself from list of sources */
    registry = grl_registry_get_default ();
    grl_registry_unregister_source (registry,
                                    GRL_SOURCE (source),
                                    NULL);
    return;
  }
  g_free (db_path);

  /* Creates our pending queue */
  source->priv->ht_wait_list = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                      g_free, NULL);

  source->priv->repository = gom_repository_new (source->priv->adapter);
  tables = g_list_prepend (NULL, GINT_TO_POINTER (SERIES_TYPE_RESOURCE));
  tables = g_list_prepend (tables, GINT_TO_POINTER (EPISODE_TYPE_RESOURCE));
  tables = g_list_prepend (tables, GINT_TO_POINTER (FUZZY_SERIES_NAMES_TYPE_RESOURCE));
  gom_repository_automatic_migrate_async (source->priv->repository, GOM_DB_VERSION,
                                          tables, thetvdb_migrate_db_done, source);
}

static void
grl_thetvdb_source_finalize (GObject *object)
{
  GrlTheTVDBSource *source;

  GRL_DEBUG ("grl_thetvdb_source_finalize");

  source = GRL_THETVDB_SOURCE (object);

  g_list_free (source->priv->supported_keys);
  g_hash_table_destroy (source->priv->ht_wait_list);
  g_clear_object (&source->priv->repository);
  g_clear_pointer (&source->priv->api_key, g_free);

  if (source->priv->adapter) {
    gom_adapter_close_sync (source->priv->adapter, NULL);
    g_clear_object (&source->priv->adapter);
  }

  G_OBJECT_CLASS (grl_thetvdb_source_parent_class)->finalize (object);
}

/* ======================= Utilities ==================== */
static void
thetvdb_migrate_db_done (GObject      *object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  gboolean ret;
  GError *error = NULL;
  GrlRegistry *registry;

  ret = gom_repository_migrate_finish (GOM_REPOSITORY (object), result, &error);
  if (ret == FALSE) {
    GRL_WARNING ("Failed to migrate database: %s", error->message);
    g_error_free (error);

    /* Removes itself from list of sources */
    registry = grl_registry_get_default ();
    grl_registry_unregister_source (registry,
                                    GRL_SOURCE (user_data),
                                    NULL);
  }
}

static void
free_operation_spec (gpointer os_pointer)
{
  OperationSpec *os = (OperationSpec *) os_pointer;
  g_free (os->lang);
  g_list_free (os->keys);
  g_clear_object (&os->serie_resource);
  g_slice_free (OperationSpec, os);
}

static gchar *
get_pref_language (GrlTheTVDBSource *tvdb_source)
{
  const gchar * const *strv;
  gint strv_len, langv_len;
  gint i, j;

  strv = g_get_language_names ();
  strv_len = g_strv_length ((gchar **) strv);
  langv_len = G_N_ELEMENTS (supported_languages);
  for (i = 0; i < strv_len; i++) {
    /* We are only interested in two letter language */
    if (strlen (strv[i]) != 2)
      continue;

    for (j = 0; j < langv_len; j++) {
      if (g_strcmp0 (supported_languages[j].name, strv[i]) == 0)
        return g_strdup (strv[i]);
    }
  }

  return g_strdup (THETVDB_DEFAULT_LANG);
}

static EpisodeResource *
xml_parse_and_save_episodes (GomRepository *repository,
                             xmlDocPtr doc,
                             const gchar *title,
                             gint season_number,
                             gint episode_number)
{
  xmlNodePtr node, child;
  xmlChar *node_data = NULL;
  EpisodeResource *episode_resource= NULL;

  /* Get ptr to <data> */
  node = xmlDocGetRootElement (doc);
  /* Get ptr to <Serie> */
  node = node->xmlChildrenNode;
  /* Iterate in the <Episode> nodes */
  for (node = node->next; node != NULL; node = node->next) {
    GError *error = NULL;
    gint tmp_season_number = -1;
    gint tmp_episode_number = -1;
    gchar *tmp_title = NULL;
    gboolean episode_found = FALSE;
    EpisodeResource *eres;

    eres = g_object_new (EPISODE_TYPE_RESOURCE,
                         "repository", repository,
                         NULL);

    for (child = node->xmlChildrenNode; child != NULL; child = child->next) {
      node_data = xmlNodeListGetString (doc, child->xmlChildrenNode, 1);
      if (node_data == NULL)
        continue;

      if (xmlStrcasecmp (child->name, THETVDB_LANGUAGE) == 0) {
        g_object_set (G_OBJECT (eres), EPISODE_COLUMN_LANGUAGE,
                      (gchar *) node_data, NULL);

      } else if (xmlStrcmp (child->name, THETVDB_OVERVIEW) == 0) {
        g_object_set (G_OBJECT (eres), EPISODE_COLUMN_OVERVIEW,
                      (gchar *) node_data, NULL);

      } else if (xmlStrcmp (child->name, THETVDB_FIRST_AIRED) == 0) {
        g_object_set (G_OBJECT (eres), EPISODE_COLUMN_FIRST_AIRED,
                      (gchar *) node_data, NULL);

      } else if (xmlStrcmp (child->name, THETVDB_IMDB_ID) == 0) {
        g_object_set (G_OBJECT (eres), EPISODE_COLUMN_IMDB_ID,
                      (gchar *) node_data, NULL);

      } else if (xmlStrcmp (child->name, THETVDB_SEASON_ID) == 0) {
        g_object_set (G_OBJECT (eres), EPISODE_COLUMN_SEASON_ID,
                      (gchar *) node_data, NULL);

      } else if (xmlStrcmp (child->name, THETVDB_SERIE_ID) == 0) {
        g_object_set (G_OBJECT (eres), EPISODE_COLUMN_SERIES_ID,
                      (gchar *) node_data, NULL);

      } else if (xmlStrcmp (child->name, THETVDB_DIRECTOR) == 0) {
        g_object_set (G_OBJECT (eres), EPISODE_COLUMN_DIRECTOR_NAMES,
                      (gchar *) node_data, NULL);

      } else if (xmlStrcmp (child->name, THETVDB_GUEST_STARS) == 0) {
        g_object_set (G_OBJECT (eres), EPISODE_COLUMN_GUEST_STARS_NAMES,
                      (gchar *) node_data, NULL);

      } else if (xmlStrcmp (child->name, THETVDB_ID) == 0) {
        g_object_set (G_OBJECT (eres), EPISODE_COLUMN_EPISODE_ID,
                      (gchar *) node_data, NULL);

      } else if (xmlStrcmp (child->name, THETVDB_FILENAME) == 0) {
        gchar *str = g_strdup_printf (THETVDB_BASE_IMG, (gchar *) node_data);
        g_object_set (G_OBJECT (eres), EPISODE_COLUMN_URL_EPISODE_SCREEN,
                      str, NULL);
        g_free (str);

      } else if (xmlStrcmp (child->name, THETVDB_EPISODE_NAME) == 0) {
        g_object_set (G_OBJECT (eres), EPISODE_COLUMN_EPISODE_NAME,
                      (gchar *) node_data, NULL);
        tmp_title = g_strdup ((gchar *) node_data);

      } else if (xmlStrcmp (child->name, THETVDB_RATING) == 0) {
        gdouble num_double = g_ascii_strtod ((gchar *) node_data, NULL);
        g_object_set (G_OBJECT (eres), EPISODE_COLUMN_RATING, num_double, NULL);

      } else if (xmlStrcmp (child->name, THETVDB_SEASON_NUMBER) == 0) {
        gint num = g_ascii_strtoull ((gchar *) node_data, NULL, 10);
        g_object_set (G_OBJECT (eres), EPISODE_COLUMN_SEASON_NUMBER, num, NULL);
        tmp_season_number = num;

      } else if (xmlStrcmp (child->name, THETVDB_ABSOLUTE_NUMBER) == 0) {
        gint num = g_ascii_strtoull ((gchar *) node_data, NULL, 10);
        g_object_set (G_OBJECT (eres), EPISODE_COLUMN_ABSOLUTE_NUMBER,
                      num, NULL);

      } else if (xmlStrcmp (child->name, THETVDB_EPISODE_NUMBER) == 0) {
        gint num = g_ascii_strtoull ((gchar *) node_data, NULL, 10);
        g_object_set (G_OBJECT (eres), EPISODE_COLUMN_EPISODE_NUMBER,
                      num, NULL);
        tmp_episode_number = num;
      }

      if (node_data != NULL) {
        xmlFree (node_data);
        node_data = NULL;
      }
    }

    /* Check if the episode is the same we want */
    if (tmp_season_number == season_number
        && tmp_episode_number == episode_number) {
      episode_found = TRUE;
      episode_resource = eres;
    }

    if (title != NULL && tmp_title != NULL
        && g_ascii_strcasecmp (title, tmp_title) == 0) {
      episode_found = TRUE;
      episode_resource = eres;
    }

    gom_resource_save_sync (GOM_RESOURCE (eres), &error);
    if (error != NULL) {
      GRL_DEBUG ("Failed to store episode '%s' due %s",
                  tmp_title, error->message);
      g_error_free (error);
    }

    g_clear_pointer (&tmp_title, g_free);
    if (!episode_found)
      g_object_unref (eres);
  }

  return episode_resource;
}

static gchar *
xml_parse_get_series_id (xmlDocPtr doc)
{
  xmlNodePtr node;
  xmlChar *node_data = NULL;
  gchar *series_id = NULL;

  /* Get ptr to <data> */
  node = xmlDocGetRootElement (doc);
  /* Get ptr to <Serie> */
  node = node->xmlChildrenNode;
  /* Iterate in the <Serie> */
  for (node = node->xmlChildrenNode; node != NULL; node = node->next) {
    node_data = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    if (node_data == NULL)
      continue;

    if (xmlStrcmp (node->name, THETVDB_ID) == 0) {
        series_id = g_strdup ((gchar *) node_data);
        xmlFree (node_data);
        break;
    }
    xmlFree (node_data);
  }
  return series_id;
}

static void
cache_save_fuzzy_series_names (GomRepository *repository,
                               const gchar *fuzzy_name,
                               const gchar *series_id)
{
  GError *error = NULL;
  FuzzySeriesNamesResource *fsres =
    g_object_new (FUZZY_SERIES_NAMES_TYPE_RESOURCE,
                  "repository", repository,
                  FUZZY_SERIES_NAMES_COLUMN_FUZZY_NAME, fuzzy_name,
                  FUZZY_SERIES_NAMES_COLUMN_SERIES_ID, series_id,
                  NULL);
  gom_resource_save_sync (GOM_RESOURCE (fsres), &error);
  if (error != NULL) {
    GRL_DEBUG ("Failed to store fuzzy series name '%s' due %s",
               fuzzy_name, error->message);
    g_error_free (error);
  }
  g_object_unref (fsres);
}

static SeriesResource *
xml_parse_and_save_serie (GomRepository *repository,
                          xmlDocPtr doc,
                          const gchar *requested_show)
{
  xmlNodePtr node;
  xmlChar *node_data = NULL;
  SeriesResource *sres;
  GError *error = NULL;
  gchar *show = NULL;
  gchar *series_id = NULL;

  sres = g_object_new (SERIES_TYPE_RESOURCE,
                       "repository", repository,
                       NULL);

  /* Get ptr to <data> */
  node = xmlDocGetRootElement (doc);
  /* Get ptr to <Serie> */
  node = node->xmlChildrenNode;
  /* Iterate in the <Serie> */
  for (node = node->xmlChildrenNode; node != NULL; node = node->next) {
    node_data = xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    if (node_data == NULL)
      continue;

    if (xmlStrcasecmp (node->name, THETVDB_LANGUAGE) == 0) {
      g_object_set (G_OBJECT (sres), SERIES_COLUMN_LANGUAGE,
                    (gchar *) node_data, NULL);

    } else if (xmlStrcmp (node->name, THETVDB_STATUS) == 0) {
      g_object_set (G_OBJECT (sres), SERIES_COLUMN_STATUS,
                    (gchar *) node_data, NULL);

    } else if (xmlStrcmp (node->name, THETVDB_SERIES_NAME) == 0) {
      g_object_set (G_OBJECT (sres), SERIES_COLUMN_SERIES_NAME,
                    (gchar *) node_data, NULL);
      show = g_strdup ((gchar *) node_data);

    } else if (xmlStrcmp (node->name, THETVDB_OVERVIEW) == 0) {
      g_object_set (G_OBJECT (sres), SERIES_COLUMN_OVERVIEW,
                    (gchar *) node_data, NULL);

    } else if (xmlStrcmp (node->name, THETVDB_FIRST_AIRED) == 0) {
      g_object_set (G_OBJECT (sres), SERIES_COLUMN_FIRST_AIRED,
                    (gchar *) node_data, NULL);

    } else if (xmlStrcmp (node->name, THETVDB_IMDB_ID) == 0) {
      g_object_set (G_OBJECT (sres), SERIES_COLUMN_IMDB_ID,
                    (gchar *) node_data, NULL);

    } else if (xmlStrcmp (node->name, THETVDB_ZAP2IT_ID) == 0) {
      g_object_set (G_OBJECT (sres), SERIES_COLUMN_ZAP2IT_ID,
                    (gchar *) node_data, NULL);

    } else if (xmlStrcmp (node->name, THETVDB_ACTORS) == 0) {
      g_object_set (G_OBJECT (sres), SERIES_COLUMN_ACTOR_NAMES,
                    (gchar *) node_data, NULL);

    } else if (xmlStrcmp (node->name, THETVDB_GENRE) == 0) {
      g_object_set (G_OBJECT (sres), SERIES_COLUMN_GENRES,
                    (gchar *) node_data, NULL);

    } else if (xmlStrcmp (node->name, THETVDB_ID) == 0) {
      g_object_set (G_OBJECT (sres), SERIES_COLUMN_SERIES_ID,
                    (gchar *) node_data, NULL);
      series_id = g_strdup ((gchar *) node_data);

    } else if (xmlStrcmp (node->name, THETVDB_BANNER) == 0) {
      gchar *str = g_strdup_printf (THETVDB_BASE_IMG, (gchar *) node_data);
      g_object_set (G_OBJECT (sres), SERIES_COLUMN_URL_BANNER, str, NULL);
      g_free (str);

    } else if (xmlStrcmp (node->name, THETVDB_POSTER) == 0) {
      gchar *str = g_strdup_printf (THETVDB_BASE_IMG, (gchar *) node_data);
      g_object_set (G_OBJECT (sres), SERIES_COLUMN_URL_POSTER, str, NULL);
      g_free (str);

    } else if (xmlStrcmp (node->name, THETVDB_FANART) == 0) {
      gchar *str = g_strdup_printf (THETVDB_BASE_IMG, (gchar *) node_data);
      g_object_set (G_OBJECT (sres), SERIES_COLUMN_URL_FANART, str, NULL);
      g_free (str);

    } else if (xmlStrcmp (node->name, THETVDB_RATING) == 0) {
      gdouble num_double = g_ascii_strtod ((gchar *) node_data, NULL);
      g_object_set (G_OBJECT (sres), SERIES_COLUMN_RATING, num_double, NULL);
    }

    if (node_data != NULL) {
      xmlFree (node_data);
      node_data = NULL;
    }
  }

  gom_resource_save_sync (GOM_RESOURCE (sres), &error);
  if (error != NULL) {
    GRL_DEBUG ("Failed to store series '%s' due %s",
                show, error->message);
    g_error_free (error);

  } else if (series_id != NULL) {
    /* This is a new series to our db. Keep it on fuzzy naming db as well */
      cache_save_fuzzy_series_names (repository, show, series_id);
  }

  if (series_id != NULL && requested_show != NULL &&
      g_strcmp0 (show, requested_show) != 0) {
    /* Always save the user's requested show to our fuzzy naming db */
    cache_save_fuzzy_series_names (repository, requested_show, series_id);
  }

  g_clear_pointer (&show, g_free);
  g_clear_pointer (&series_id, g_free);
  return sres;
}

static void
thetvdb_add_data_string_unique (GrlData *data,
                                GrlKeyID key_id,
                                gchar **strv)
{
  guint i, j;
  guint data_len;
  GrlRelatedKeys *related_keys;

  for (i = 0; strv[i] != NULL; i++) {
    gboolean insert = TRUE;

    /* Do not insert empty strings */
    if (strv[i] && *strv[i] == '\0')
      continue;

    /* Check if this value already exists to this key */
    data_len = grl_data_length (data, key_id);
    for (j = 0; insert && j < data_len; j++) {
      const gchar *element;

      related_keys = grl_data_get_related_keys (data, key_id, j);
      element = grl_related_keys_get_string (related_keys, key_id);
      if (g_strcmp0 (element, strv[i]) == 0)
        insert = FALSE;
    }

    if (insert)
      grl_data_add_string (data, key_id, strv[i]);
  }
}

static gchar *
get_from_cache_episode_or_series (EpisodeResource *eres,
                                  const gchar *ecol,
                                  SeriesResource *sres,
                                  const gchar *scol)
{
  gchar *str = NULL;
  if (eres != NULL)
    g_object_get (eres, ecol, &str, NULL);

  if (str == NULL && sres != NULL)
    g_object_get (sres, scol, &str, NULL);

  return str;
}

/* Update the media with metadata
 *
 * NOTE: We give preference to an episode over the tv show for those metadatas
 * that could provide information for both. */
static void
thetvdb_update_media_from_resources (GrlMedia *video,
                                     GList *keys,
                                     SeriesResource *sres,
                                     EpisodeResource *eres)
{
  gint failed_keys = 0;
  GList *it;
  gchar *str = NULL;

  if (sres == NULL)
    return;

  for (it = keys; it != NULL; it = it->next) {
    GrlKeyID key_id = GRLPOINTER_TO_KEYID (it->data);
    gint num = -1;

    switch (key_id) {
    case GRL_METADATA_KEY_SEASON:
      if (eres != NULL)
        g_object_get (eres, EPISODE_COLUMN_SEASON_NUMBER, &num, NULL);

      if (num > 0)
        grl_media_set_season (video, num);
      else
        failed_keys++;
      break;

    case GRL_METADATA_KEY_EPISODE:
      if (eres != NULL)
        g_object_get (eres, EPISODE_COLUMN_EPISODE_NUMBER, &num, NULL);

      if (num > 0)
        grl_media_set_episode (video, num);
      else
        failed_keys++;
      break;

    case GRL_METADATA_KEY_EPISODE_TITLE:
      if (eres != NULL)
        g_object_get (eres, EPISODE_COLUMN_EPISODE_NAME, &str, NULL);

      if (str != NULL) {
        grl_media_set_episode_title (video, str);
        g_free (str);
      } else
        failed_keys++;
      break;

    case GRL_METADATA_KEY_GENRE:
      g_object_get (sres, SERIES_COLUMN_GENRES, &str, NULL);
      if (str != NULL) {
        gchar **genres;
        genres = g_strsplit (str, THETVDB_STR_DELIMITER, 0);
        thetvdb_add_data_string_unique (GRL_DATA (video),
                                        GRL_METADATA_KEY_GENRE,
                                        genres);
        g_free (str);
        g_strfreev (genres);
      } else
        failed_keys++;
      break;

    case GRL_METADATA_KEY_PERFORMER:
      g_object_get (sres, SERIES_COLUMN_ACTOR_NAMES, &str, NULL);
      if (str != NULL) {
        gchar **actors;
        actors = g_strsplit (str, THETVDB_STR_DELIMITER, 0);
        thetvdb_add_data_string_unique (GRL_DATA (video),
                                        GRL_METADATA_KEY_PERFORMER,
                                        actors);
        g_free (str);
        g_strfreev (actors);
      } else
        failed_keys++;
      break;

    case GRL_METADATA_KEY_DIRECTOR:
      if (eres != NULL)
        g_object_get (eres, EPISODE_COLUMN_DIRECTOR_NAMES, &str, NULL);

      if (str != NULL) {
        gchar **directors;
        directors = g_strsplit (str, THETVDB_STR_DELIMITER, 0);
        thetvdb_add_data_string_unique (GRL_DATA (video),
                                        GRL_METADATA_KEY_DIRECTOR,
                                        directors);
        g_free (str);
        g_strfreev (directors);
      } else
        failed_keys++;
      break;

    case GRL_METADATA_KEY_PUBLICATION_DATE:
      str = get_from_cache_episode_or_series (eres, EPISODE_COLUMN_FIRST_AIRED,
                                              sres, SERIES_COLUMN_FIRST_AIRED);
      if (str != NULL) {
        GDateTime *date;
        gint year, month, day;

        sscanf (str, "%d-%d-%d", &year, &month, &day);
        date = g_date_time_new_local (year, month, day, 0, 0, 0);
        grl_media_set_publication_date (GRL_MEDIA (video), date);
        g_date_time_unref (date);
        g_free (str);
      } else
        failed_keys++;
      break;

    case GRL_METADATA_KEY_DESCRIPTION:
      str = get_from_cache_episode_or_series (eres, EPISODE_COLUMN_OVERVIEW,
                                              sres, SERIES_COLUMN_OVERVIEW);
      if (str != NULL) {
        grl_media_set_description (GRL_MEDIA (video), str);
        g_free (str);
      } else
        failed_keys++;
      break;

    default:
      /* THETVDB Keys */
      if (key_id == GRL_THETVDB_METADATA_KEY_THETVDB_ID) {
        str = get_from_cache_episode_or_series (eres, EPISODE_COLUMN_EPISODE_ID,
                                                sres, SERIES_COLUMN_SERIES_ID);
        if (str != NULL) {
          grl_data_set_string (GRL_DATA (video),
                               GRL_THETVDB_METADATA_KEY_THETVDB_ID,
                               str);
          g_free (str);
        } else
          failed_keys++;

      } else if (key_id == GRL_THETVDB_METADATA_KEY_IMDB_ID) {
        str = get_from_cache_episode_or_series (eres, EPISODE_COLUMN_IMDB_ID,
                                                sres, SERIES_COLUMN_IMDB_ID);
        if (str != NULL) {
          grl_data_set_string (GRL_DATA (video),
                               GRL_THETVDB_METADATA_KEY_IMDB_ID,
                               str);
          g_free (str);
        } else
          failed_keys++;

      } else if (key_id == GRL_THETVDB_METADATA_KEY_ZAP2IT_ID) {
        g_object_get (sres, SERIES_COLUMN_ZAP2IT_ID, &str, NULL);
        if (str != NULL) {
          grl_data_set_string (GRL_DATA (video),
                               GRL_THETVDB_METADATA_KEY_ZAP2IT_ID,
                               str);
          g_free (str);
        } else
          failed_keys++;

      } else if (key_id == GRL_THETVDB_METADATA_KEY_GUEST_STARS) {
        if (eres != NULL)
          g_object_get (eres, EPISODE_COLUMN_GUEST_STARS_NAMES, &str, NULL);

        if (str != NULL) {
          gchar **stars;
          stars = g_strsplit (str, THETVDB_STR_DELIMITER, 0);
          thetvdb_add_data_string_unique (GRL_DATA (video),
                                          GRL_THETVDB_METADATA_KEY_GUEST_STARS,
                                          stars);
          g_free (str);
          g_strfreev (stars);
        } else
          failed_keys++;

      } else if (key_id == GRL_THETVDB_METADATA_KEY_FANART) {
        g_object_get (sres, SERIES_COLUMN_URL_FANART, &str, NULL);
        if (str != NULL) {
          grl_data_set_string (GRL_DATA (video),
                               GRL_THETVDB_METADATA_KEY_FANART,
                               str);
          g_free (str);
        } else
          failed_keys++;

      } else if (key_id == GRL_THETVDB_METADATA_KEY_BANNER) {
        g_object_get (sres, SERIES_COLUMN_URL_BANNER, &str, NULL);
        if (str != NULL) {
          grl_data_set_string (GRL_DATA (video),
                               GRL_THETVDB_METADATA_KEY_BANNER,
                               str);
          g_free (str);
        } else
          failed_keys++;

      } else if (key_id == GRL_THETVDB_METADATA_KEY_POSTER) {
        g_object_get (sres, SERIES_COLUMN_URL_POSTER, &str, NULL);
        if (str != NULL) {
          grl_data_set_string (GRL_DATA (video),
                               GRL_THETVDB_METADATA_KEY_POSTER,
                               str);
          g_free (str);
        } else
          failed_keys++;

      } else if (key_id == GRL_THETVDB_METADATA_KEY_EPISODE_SS) {
        if (eres != NULL)
          g_object_get (eres, EPISODE_COLUMN_URL_EPISODE_SCREEN, &str, NULL);

        if (str != NULL) {
          grl_data_set_string (GRL_DATA (video),
                               GRL_THETVDB_METADATA_KEY_EPISODE_SS,
                               str);
          g_free (str);
        } else
          failed_keys++;
      } else
        failed_keys++;
    }
  }

  /* Always set the series/show name */
  str = NULL;
  g_object_get (sres, SERIES_COLUMN_SERIES_NAME, &str, NULL);
  if (str != NULL) {
    grl_media_set_show (video, str);
    g_free (str);
  }

  if (failed_keys == g_list_length (keys)) {
    GRL_DEBUG ("Couldn't resolve requested keys for %s",
               grl_media_get_show (video));
  }
}

static gboolean
xml_load_data (const gchar *str,
               xmlDocPtr *doc)
{
  xmlDocPtr doc_ptr;
  xmlNodePtr node;

  doc_ptr = xmlReadMemory (str, strlen (str), NULL, NULL,
                           XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  if (!doc_ptr)
    goto free_resources;

  node = xmlDocGetRootElement (doc_ptr);
  if (!node)
    goto free_resources;

  *doc = doc_ptr;
  return TRUE;

free_resources:
  xmlFreeDoc (doc_ptr);
  return FALSE;
}

static void
web_request_succeed (const OperationSpec *os,
                     SeriesResource *sres)
{
  const gchar *show;
  GList *wait_list, *it;
  GrlTheTVDBSource *tvdb_source;

  tvdb_source = GRL_THETVDB_SOURCE (os->source);
  show = grl_media_get_show (os->media);

  wait_list = g_hash_table_lookup (tvdb_source->priv->ht_wait_list, show);
  for (it = wait_list; it != NULL; it = it->next) {
    OperationSpec *os = (OperationSpec *) it->data;

    GRL_DEBUG ("Request with id %d succeed. Show name is %s",
               os->operation_id, show);

    /* To avoid fetching again */
    os->fetched_web = TRUE;
    os->serie_resource = g_object_ref (sres);
    cache_find_episode (os);
  }
  g_object_unref (sres);
  g_list_free (wait_list);
  g_hash_table_remove (tvdb_source->priv->ht_wait_list, show);
}

static void
web_request_failed (const OperationSpec *os)
{
  const gchar *show;
  GList *wait_list, *it;
  GrlTheTVDBSource *tvdb_source;

  tvdb_source = GRL_THETVDB_SOURCE (os->source);
  show = grl_media_get_show (os->media);

  wait_list = g_hash_table_lookup (tvdb_source->priv->ht_wait_list, show);
  for (it = wait_list; it != NULL; it = it->next) {
    OperationSpec *os = (OperationSpec *) it->data;

    GRL_DEBUG ("Request with id %d failed. Show name is %s",
               os->operation_id, show);

    os->callback (os->source, os->operation_id, os->media, os->user_data, NULL);
  }
  g_list_free_full (wait_list, free_operation_spec);
  g_hash_table_remove (tvdb_source->priv->ht_wait_list, show);
}

static gboolean
unzip_all_series_data (gchar *zip,
                       gsize zip_len,
                       gchar *language,
                       gchar **dest)
{
  struct archive *a;
  struct archive_entry *entry;
  gint r;
  gboolean ret = FALSE;
  gchar *target_xml;

  a = archive_read_new ();
  archive_read_support_format_zip (a);
  r = archive_read_open_memory (a, zip, zip_len);
  if (r != ARCHIVE_OK) {
    GRL_DEBUG ("Fail to open archive.");
    goto unzip_all_series_end;
  }

  target_xml = g_strdup_printf (THETVDB_ALL_DATA_XML, language);
  while ((r = archive_read_next_header (a, &entry)) == ARCHIVE_OK) {
    const char *name;

    name = archive_entry_pathname (entry);
    GRL_DEBUG ("[ZIP] ENTRY-NAME: '%s'", name);
    if (g_strcmp0 (name, target_xml) == 0) {
      size_t size = archive_entry_size (entry);
      char *buf;
      ssize_t read;

      buf = g_malloc (size + 1);
      buf[size] = 0;
      read = archive_read_data (a, buf, size);
      if (read <= 0) {
        g_free (buf);
        if (read < 0)
          GRL_DEBUG ("Fatal error reading '%s' in archive: %s",
                     name, archive_error_string (a));
        else
          GRL_DEBUG ("Read an empty file from the archive");
      } else {
        *dest = buf;
        break;
      }
    }
    archive_read_data_skip (a);
  }

  g_free (target_xml);
  if (r != ARCHIVE_OK) {
    if (r == ARCHIVE_FATAL) {
      GRL_WARNING ("Fatal error handling archive: %s",
                   archive_error_string (a));
    }
    goto unzip_all_series_end;
  }

  ret = TRUE;
unzip_all_series_end:
  archive_read_free (a);
  return ret;
}

static void
web_get_all_zipped_done (GObject *source_object,
                         GAsyncResult *res,
                         gpointer user_data)
{
  gchar *zip_data;
  gchar *xml_data;
  gsize zip_len;
  GError *err = NULL;
  xmlDocPtr doc;
  SeriesResource *sres = NULL;
  EpisodeResource *eres;
  OperationSpec *os;
  GrlTheTVDBSource *tvdb_source;

  os = (OperationSpec *) user_data;
  tvdb_source = GRL_THETVDB_SOURCE (os->source);

  grl_net_wc_request_finish (GRL_NET_WC (source_object),
                             res, &zip_data, &zip_len, &err);
  if (err != NULL) {
    GRL_DEBUG ("Resolve operation failed due '%s'", err->message);
    g_error_free (err);
    goto get_all_zipped_end;
  }

  if (!unzip_all_series_data (zip_data, zip_len, os->lang, &xml_data)) {
    GRL_DEBUG ("Resolve operation failed while unzipping data");
    goto get_all_zipped_end;
  }

  if (!xml_load_data (xml_data, &doc)) {
    GRL_DEBUG ("Resolve operation failed while loading xml");
    g_free (xml_data);
    goto get_all_zipped_end;
  }
  g_free (xml_data);

  sres = xml_parse_and_save_serie (tvdb_source->priv->repository, doc,
                                   grl_media_get_show (os->media));
  eres = xml_parse_and_save_episodes (tvdb_source->priv->repository, doc,
                                      grl_media_get_title (os->media),
                                      grl_media_get_season (os->media),
                                      grl_media_get_episode (os->media));
  xmlFreeDoc (doc);

get_all_zipped_end:
  if (sres != NULL) {
    web_request_succeed (os, sres);
    if (eres)
      g_clear_object (&eres);
  } else {
    web_request_failed (os);
  }
}

static void
web_get_series_done (GObject *source_object,
                     GAsyncResult *res,
                     gpointer user_data)
{
  gchar *data;
  gsize len;
  gchar *url;
  gchar *serie_id;
  GError *err = NULL;
  xmlDocPtr doc;
  GrlNetWc *wc;
  OperationSpec *os;
  GrlTheTVDBSource *tvdb_source;

  os = (OperationSpec *) user_data;
  tvdb_source = GRL_THETVDB_SOURCE (os->source);

  grl_net_wc_request_finish (GRL_NET_WC (source_object),
                             res, &data, &len, &err);
  if (err != NULL) {
    GRL_WARNING ("Resolve operation failed due '%s'", err->message);
    g_error_free (err);
    goto get_series_done_error;
  }

  if (!xml_load_data (data, &doc)) {
    GRL_WARNING ("Resolve operation failed while loading xml");
    goto get_series_done_error;
  }

  serie_id = xml_parse_get_series_id (doc);
  wc = grl_net_wc_new ();
  url = g_strdup_printf (THETVDB_GET_EPISODES, tvdb_source->priv->api_key,
                         serie_id, os->lang);
  g_free (serie_id);

  GRL_DEBUG ("url[2] %s", url);
  grl_net_wc_request_async (wc, url, NULL, web_get_all_zipped_done, os);
  g_free (url);
  g_object_unref (wc);
  xmlFreeDoc (doc);
  return;

get_series_done_error:
  os->callback (os->source, os->operation_id, os->media, os->user_data, NULL);
  web_request_failed (os);
}

static void
thetvdb_execute_resolve_web (OperationSpec *os)
{
  GrlNetWc *wc;
  gchar *url;
  const gchar *show;
  GrlTheTVDBSource *tvdb_source;
  GList *wait_list;

  GRL_DEBUG ("thetvdb_resolve_web");

  tvdb_source = GRL_THETVDB_SOURCE (os->source);
  show = grl_media_get_show (os->media);

  /* If there is a request on this show already, wait. */
  wait_list = g_hash_table_lookup (tvdb_source->priv->ht_wait_list, show);
  if (wait_list == NULL) {
    wait_list = g_list_append (wait_list, os);
    g_hash_table_insert (tvdb_source->priv->ht_wait_list,
                         g_strdup (show),
                         wait_list);
    wc = grl_net_wc_new ();
    url = g_strdup_printf (THETVDB_GET_SERIES, show);

    GRL_DEBUG ("url[1] %s", url);
    grl_net_wc_request_async (wc, url, NULL, web_get_series_done, os);
    g_free (url);
    g_object_unref (wc);
  } else {
    wait_list = g_list_append (wait_list, os);
    GRL_DEBUG ("[%s] Add to wait list: %d", show, os->operation_id);
  }
}

static void
cache_find_episode_done (GObject *object,
                         GAsyncResult *res,
                         gpointer user_data)
{
  const gchar *show;
  OperationSpec *os;
  GomResource *resource;
  GError *err = NULL;

  os = (OperationSpec *) user_data;
  show = grl_media_get_show (os->media);

  resource = gom_repository_find_one_finish (GOM_REPOSITORY (object),
                                             res,
                                             &err);
  if (resource == NULL) {
    GRL_DEBUG ("[Episode] Cache miss with '%s' due '%s'", show, err->message);
    g_error_free (err);

    if (os->fetched_web == FALSE && os->cache_only == FALSE) {
      /* Fetch web API in order to update current cache */
      thetvdb_execute_resolve_web (os);
      return;
    }
    /* The cache is up-to-date and it doesn't have this episode */
    goto episode_done_end;
  }

  thetvdb_update_media_from_resources (os->media,
                                       os->keys,
                                       os->serie_resource,
                                       EPISODE_RESOURCE (resource));
  g_object_unref (resource);

episode_done_end:
  os->callback (os->source, os->operation_id, os->media, os->user_data, NULL);
  free_operation_spec (os);
}

static void
cache_find_episode (OperationSpec *os)
{
  GrlTheTVDBSource *tvdb_source;
  GomFilter *query, *by_series_id, *by_episode;
  GValue value_str = { 0, };
  gchar *series_id;
  gchar *show;
  const gchar *title;
  gint season_number, episode_number;

  GRL_DEBUG ("cache_find_episode");

  tvdb_source = GRL_THETVDB_SOURCE (os->source);
  title = grl_media_get_title (os->media);
  season_number = grl_media_get_season (os->media);
  episode_number = grl_media_get_episode (os->media);

  g_object_get (os->serie_resource,
                SERIES_COLUMN_SERIES_ID, &series_id,
                SERIES_COLUMN_SERIES_NAME, &show,
                NULL);

  /* Check if this media is an Episode of a TV Show */
  if (title == NULL && (season_number == 0 || episode_number == 0)) {
    goto cache_episode_end;
  }

  /* Find the exactly episode using series-id and some episode specific info */
  g_value_init (&value_str, G_TYPE_STRING);
  g_value_set_string (&value_str, series_id);
  by_series_id = gom_filter_new_eq (EPISODE_TYPE_RESOURCE,
                                    EPISODE_COLUMN_SERIES_ID,
                                    &value_str);
  g_value_unset (&value_str);

  if (season_number != 0 && episode_number != 0) {
    GomFilter *filter1, *filter2;
    GValue value_num = { 0, };

    g_value_init (&value_num, G_TYPE_UINT);
    g_value_set_uint (&value_num, season_number);
    filter1 = gom_filter_new_eq (EPISODE_TYPE_RESOURCE,
                                 EPISODE_COLUMN_SEASON_NUMBER,
                                 &value_num);
    g_value_set_uint (&value_num, episode_number);
    filter2 = gom_filter_new_eq (EPISODE_TYPE_RESOURCE,
                                 EPISODE_COLUMN_EPISODE_NUMBER,
                                 &value_num);
    g_value_unset (&value_num);

    /* Find episode by season number and episode number */
    by_episode = gom_filter_new_and (filter1, filter2);
    g_object_unref (filter1);
    g_object_unref (filter2);
  } else {
    g_value_init (&value_str, G_TYPE_STRING);
    g_value_set_string (&value_str, title);

    /* Find episode by its title */
    by_episode = gom_filter_new_like (EPISODE_TYPE_RESOURCE,
                                      EPISODE_COLUMN_EPISODE_NAME,
                                      &value_str);
    g_value_unset (&value_str);
  }

  query = gom_filter_new_and (by_series_id, by_episode);
  g_object_unref (by_series_id);
  g_object_unref (by_episode);

  gom_repository_find_one_async (tvdb_source->priv->repository,
                                 EPISODE_TYPE_RESOURCE,
                                 query,
                                 cache_find_episode_done,
                                 os);
  g_object_unref (query);
  g_clear_pointer (&series_id, g_free);
  g_clear_pointer (&show, g_free);
  return;

cache_episode_end:
  /* This media does not specify an episode: return series metadata */
  thetvdb_update_media_from_resources (os->media,
                                       os->keys,
                                       os->serie_resource,
                                       NULL);
  os->callback (os->source, os->operation_id, os->media, os->user_data, NULL);
  g_clear_pointer (&series_id, g_free);
  g_clear_pointer (&show, g_free);
  free_operation_spec (os);
}

static void
cache_find_serie_done (GObject *object,
                       GAsyncResult *res,
                       gpointer user_data)
{
  OperationSpec *os;
  GomResource *resource;
  const gchar *show;
  GError *err = NULL;

  os = (OperationSpec *) user_data;
  show = grl_media_get_show (os->media);

  resource = gom_repository_find_one_finish (GOM_REPOSITORY (object),
                                             res,
                                             &err);
  if (resource == NULL) {
    GRL_DEBUG ("[Series] Cache miss with '%s' due '%s'", show, err->message);
    g_error_free (err);
    if (os->cache_only == FALSE) {
      thetvdb_execute_resolve_web (os);
    } else {
      os->callback (os->source, os->operation_id, os->media, os->user_data, NULL);
      free_operation_spec (os);
    }
    return;
  }

  os->serie_resource = SERIES_RESOURCE (resource);
  cache_find_episode (os);
}

static void
cache_find_fuzzy_series_done (GObject *object,
                              GAsyncResult *res,
                              gpointer user_data)
{
  GrlTheTVDBSource *tvdb_source;
  OperationSpec *os;
  GomResource *resource;
  GError *err = NULL;
  GomFilter *query;
  GValue value = { 0, };
  gchar *series_id;

  os = (OperationSpec *) user_data;
  tvdb_source = GRL_THETVDB_SOURCE (os->source);

  /* we are interested in the series-id */
  resource = gom_repository_find_one_finish (GOM_REPOSITORY (object),
                                             res,
                                             &err);
  if (resource == NULL)
    goto cache_miss;

  g_object_get (G_OBJECT (resource),
                FUZZY_SERIES_NAMES_COLUMN_SERIES_ID, &series_id,
                NULL);
  g_object_unref (resource);

  /* Get series async */
  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, series_id);
  g_free (series_id);
  query = gom_filter_new_like (SERIES_TYPE_RESOURCE,
                               SERIES_COLUMN_SERIES_ID,
                               &value);
  g_value_unset (&value);
  gom_repository_find_one_async (tvdb_source->priv->repository,
                                 SERIES_TYPE_RESOURCE,
                                 query,
                                 cache_find_serie_done,
                                 os);
  g_object_unref (query);
  return;

cache_miss:
  if (err != NULL) {
    const gchar *show = grl_media_get_show (os->media);
    GRL_DEBUG ("[Series] Cache miss with '%s' due '%s'", show, err->message);
    g_error_free (err);
  }

  if (os->cache_only == FALSE) {
    thetvdb_execute_resolve_web (os);
  } else {
    os->callback (os->source, os->operation_id, os->media, os->user_data, NULL);
    free_operation_spec (os);
  }
}

static void
thetvdb_execute_resolve_cache (OperationSpec *os)
{
  const gchar *show;
  GrlTheTVDBSource *tvdb_source;
  GomFilter *query;
  GValue value = { 0, };

  GRL_DEBUG ("thetvdb_resolve_cache");

  tvdb_source = GRL_THETVDB_SOURCE (os->source);
  show = grl_media_get_show (os->media);

  /* Get series async */
  g_value_init (&value, G_TYPE_STRING);
  g_value_set_string (&value, show);
  query = gom_filter_new_like (FUZZY_SERIES_NAMES_TYPE_RESOURCE,
                               FUZZY_SERIES_NAMES_COLUMN_FUZZY_NAME,
                               &value);
  g_value_unset (&value);
  gom_repository_find_one_async (tvdb_source->priv->repository,
                                 FUZZY_SERIES_NAMES_TYPE_RESOURCE,
                                 query,
                                 cache_find_fuzzy_series_done,
                                 os);
  g_object_unref (query);
}

/* ================== API Implementation ================ */
static void
grl_thetvdb_source_resolve (GrlSource *source,
                            GrlSourceResolveSpec *rs)
{
  OperationSpec *os = NULL;
  GrlResolutionFlags res;

  GRL_DEBUG ("thetvdb_resolve");
  res = grl_operation_options_get_resolution_flags (rs->options);

  os = g_slice_new0 (OperationSpec);
  os->source = rs->source;
  os->operation_id = rs->operation_id;
  os->keys = g_list_copy (rs->keys);
  os->callback = rs->callback;
  os->media = rs->media;
  os->user_data = rs->user_data;
  os->error_code = GRL_CORE_ERROR_RESOLVE_FAILED;
  os->lang = get_pref_language (GRL_THETVDB_SOURCE (source));
  os->fetched_web = FALSE;
  os->cache_only = (res & GRL_RESOLVE_FAST_ONLY);

  GRL_DEBUG ("cache-only: %s", (os->cache_only) ? "yes" : "no");

  thetvdb_execute_resolve_cache (os);
}

static gboolean
grl_thetvdb_source_may_resolve (GrlSource *source,
                                GrlMedia *media,
                                GrlKeyID key_id,
                                GList **missing_keys)
{
  GrlTheTVDBSource *tvdb_source = GRL_THETVDB_SOURCE (source);

  GRL_DEBUG ("thetvdb_may_resolve");

  /* Check if this key is supported */
  if (!g_list_find (tvdb_source->priv->supported_keys,
                    GRLKEYID_TO_POINTER (key_id)))
    return FALSE;

  /* Check if resolve type and media type match */
  if (media && !grl_media_is_video (media))
    return FALSE;

  /* Check if the media has a show */
  if (!media || !grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_SHOW)) {
    if (missing_keys)
      *missing_keys = grl_metadata_key_list_new (GRL_METADATA_KEY_SHOW, NULL);

    return FALSE;
  }

  /* For season and episode number, we need the the title of the episode */
  if ((key_id == GRL_METADATA_KEY_SEASON || key_id == GRL_METADATA_KEY_EPISODE)
      && !grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_EPISODE_TITLE)) {
      if (missing_keys)
        *missing_keys = grl_metadata_key_list_new (GRL_METADATA_KEY_EPISODE_TITLE, NULL);

      return FALSE;
  }

  /* For the title of the episode, we need season and episode number */
  if (key_id == GRL_METADATA_KEY_EPISODE_TITLE) {
    GList *l = NULL;
    if (!grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_SEASON))
        l = g_list_prepend (l, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_SEASON));

    if (!grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_EPISODE))
        l = g_list_prepend (l, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_EPISODE));

    if (l != NULL) {
      if (missing_keys)
        *missing_keys = l;

      return FALSE;
    }
  }

  /* For episode's specific metadata, we need the season
   * and episode number or the title of the episode */
  if ((key_id == GRL_METADATA_KEY_DIRECTOR
       || key_id == GRL_THETVDB_METADATA_KEY_GUEST_STARS)
      && !grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_EPISODE_TITLE)) {
    GList *l = NULL;
    if (!grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_SEASON))
        l = g_list_prepend (l, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_SEASON));

    if (!grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_EPISODE))
        l = g_list_prepend (l, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_EPISODE));

    if (l != NULL) {
      if (missing_keys)
        *missing_keys = l;

      return FALSE;
    }
  }

  /* Note: The following keys could be related for both show and episode.
   * To the may-resolve operation we consider the request of those keys as
   * for the show because we only need the show name to retrieve that
   * information.
   * If the GrlMedia has Title or Episode and Season, we retrieve those keys
   * for the episode.
   *
   * GRL_METADATA_KEY_PUBLICATION_DATE
   * GRL_METADATA_KEY_DESCRIPTION
   * GRL_THETVDB_METADATA_KEY_THETVDB_ID
   * GRL_THETVDB_METADATA_KEY_IMDB_ID
   */

  return TRUE;
}

static const GList *
grl_thetvdb_source_supported_keys (GrlSource *source)
{
  GrlTheTVDBSource *tvdb_source = GRL_THETVDB_SOURCE (source);

  return tvdb_source->priv->supported_keys;
}
