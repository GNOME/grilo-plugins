/*
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Juan A. Suarez Romero <jasuarez@igalia.com>
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
#include <string.h>
#include <gio/gio.h>
#include <tracker-sparql.h>

#include "grl-tracker.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT tracker_log_domain
GRL_LOG_DOMAIN_STATIC(tracker_log_domain);

/* ------- Definitions ------- */

#define MEDIA_TYPE "grilo-media-type"

#define RDF_TYPE_ALBUM  "nmm#MusicAlbum"
#define RDF_TYPE_ARTIST "nmm#Artist"
#define RDF_TYPE_AUDIO  "nfo#Audio"
#define RDF_TYPE_MUSIC  "nmm#MusicPiece"
#define RDF_TYPE_IMAGE  "nmm#Photo"
#define RDF_TYPE_VIDEO  "nmm#Video"
#define RDF_TYPE_BOX    "grilo#Box"

/* ---- Plugin information --- */

#define PLUGIN_ID   TRACKER_PLUGIN_ID

#define SOURCE_ID   "grl-tracker"
#define SOURCE_NAME "Tracker"
#define SOURCE_DESC "A plugin for searching multimedia content using Tracker"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

enum {
  METADATA,
  BROWSE,
  QUERY,
  SEARCH
};

/* --- Other --- */

#define TRACKER_MOUNTED_DATASOURCES_START                    \
  "SELECT nie:dataSource(?urn) AS ?datasource "              \
  "(SELECT nie:url(tracker:mountPoint(?ds)) "                \
  "WHERE { ?urn nie:dataSource ?ds  }) "                     \
  "(SELECT GROUP_CONCAT(tracker:isMounted(?ds), \",\") "     \
  "WHERE { ?urn nie:dataSource ?ds  }) "                     \
  "WHERE { ?urn a nfo:FileDataObject . FILTER (?urn IN ("

#define TRACKER_MOUNTED_DATASOURCES_END " ))} GROUP BY (?datasource)"

#define TRACKER_DATASOURCES_REQUEST                                     \
  "SELECT ?urn nie:dataSource(?urn) AS ?source "                        \
  "(SELECT GROUP_CONCAT(nie:url(tracker:mountPoint(?ds)), \",\") "      \
  "WHERE { ?urn nie:dataSource ?ds  }) "                                \
  "WHERE { "                                                            \
  "?urn tracker:available ?tr . "                                       \
  "?source a tracker:Volume . "                                         \
  "FILTER (bound(nie:dataSource(?urn))) "                               \
  "} "                                                                  \
  "GROUP BY (?source)"

#define TRACKER_QUERY_REQUEST                                         \
  "SELECT rdf:type(?urn) %s "                                         \
  "WHERE { %s } "                                                     \
  "ORDER BY DESC(nfo:fileLastModified(?urn)) "                        \
  "OFFSET %i "                                                        \
  "LIMIT %i"

#define TRACKER_SEARCH_REQUEST                   \
  "SELECT rdf:type(?urn) %s "                    \
  "WHERE "                                       \
  "{ "                                           \
  "?urn a nfo:Media . "                          \
  "?urn tracker:available ?tr . "                \
  "?urn fts:match '%s' . "                       \
  "%s "                                          \
  "} "                                           \
  "ORDER BY DESC(nfo:fileLastModified(?urn)) "   \
  "OFFSET %i "                                   \
  "LIMIT %i"

#define TRACKER_BROWSE_CATEGORY_REQUEST                                 \
  "SELECT rdf:type(?urn) %s "                                           \
  "WHERE "                                                              \
  "{ "                                                                  \
  "?urn a %s . "                                                        \
  "?urn tracker:available ?tr . "                                       \
  "%s "                                                                 \
  "} "                                                                  \
  "ORDER BY DESC(nfo:fileLastModified(?urn)) "                          \
  "OFFSET %i "                                                          \
  "LIMIT %i"

#define TRACKER_METADATA_REQUEST                                    \
  "SELECT %s "                                                      \
  "WHERE { ?urn nie:isStoredAs <%s> }"                              \

typedef struct {
  GrlKeyID     grl_key;
  const gchar *sparql_key_name;
  const gchar *sparql_key_attr;
  const gchar *sparql_key_flavor;
} tracker_grl_sparql_t;

typedef struct {
  gboolean in_use;

  GHashTable *updated_items;
  GList *updated_items_list;
  GList *updated_items_iter;

  /* GList *updated_sources; */

  TrackerSparqlCursor *cursor;
} tracker_evt_update_t;

struct OperationSpec {
  GrlMediaSource         *source;
  GrlTrackerSourcePriv   *priv;
  guint                   operation_id;
  GCancellable           *cancel_op;
  const GList            *keys;
  guint                   skip;
  guint                   count;
  guint                   current;
  GrlMediaSourceResultCb  callback;
  gpointer                user_data;
  TrackerSparqlCursor    *cursor;
};

enum {
  PROP_0,
  PROP_TRACKER_CONNECTION,
};

struct _GrlTrackerSourcePriv {
  TrackerSparqlConnection *tracker_connection;

  GHashTable *operations;

  gchar *tracker_datasource;
};

#define GRL_TRACKER_SOURCE_GET_PRIVATE(object)		\
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                \
                               GRL_TRACKER_SOURCE_TYPE,	\
                               GrlTrackerSourcePriv))

static GrlTrackerSource *grl_tracker_source_new (TrackerSparqlConnection *connection);

static void grl_tracker_source_set_property (GObject      *object,
                                             guint         propid,
                                             const GValue *value,
                                             GParamSpec   *pspec);

static void grl_tracker_source_constructed (GObject *object);

static void grl_tracker_source_finalize (GObject *object);

gboolean grl_tracker_plugin_init (GrlPluginRegistry *registry,
                                  const GrlPluginInfo *plugin,
                                  GList *configs);

static const GList *grl_tracker_source_supported_keys (GrlMetadataSource *source);

static void grl_tracker_source_query (GrlMediaSource *source,
                                      GrlMediaSourceQuerySpec *qs);

static void grl_tracker_source_metadata (GrlMediaSource *source,
                                         GrlMediaSourceMetadataSpec *ms);

static void grl_tracker_source_search (GrlMediaSource *source,
                                       GrlMediaSourceSearchSpec *ss);

static void grl_tracker_source_browse (GrlMediaSource *source,
                                       GrlMediaSourceBrowseSpec *bs);

static void grl_tracker_source_cancel (GrlMediaSource *source,
                                       guint operation_id);

static gchar *get_tracker_source_name (const gchar *uri,
                                       const gchar *datasource);

static void setup_key_mappings (void);

/* ===================== Globals  ================= */

static GHashTable *grl_to_sparql_mapping = NULL;
static GHashTable *sparql_to_grl_mapping = NULL;

static GVolumeMonitor *volume_monitor = NULL;
static TrackerSparqlConnection *tracker_connection = NULL;
static gboolean tracker_per_device_source = FALSE;
static const GrlPluginInfo *tracker_grl_plugin;
static guint tracker_dbus_signal_id = 0;

/* =================== Tracker Plugin  =============== */

static tracker_evt_update_t *
tracker_evt_update_new (void)
{
  tracker_evt_update_t *evt = g_slice_new0 (tracker_evt_update_t);

  evt->updated_items = g_hash_table_new (g_direct_hash, g_direct_equal);

  return evt;
}

static void
tracker_evt_update_free (tracker_evt_update_t *evt)
{
  if (!evt)
    return;

  g_hash_table_destroy (evt->updated_items);
  g_list_free (evt->updated_items_list);

  if (evt->cursor != NULL)
    g_object_unref (evt->cursor);

  g_slice_free (tracker_evt_update_t, evt);
}

static void
grl_tracker_add_source (GrlTrackerSource *source)
{
  grl_plugin_registry_register_source (grl_plugin_registry_get_default (),
                                       tracker_grl_plugin,
                                       GRL_MEDIA_PLUGIN (source),
                                       NULL);
}

static void
grl_tracker_del_source (GrlTrackerSource *source)
{
  grl_plugin_registry_unregister_source (grl_plugin_registry_get_default (),
                                         GRL_MEDIA_PLUGIN (source),
                                         NULL);
}

static void
tracker_evt_update_process_item_cb (GObject              *object,
                                    GAsyncResult         *result,
                                    tracker_evt_update_t *evt)
{
  const gchar *datasource, *uri;
  gboolean source_mounted;
  gchar *source_name = NULL;
  GrlMediaPlugin *plugin;
  GError *tracker_error = NULL;

  GRL_DEBUG ("%s", __FUNCTION__);

  if (!tracker_sparql_cursor_next_finish (evt->cursor,
                                          result,
                                          &tracker_error)) {
    if (tracker_error != NULL) {
      GRL_DEBUG ("\terror in parsing : %s", tracker_error->message);
      g_error_free (tracker_error);
    } else {
      GRL_DEBUG ("\tend of parsing :)");
    }

    tracker_evt_update_free (evt);
    return;
  }

  datasource = tracker_sparql_cursor_get_string (evt->cursor, 0, NULL);
  uri = tracker_sparql_cursor_get_string (evt->cursor, 1, NULL);
  source_mounted = tracker_sparql_cursor_get_boolean (evt->cursor, 2);

  plugin = grl_plugin_registry_lookup_source (grl_plugin_registry_get_default (),
                                              datasource);

  GRL_DEBUG ("\tdatasource=%s uri=%s mounted=%i plugin=%p",
             datasource, uri, source_mounted, plugin);

  if (source_mounted) {
    if (plugin == NULL) {
      source_name = get_tracker_source_name (uri, datasource);

      plugin = g_object_new (GRL_TRACKER_SOURCE_TYPE,
                             "source-id", datasource,
                             "source-name", source_name,
                             "source-desc", SOURCE_DESC,
                             "tracker-connection", tracker_connection,
                             NULL);
      grl_tracker_add_source (GRL_TRACKER_SOURCE (plugin));
      g_free (source_name);
    }
  } else if (!source_mounted && plugin != NULL) {
    grl_tracker_del_source (GRL_TRACKER_SOURCE (plugin));
  }

  tracker_sparql_cursor_next_async (evt->cursor, NULL,
                                    (GAsyncReadyCallback) tracker_evt_update_process_item_cb,
                                    (gpointer) evt);
}

static void
tracker_evt_update_process_cb (GObject              *object,
                               GAsyncResult         *result,
                               tracker_evt_update_t *evt)
{
  GError *tracker_error = NULL;

  GRL_DEBUG ("%s", __FUNCTION__);

  evt->cursor = tracker_sparql_connection_query_finish (tracker_connection,
                                                        result, NULL);

  if (tracker_error != NULL) {
    GRL_WARNING ("Could not execute sparql query: %s", tracker_error->message);

    g_error_free (tracker_error);
    tracker_evt_update_free (evt);
    return;
  }

  tracker_sparql_cursor_next_async (evt->cursor, NULL,
                                    (GAsyncReadyCallback) tracker_evt_update_process_item_cb,
                                    (gpointer) evt);
}

static void
tracker_evt_update_process (tracker_evt_update_t *evt)
{
  GString *request_str = g_string_new (TRACKER_MOUNTED_DATASOURCES_START);

  GRL_DEBUG ("%s", __FUNCTION__);

  g_string_append_printf (request_str, "%i",
                          GPOINTER_TO_INT (evt->updated_items_iter));
  evt->updated_items_iter = evt->updated_items_iter->next;

  while (evt->updated_items_iter != NULL) {
    g_string_append_printf (request_str, ", %i",
                            GPOINTER_TO_INT (evt->updated_items_iter->data));
    evt->updated_items_iter = evt->updated_items_iter->next;
  }

  g_string_append (request_str, TRACKER_MOUNTED_DATASOURCES_END);

  GRL_DEBUG ("\trequest : %s", request_str->str);

  tracker_sparql_connection_query_async (tracker_connection,
                                         request_str->str,
                                         NULL,
                                         (GAsyncReadyCallback) tracker_evt_update_process_cb,
                                         evt);
  g_string_free (request_str, TRUE);
}

static void
tracker_dbus_signal_cb (GDBusConnection *connection,
                        const gchar     *sender_name,
                        const gchar     *object_path,
                        const gchar     *interface_name,
                        const gchar     *signal_name,
                        GVariant        *parameters,
                        gpointer         user_data)

{
  gchar *class_name;
  gint graph = 0, subject = 0, predicate = 0, object = 0, subject_state;
  GVariantIter *iter1, *iter2;
  tracker_evt_update_t *evt = tracker_evt_update_new ();

  GRL_DEBUG ("%s", __FUNCTION__);

  g_variant_get (parameters, "(&sa(iiii)a(iiii))", &class_name, &iter1, &iter2);

  GRL_DEBUG ("\tTracker update event for class=%s ins=%li del=%li",
             class_name, g_variant_iter_n_children (iter1),
             g_variant_iter_n_children (iter2));

  while (g_variant_iter_loop (iter1, "(iiii)", &graph,
                              &subject, &predicate, &object)) {
    subject_state = GPOINTER_TO_INT (g_hash_table_lookup (evt->updated_items,
                                                          GSIZE_TO_POINTER (subject)));

    if (subject_state == 0) {
      g_hash_table_insert (evt->updated_items,
                           GSIZE_TO_POINTER (subject),
                           GSIZE_TO_POINTER (1));
      evt->updated_items_list = g_list_append (evt->updated_items_list,
                                               GSIZE_TO_POINTER (subject));
    } else if (subject_state == 2)
      evt->updated_items_list = g_list_append (evt->updated_items_list,
                                               GSIZE_TO_POINTER (subject));
  }
  g_variant_iter_free (iter1);

  while (g_variant_iter_loop (iter2, "(iiii)", &graph,
                              &subject, &predicate, &object)) {
    subject_state = GPOINTER_TO_INT (g_hash_table_lookup (evt->updated_items,
                                                          GSIZE_TO_POINTER (subject)));

    if (subject_state == 0) {
      g_hash_table_insert (evt->updated_items,
                           GSIZE_TO_POINTER (subject),
                           GSIZE_TO_POINTER (1));
      evt->updated_items_list = g_list_append (evt->updated_items_list,
                                              GSIZE_TO_POINTER (subject));
    } else if (subject_state == 2)
      evt->updated_items_list = g_list_append (evt->updated_items_list,
                                              GSIZE_TO_POINTER (subject));
  }
  g_variant_iter_free (iter2);

  evt->updated_items_iter = evt->updated_items_list;

  GRL_DEBUG ("\t%u elements updated", g_hash_table_size (evt->updated_items));
  GRL_DEBUG ("\t%u elements updated (list)",
             g_list_length (evt->updated_items_list));

  tracker_evt_update_process (evt);
}

static void
tracker_dbus_start_watch (void)
{
  GDBusConnection *connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  tracker_dbus_signal_id = g_dbus_connection_signal_subscribe (connection,
                                                               TRACKER_DBUS_SERVICE,
                                                               TRACKER_DBUS_INTERFACE_RESOURCES,
                                                               "GraphUpdated",
                                                               TRACKER_DBUS_OBJECT_RESOURCES,
                                                               NULL,
                                                               G_DBUS_SIGNAL_FLAGS_NONE,
                                                               tracker_dbus_signal_cb,
                                                               NULL,
                                                               NULL);
}

static void
tracker_get_datasource_cb (GObject             *object,
                           GAsyncResult        *result,
                           TrackerSparqlCursor *cursor)
{
  const gchar *datasource, *uri;
  gchar *source_name;
  GError *tracker_error = NULL;
  GrlTrackerSource *source;

  GRL_DEBUG ("%s", __FUNCTION__);

  if (!tracker_sparql_cursor_next_finish (cursor, result, &tracker_error)) {
    if (tracker_error == NULL) {
      GRL_DEBUG ("\tEnd of parsing of devices");
    } else {
      GRL_DEBUG ("\tError while parsing devices: %s", tracker_error->message);
      g_error_free (tracker_error);
    }
    g_object_unref (G_OBJECT (cursor));
    return;
  }

  datasource = tracker_sparql_cursor_get_string (cursor, 1, NULL);
  uri = tracker_sparql_cursor_get_string (cursor, 2, NULL);
  source = GRL_TRACKER_SOURCE (grl_plugin_registry_lookup_source (grl_plugin_registry_get_default (),
                                                                  datasource));

  if (source == NULL) {
    source_name = get_tracker_source_name (uri, datasource); /* TODO: get a better name */

    GRL_DEBUG ("\tnew datasource: urn=%s uri=%s\n", datasource, uri);

    source = g_object_new (GRL_TRACKER_SOURCE_TYPE,
                           "source-id", datasource,
                           "source-name", source_name,
                           "source-desc", SOURCE_DESC,
                           "tracker-connection", tracker_connection,
                           NULL);
    grl_plugin_registry_register_source (grl_plugin_registry_get_default (),
                                         tracker_grl_plugin,
                                         GRL_MEDIA_PLUGIN (source),
                                         NULL);
    g_free (source_name);
  }

  tracker_sparql_cursor_next_async (cursor, NULL,
                                    (GAsyncReadyCallback) tracker_get_datasource_cb,
                                    cursor);
}

static void
tracker_get_datasources_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      data)
{
  TrackerSparqlCursor *cursor;

  GRL_DEBUG ("%s", __FUNCTION__);

  cursor = tracker_sparql_connection_query_finish (tracker_connection,
                                                   result, NULL);

  tracker_sparql_cursor_next_async (cursor, NULL,
                                    (GAsyncReadyCallback) tracker_get_datasource_cb,
                                    cursor);
}

static void
tracker_get_connection_cb (GObject             *object,
                           GAsyncResult        *res,
                           const GrlPluginInfo *plugin)
{
  /* GrlTrackerSource *source; */

  GRL_DEBUG ("%s", __FUNCTION__);

  tracker_connection = tracker_sparql_connection_get_finish (res, NULL);

  if (tracker_connection != NULL) {
    if (tracker_per_device_source == TRUE) {
      /* Let's discover available data sources. */
      GRL_DEBUG ("per device source mode");

      tracker_dbus_start_watch ();

      volume_monitor = g_volume_monitor_get ();

      tracker_sparql_connection_query_async (tracker_connection,
                                             TRACKER_DATASOURCES_REQUEST,
                                             NULL,
                                             (GAsyncReadyCallback) tracker_get_datasources_cb,
                                             NULL);
    } else {
      /* One source to rule them all. */
      grl_tracker_add_source (grl_tracker_source_new (tracker_connection));
    }
  }
}

gboolean
grl_tracker_plugin_init (GrlPluginRegistry *registry,
                         const GrlPluginInfo *plugin,
                         GList *configs)
{
  GrlConfig *config;
  gint config_count;

  GRL_LOG_DOMAIN_INIT (tracker_log_domain, "tracker");

  GRL_DEBUG ("%s", __FUNCTION__);

  tracker_grl_plugin = plugin;

  if (!configs) {
    GRL_WARNING ("Configuration not provided! Using default configuration.");
  } else {
    config_count = g_list_length (configs);
    if (config_count > 1) {
      GRL_WARNING ("Provided %i configs, but will only use one", config_count);
    }

    config = GRL_CONFIG (configs->data);

    tracker_per_device_source = grl_config_get_boolean (config,
                                                        "per-device-source");
  }

  tracker_sparql_connection_get_async (NULL,
                                       (GAsyncReadyCallback) tracker_get_connection_cb,
                                       (gpointer) plugin);
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_tracker_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== Tracker GObject ================ */

static GrlTrackerSource *
grl_tracker_source_new (TrackerSparqlConnection *connection)
{
  GRL_DEBUG ("%s", __FUNCTION__);

  return g_object_new (GRL_TRACKER_SOURCE_TYPE,
                       "source-id", SOURCE_ID,
                       "source-name", SOURCE_NAME,
                       "source-desc", SOURCE_DESC,
                       "tracker-connection", connection,
                       NULL);
}

G_DEFINE_TYPE (GrlTrackerSource, grl_tracker_source, GRL_TYPE_MEDIA_SOURCE);

static void
grl_tracker_source_class_init (GrlTrackerSourceClass * klass)
{
  GrlMediaSourceClass    *source_class   = GRL_MEDIA_SOURCE_CLASS (klass);
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);
  GObjectClass           *g_class        = G_OBJECT_CLASS (klass);

  source_class->query    = grl_tracker_source_query;
  source_class->metadata = grl_tracker_source_metadata;
  source_class->search   = grl_tracker_source_search;
  source_class->browse   = grl_tracker_source_browse;
  source_class->cancel   = grl_tracker_source_cancel;

  metadata_class->supported_keys = grl_tracker_source_supported_keys;

  g_class->finalize     = grl_tracker_source_finalize;
  g_class->set_property = grl_tracker_source_set_property;
  g_class->constructed  = grl_tracker_source_constructed;

  g_object_class_install_property (g_class,
                                   PROP_TRACKER_CONNECTION,
                                   g_param_spec_object ("tracker-connection",
                                                        "tracker connection",
                                                        "A Tracker connection",
                                                        TRACKER_SPARQL_TYPE_CONNECTION,
                                                        G_PARAM_WRITABLE
                                                        | G_PARAM_CONSTRUCT_ONLY
                                                        | G_PARAM_STATIC_NAME));

  g_type_class_add_private (klass, sizeof (GrlTrackerSourcePriv));

  setup_key_mappings ();
}

static void
grl_tracker_source_init (GrlTrackerSource *source)
{
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (source);

  source->priv = priv;

  priv->operations = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static void
grl_tracker_source_constructed (GObject *object)
{
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (object);

  if (tracker_per_device_source)
    g_object_get (object, "source-id", &priv->tracker_datasource, NULL);
}

static void
grl_tracker_source_finalize (GObject *object)
{
  GrlTrackerSource *self;

  self = GRL_TRACKER_SOURCE (object);
  if (self->priv->tracker_connection)
    g_object_unref (self->priv->tracker_connection);

  G_OBJECT_CLASS (grl_tracker_source_parent_class)->finalize (object);
}

static void
grl_tracker_source_set_property (GObject      *object,
                                 guint         propid,
                                 const GValue *value,
                                 GParamSpec   *pspec)

{
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (object);

  switch (propid) {
  case PROP_TRACKER_CONNECTION:
    if (priv->tracker_connection != NULL)
      g_object_unref (G_OBJECT (priv->tracker_connection));
    priv->tracker_connection = g_object_ref (g_value_get_object (value));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, propid, pspec);
  }
}

/* ======================= Utilities ==================== */

static gchar *
get_tracker_source_name (const gchar *uri, const gchar *datasource)
{
  gchar *source_name = NULL;
  GList *mounts, *mount;
  GFile *file;

  if (uri != NULL) {
    mounts = g_volume_monitor_get_mounts (volume_monitor);
    file = g_file_new_for_uri (uri);

    mount = mounts;
    while (mount != NULL) {
      GFile *m_file = g_mount_get_root (G_MOUNT (mount->data));

      if (g_file_equal (m_file, file)) {
        gchar *m_name = g_mount_get_name (G_MOUNT (mount->data));
        g_object_unref (G_OBJECT (m_file));
        source_name = g_strdup_printf ("%s %s", SOURCE_NAME, m_name);
        g_free (m_name);
        break;
      }
      g_object_unref (G_OBJECT (m_file));

      mount = mount->next;
    }
    g_list_foreach (mounts, (GFunc) g_object_unref, NULL);
    g_list_free (mounts);
    g_object_unref (G_OBJECT (file));
  }

  if (source_name == NULL)
    source_name = g_strdup_printf  ("%s %s", SOURCE_NAME, datasource);

  return source_name;
}

static gchar *
build_flavored_key (gchar *key, const gchar *flavor)
{
  gint i = 0;

  while (key[i] != '\0') {
    if (!g_ascii_isalnum (key[i])) {
      key[i] = '_';
     }
    i++;
  }

  return g_strdup_printf ("%s_%s", key, flavor);
}

static void
insert_key_mapping (GrlKeyID     grl_key,
                    const gchar *sparql_key_attr,
                    const gchar *sparql_key_flavor)
{
  tracker_grl_sparql_t *assoc = g_slice_new0 (tracker_grl_sparql_t);
  GList *assoc_list = g_hash_table_lookup (grl_to_sparql_mapping, grl_key);
  gchar *canon_name = g_strdup (g_param_spec_get_name (grl_key));

  assoc->grl_key           = grl_key;
  assoc->sparql_key_name   = build_flavored_key (canon_name, sparql_key_flavor);
  assoc->sparql_key_attr   = sparql_key_attr;
  assoc->sparql_key_flavor = sparql_key_flavor;

  assoc_list = g_list_append (assoc_list, assoc);

  g_hash_table_insert (grl_to_sparql_mapping, grl_key, assoc_list);
  g_hash_table_insert (sparql_to_grl_mapping,
                       (gpointer) assoc->sparql_key_name,
                       assoc);
  g_hash_table_insert (sparql_to_grl_mapping,
                       (gpointer) g_param_spec_get_name (G_PARAM_SPEC (grl_key)),
                       assoc);

  g_free (canon_name);
}

static void
setup_key_mappings (void)
{
  grl_to_sparql_mapping = g_hash_table_new (g_direct_hash, g_direct_equal);
  sparql_to_grl_mapping = g_hash_table_new (g_str_hash, g_str_equal);

  insert_key_mapping (GRL_METADATA_KEY_ALBUM,
                      "nmm:albumTitle(nmm:musicAlbum(?urn))",
                      "audio");

  insert_key_mapping (GRL_METADATA_KEY_ARTIST,
                      "nmm:artistName(nmm:performer(?urn))",
                      "audio");

  insert_key_mapping (GRL_METADATA_KEY_AUTHOR,
                      "nmm:artistName(nmm:performer(?urn))",
                      "audio");

  insert_key_mapping (GRL_METADATA_KEY_BITRATE,
                      "nfo:averageBitrate(?urn)",
                      "audio");

  insert_key_mapping (GRL_METADATA_KEY_CHILDCOUNT,
                      "nfo:entryCounter(?urn)",
                      "directory");

  insert_key_mapping (GRL_METADATA_KEY_DATE,
                      "nfo:fileLastModified(?urn)",
                      "file");

  insert_key_mapping (GRL_METADATA_KEY_DURATION,
                      "nfo:duration(?urn)",
                      "audio");

  insert_key_mapping (GRL_METADATA_KEY_FRAMERATE,
                      "nfo:frameRate(?urn)",
                      "video");

  insert_key_mapping (GRL_METADATA_KEY_HEIGHT,
                      "nfo:height(?urn)",
                      "video");

  insert_key_mapping (GRL_METADATA_KEY_ID,
                      "nie:isStoredAs(?urn)",
                      "file");

  insert_key_mapping (GRL_METADATA_KEY_LAST_PLAYED,
                      "nfo:fileLastAccessed(?urn)",
                      "file");

  insert_key_mapping (GRL_METADATA_KEY_MIME,
                      "nie:mimeType(?urn)",
                      "file");

  insert_key_mapping (GRL_METADATA_KEY_SITE,
                      "nie:url(?urn)",
                      "file");

  insert_key_mapping (GRL_METADATA_KEY_TITLE,
                      "nie:title(?urn)",
                      "audio");

  insert_key_mapping (GRL_METADATA_KEY_TITLE,
                      "nfo:fileName(?urn)",
                      "file");

  insert_key_mapping (GRL_METADATA_KEY_URL,
                      "nie:url(?urn)",
                      "file");

  insert_key_mapping (GRL_METADATA_KEY_WIDTH,
                      "nfo:width(?urn)",
                      "video");
}

static tracker_grl_sparql_t *
get_mapping_from_sparql (const gchar *key)
{
  return (tracker_grl_sparql_t *) g_hash_table_lookup (sparql_to_grl_mapping,
                                                       key);
}

static GList *
get_mapping_from_grl (const GrlKeyID key)
{
  return (GList *) g_hash_table_lookup (grl_to_sparql_mapping, key);
}

static struct OperationSpec *
tracker_operation_initiate (GrlMediaSource *source,
                            GrlTrackerSourcePriv *priv,
                            guint operation_id)
{
  struct OperationSpec *os = g_slice_new0 (struct OperationSpec);

  os->source       = source;
  os->priv         = priv;
  os->operation_id = operation_id;
  os->cancel_op    = g_cancellable_new ();

  g_hash_table_insert (priv->operations, GSIZE_TO_POINTER (operation_id), os);

  return os;
}

static void
tracker_operation_terminate (struct OperationSpec *os)
{
  if (os == NULL)
    return;

  g_hash_table_remove (os->priv->operations,
                       GSIZE_TO_POINTER (os->operation_id));

  g_object_unref (G_OBJECT (os->cursor));
  g_object_unref (G_OBJECT (os->cancel_op));
  g_slice_free (struct OperationSpec, os);
}

static gchar *
get_select_string (GrlMediaSource *source, const GList *keys)
{
  const GList *key = keys;
  GString *gstr = g_string_new ("");
  GList *assoc_list;
  tracker_grl_sparql_t *assoc;

  while (key != NULL) {
    assoc_list = get_mapping_from_grl ((GrlKeyID) key->data);
    while (assoc_list != NULL) {
      assoc = (tracker_grl_sparql_t *) assoc_list->data;
      if (assoc != NULL) {
        g_string_append_printf (gstr, "%s AS %s",
                                assoc->sparql_key_attr,
                                assoc->sparql_key_name);
        g_string_append (gstr, " ");
      }
      assoc_list = assoc_list->next;
    }
    key = key->next;
  }

  return g_string_free (gstr, FALSE);
}

/* Builds an appropriate GrlMedia based on ontology type returned by tracker, or
   NULL if unknown */
static GrlMedia *
build_grilo_media (const gchar *rdf_type)
{
  GrlMedia *media = NULL;
  gchar **rdf_single_type;
  int i;

  if (!rdf_type) {
    return NULL;
  }

  /* As rdf_type can be formed by several types, split them */
  rdf_single_type = g_strsplit (rdf_type, ",", -1);
  i = g_strv_length (rdf_single_type) - 1;

  while (!media && i >= 0) {
    if (g_str_has_suffix (rdf_single_type[i], RDF_TYPE_MUSIC)) {
      media = grl_media_audio_new ();
    } else if (g_str_has_suffix (rdf_single_type[i], RDF_TYPE_VIDEO)) {
      media = grl_media_video_new ();
    } else if (g_str_has_suffix (rdf_single_type[i], RDF_TYPE_IMAGE)) {
      media = grl_media_image_new ();
    } else if (g_str_has_suffix (rdf_single_type[i], RDF_TYPE_ARTIST)) {
      media = grl_media_box_new ();
    } else if (g_str_has_suffix (rdf_single_type[i], RDF_TYPE_ALBUM)) {
      media = grl_media_box_new ();
    } else if (g_str_has_suffix (rdf_single_type[i], RDF_TYPE_BOX)) {
      media = grl_media_box_new ();
    }
    i--;
  }

  g_strfreev (rdf_single_type);

  return media;
}

static void
fill_grilo_media_from_sparql (GrlMedia            *media,
                              TrackerSparqlCursor *cursor,
                              gint                 column)
{
  const gchar *sparql_key = tracker_sparql_cursor_get_variable_name (cursor, column);
  tracker_grl_sparql_t *assoc = get_mapping_from_sparql (sparql_key);;
  union {
    gint int_val;
    gdouble double_val;
    const gchar *str_val;
  } val;

  if (assoc == NULL)
    return;

  GRL_DEBUG ("\tSetting media prop (col=%i/var=%s/prop=%s) %s",
             column,
             sparql_key,
             g_param_spec_get_name (G_PARAM_SPEC (assoc->grl_key)),
             tracker_sparql_cursor_get_string (cursor, column, NULL));

  if (tracker_sparql_cursor_is_bound (cursor, column) == FALSE) {
    GRL_DEBUG ("\t\tDropping, no data");
    return;
  }

  if (grl_data_has_key (GRL_DATA (media), assoc->grl_key)) {
    GRL_DEBUG ("\t\tDropping, already here");
    return;
  }

  switch (G_PARAM_SPEC (assoc->grl_key)->value_type) {
  case G_TYPE_STRING:
    val.str_val = tracker_sparql_cursor_get_string (cursor, column, NULL);
    if (val.str_val != NULL)
      grl_data_set_string (GRL_DATA (media), assoc->grl_key, val.str_val);
    break;

  case G_TYPE_INT:
    val.int_val = tracker_sparql_cursor_get_integer (cursor, column);
    grl_data_set_int (GRL_DATA (media), assoc->grl_key, val.int_val);
    break;

  case G_TYPE_FLOAT:
    val.double_val = tracker_sparql_cursor_get_double (cursor, column);
    grl_data_set_float (GRL_DATA (media), assoc->grl_key, (gfloat) val.double_val);
    break;

  default:
    GRL_DEBUG ("\t\tUnexpected data type");
    break;
  }
}

static void
tracker_query_result_cb (GObject              *source_object,
                         GAsyncResult         *result,
                         struct OperationSpec *operation)
{
  gint         col;
  const gchar *sparql_type;
  GError      *tracker_error = NULL, *error = NULL;
  GrlMedia    *media;

  GRL_DEBUG ("%s", __FUNCTION__);

  if (g_cancellable_is_cancelled (operation->cancel_op)) {
    GRL_DEBUG ("\tOperation %u cancelled", operation->operation_id);
    operation->callback (operation->source,
                         operation->operation_id,
                         NULL, 0,
                         operation->user_data, NULL);
    tracker_operation_terminate (operation);

    return;
  }

  if (!tracker_sparql_cursor_next_finish (operation->cursor,
                                          result,
                                          &tracker_error)) {
    if (tracker_error != NULL) {
      GRL_DEBUG ("\terror in parsing : %s", tracker_error->message);

      error = g_error_new (GRL_CORE_ERROR,
                           GRL_CORE_ERROR_BROWSE_FAILED,
                           "Failed to start browse action : %s",
                           tracker_error->message);

      operation->callback (operation->source,
                           operation->operation_id,
                           NULL, 0,
                           operation->user_data, error);

      g_error_free (error);
      g_error_free (tracker_error);
    } else {
      GRL_DEBUG ("\tend of parsing :)");

      /* Only emit this last one if more result than expected */
      if (operation->count > 1)
        operation->callback (operation->source,
                             operation->operation_id,
                             NULL, 0,
                             operation->user_data, NULL);
    }

    tracker_operation_terminate (operation);
    return;
  }

  sparql_type = tracker_sparql_cursor_get_string (operation->cursor, 0, NULL);

  GRL_DEBUG ("Parsing line %i of type %s", operation->current, sparql_type);

  media = build_grilo_media (sparql_type);

  if (media != NULL) {
    for (col = 1 ;
         col < tracker_sparql_cursor_get_n_columns (operation->cursor) ;
         col++) {
      fill_grilo_media_from_sparql (media, operation->cursor, col);
    }

    operation->callback (operation->source,
                         operation->operation_id,
                         media,
                         --operation->count,
                         operation->user_data,
                         NULL);
  }

  /* Schedule the next line to parse */
  operation->current++;
  if (operation->count < 1)
        tracker_operation_terminate (operation);
  else
    tracker_sparql_cursor_next_async (operation->cursor, operation->cancel_op,
                                      (GAsyncReadyCallback) tracker_query_result_cb,
                                      (gpointer) operation);
}

static void
tracker_query_cb (GObject              *source_object,
                  GAsyncResult         *result,
                  struct OperationSpec *operation)
{
  GError *tracker_error = NULL, *error = NULL;

  GRL_DEBUG ("%s", __FUNCTION__);

  operation->cursor =
    tracker_sparql_connection_query_finish (operation->priv->tracker_connection,
                                            result, &tracker_error);

  if (tracker_error) {
    GRL_WARNING ("Could not execute sparql query: %s", tracker_error->message);

    error = g_error_new (GRL_CORE_ERROR,
			 GRL_CORE_ERROR_BROWSE_FAILED,
			 "Failed to start browse action : %s",
                         tracker_error->message);

    operation->callback (operation->source, operation->operation_id, NULL, 0,
                         operation->user_data, error);

    g_error_free (tracker_error);
    g_error_free (error);
    g_slice_free (struct OperationSpec, operation);

    return;
  }

  /* Start parsing results */
  operation->current = 0;
  tracker_sparql_cursor_next_async (operation->cursor, NULL,
                                    (GAsyncReadyCallback) tracker_query_result_cb,
                                    (gpointer) operation);
}

static void
tracker_metadata_cb (GObject                    *source_object,
                     GAsyncResult               *result,
                     GrlMediaSourceMetadataSpec *ms)
{
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (ms->source);
  gint                  col;
  GError               *tracker_error = NULL, *error = NULL;
  TrackerSparqlCursor  *cursor;

  GRL_DEBUG ("%s", __FUNCTION__);

  cursor = tracker_sparql_connection_query_finish (priv->tracker_connection,
                                                   result, &tracker_error);

  if (tracker_error) {
    GRL_WARNING ("Could not execute sparql query: %s", tracker_error->message);

    error = g_error_new (GRL_CORE_ERROR,
			 GRL_CORE_ERROR_BROWSE_FAILED,
			 "Failed to start browse action : %s",
                         tracker_error->message);

    ms->callback (ms->source, NULL, ms->user_data, error);

    g_error_free (tracker_error);
    g_error_free (error);

    goto end_operation;
  }


  tracker_sparql_cursor_next (cursor, NULL, NULL);

  /* Translate Sparql result into Grilo result */
  for (col = 0 ; col < tracker_sparql_cursor_get_n_columns (cursor) ; col++) {
    fill_grilo_media_from_sparql (ms->media, cursor, col);
  }

  ms->callback (ms->source, ms->media, ms->user_data, NULL);

 end_operation:
  if (cursor)
    g_object_unref (G_OBJECT (cursor));
}

static gchar *
tracker_source_get_device_constraint (GrlTrackerSourcePriv *priv)
{
  if (priv->tracker_datasource == NULL)
    return g_strdup ("");

  return g_strdup_printf ("?urn nie:dataSource <%s> .",
                          priv->tracker_datasource);
}

/* ================== API Implementation ================ */

static const GList *
grl_tracker_source_supported_keys (GrlMetadataSource *source)
{
  return grl_plugin_registry_get_metadata_keys (grl_plugin_registry_get_default ());
}

/**
 * Query is a SPARQL query.
 *
 * Columns must be named with the Grilo key name that the column
 * represent. Unnamed or unknown columns will be ignored.
 *
 * First column must be the media type, and it does not need to be named.  It
 * must match with any value supported in rdf:type() property, or
 * grilo#Box. Types understood are:
 *
 * <itemizedlist>
 *   <listitem>
 *     <para>
 *       <literal>nmm#MusicPiece</literal>
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       <literal>nmm#Video</literal>
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       <literal>nmm#Photo</literal>
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       <literal>nmm#Artist</literal>
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       <literal>nmm#MusicAlbum</literal>
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       <literal>grilo#Box</literal>
 *     </para>
 *   </listitem>
 * </itemizedlist>
 *
 * An example for searching all songs:
 *
 * <informalexample>
 *   <programlisting>
 *     SELECT rdf:type(?song)
 *            ?song            AS id
 *            nie:title(?song) AS title
 *            nie:url(?song)   AS url
 *     WHERE { ?song a nmm:MusicPiece }
 *   </programlisting>
 * </informalexample>
 *
 * Alternatively, we can use a partial SPARQL query: just specify the sentence
 * in the WHERE part. In this case, "?urn" is the ontology concept to be used in
 * the clause.
 *
 * An example of such partial query:
 *
 * <informalexample>
 *   <programlisting>
 *     ?urn a nfo:Media
 *   </programlisting>
 * </informalexample>
 *
 * In this case, all data required to build a full SPARQL query will be get from
 * the query spec.
 */
static void
grl_tracker_source_query (GrlMediaSource *source,
                          GrlMediaSourceQuerySpec *qs)
{
  GError               *error = NULL;
  GrlTrackerSourcePriv *priv  = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  gchar                *sparql_final;
  gchar                *sparql_select;
  struct OperationSpec *os;

  GRL_DEBUG ("%s: id=%u", __FUNCTION__, qs->query_id);

  if (!qs->query || qs->query[0] == '\0') {
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_QUERY_FAILED,
                                 "Empty query");
    goto send_error;
  }

  /* Check if it is a full sparql query */
  if (g_ascii_strncasecmp (qs->query, "select ", 7) != 0) {
    sparql_select = get_select_string (source, qs->keys);
    sparql_final = g_strdup_printf (TRACKER_QUERY_REQUEST,
                                    sparql_select,
                                    qs->query,
                                    qs->skip,
                                    qs->count);
    g_free (qs->query);
    g_free (sparql_select);
    qs->query = sparql_final;
    grl_tracker_source_query (source, qs);
    return;
  }

  GRL_DEBUG ("select : %s", qs->query);

  os = tracker_operation_initiate (source, priv, qs->query_id);
  os->keys         = qs->keys;
  os->skip         = qs->skip;
  os->count        = qs->count;
  os->callback     = qs->callback;
  os->user_data    = qs->user_data;

  tracker_sparql_connection_query_async (priv->tracker_connection,
                                         qs->query,
                                         os->cancel_op,
                                         (GAsyncReadyCallback) tracker_query_cb,
                                         os);

  return;

 send_error:
  qs->callback (qs->source, qs->query_id, NULL, 0, qs->user_data, error);
  g_error_free (error);
}

static void
grl_tracker_source_metadata (GrlMediaSource *source,
                             GrlMediaSourceMetadataSpec *ms)
{
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  gchar                *sparql_select, *sparql_final;

  GRL_DEBUG ("%s: id=%i", __FUNCTION__, ms->metadata_id);

  sparql_select = get_select_string (source, ms->keys);
  sparql_final = g_strdup_printf (TRACKER_METADATA_REQUEST, sparql_select,
                                  grl_media_get_id (ms->media));

  GRL_DEBUG ("select: '%s'", sparql_final);

  tracker_sparql_connection_query_async (priv->tracker_connection,
                                         sparql_final,
                                         NULL,
                                         (GAsyncReadyCallback) tracker_metadata_cb,
                                         ms);

  if (sparql_select != NULL)
    g_free (sparql_select);
  if (sparql_final != NULL)
    g_free (sparql_final);
}

static void
grl_tracker_source_search (GrlMediaSource *source, GrlMediaSourceSearchSpec *ss)
{
  GrlTrackerSourcePriv *priv  = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  gchar                *constraint;
  gchar                *sparql_select;
  gchar                *sparql_final;
  GError               *error = NULL;
  struct OperationSpec *os;

  GRL_DEBUG ("%s: id=%u", __FUNCTION__, ss->search_id);

  if (!ss->text || ss->text[0] == '\0') {
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_QUERY_FAILED,
                                 "Empty search");
    goto send_error;
  }

  constraint = tracker_source_get_device_constraint (priv);
  sparql_select = get_select_string (source, ss->keys);
  sparql_final = g_strdup_printf (TRACKER_SEARCH_REQUEST, sparql_select,
                                  ss->text, constraint, ss->skip, ss->count);

  GRL_DEBUG ("select: '%s'", sparql_final);

  os = tracker_operation_initiate (source, priv, ss->search_id);
  os->keys         = ss->keys;
  os->skip         = ss->skip;
  os->count        = ss->count;
  os->callback     = ss->callback;
  os->user_data    = ss->user_data;

  tracker_sparql_connection_query_async (priv->tracker_connection,
                                         sparql_final,
                                         os->cancel_op,
                                         (GAsyncReadyCallback) tracker_query_cb,
                                         os);

  g_free (constraint);
  g_free (sparql_select);
  g_free (sparql_final);

  return;

 send_error:
  ss->callback (ss->source, ss->search_id, NULL, 0, ss->user_data, error);
  g_error_free (error);
}

static void
grl_tracker_source_browse (GrlMediaSource *source,
                           GrlMediaSourceBrowseSpec *bs)
{
  GrlTrackerSourcePriv *priv  = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  gchar                *constraint;
  gchar                *sparql_select;
  gchar                *sparql_final;
  struct OperationSpec *os;
  GrlMedia             *media;

  GRL_DEBUG ("%s: id=%u", __FUNCTION__, bs->browse_id);

  if ((bs->container == NULL || grl_media_get_id (bs->container) == NULL)) {
    /* Hardcoded categories */
    media = grl_media_box_new ();
    grl_media_set_title (media, "Music");
    grl_media_set_id (media, "nmm:MusicPiece");
    bs->callback (bs->source, bs->browse_id, media, 2, bs->user_data, NULL);

    media = grl_media_box_new ();
    grl_media_set_title (media, "Photo");
    grl_media_set_id (media, "nmm:Photo");
    bs->callback (bs->source, bs->browse_id, media, 1, bs->user_data, NULL);

    media = grl_media_box_new ();
    grl_media_set_title (media, "Video");
    grl_media_set_id (media, "nmm:Video");
    bs->callback (bs->source, bs->browse_id, media, 0, bs->user_data, NULL);
    return;
  }

  constraint = tracker_source_get_device_constraint (priv);
  sparql_select = get_select_string (bs->source, bs->keys);
  sparql_final = g_strdup_printf (TRACKER_BROWSE_CATEGORY_REQUEST,
                                  sparql_select,
                                  grl_media_get_id (bs->container),
                                  constraint,
                                  bs->skip, bs->count);

  GRL_DEBUG ("select: '%s'", sparql_final);

  os = tracker_operation_initiate (source, priv, bs->browse_id);
  os->keys         = bs->keys;
  os->skip         = bs->skip;
  os->count        = bs->count;
  os->callback     = bs->callback;
  os->user_data    = bs->user_data;

  tracker_sparql_connection_query_async (priv->tracker_connection,
                                         sparql_final,
                                         os->cancel_op,
                                         (GAsyncReadyCallback) tracker_query_cb,
                                         os);

  g_free (constraint);
  g_free (sparql_select);
  g_free (sparql_final);
}

static void
grl_tracker_source_cancel (GrlMediaSource *source, guint operation_id)
{
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  struct OperationSpec *os;

  GRL_DEBUG ("%s: id=%u", __FUNCTION__, operation_id);

  os = g_hash_table_lookup (priv->operations, GSIZE_TO_POINTER (operation_id));

  if (os != NULL)
    g_cancellable_cancel (os->cancel_op);
}
