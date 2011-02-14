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
#include <tracker-sparql.h>

#include "grl-tracker.h"
#include "grl-tracker-priv.h"
#include "grl-tracker-api.h"
#include "grl-tracker-cache.h"
#include "grl-tracker-notif.h"
#include "grl-tracker-utils.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT tracker_general_log_domain
GRL_LOG_DOMAIN_STATIC(tracker_general_log_domain);

/* ------- Definitions ------- */

#define MEDIA_TYPE "grilo-media-type"

#define TRACKER_ITEM_CACHE_SIZE (10000)

/* --- Other --- */

enum {
  PROP_0,
  PROP_TRACKER_CONNECTION,
};

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

/* ===================== Globals  ================= */

TrackerSparqlConnection *grl_tracker_connection = NULL;
const GrlPluginInfo *grl_tracker_plugin;

/* shared data across  */
GrlTrackerItemCache *grl_tracker_item_cache;
GHashTable *grl_tracker_modified_sources;

/* tracker plugin config */
gboolean grl_tracker_per_device_source = FALSE;


/* =================== Tracker Plugin  =============== */

void
grl_tracker_add_source (GrlTrackerSource *source)
{
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (source);

  GRL_DEBUG ("====================>add source '%s' count=%u",
             grl_metadata_source_get_name (GRL_METADATA_SOURCE (source)),
             priv->notification_ref);

  if (priv->notification_ref > 0) {
    priv->notification_ref--;
  }
  if (priv->notification_ref == 0) {
    g_hash_table_remove (grl_tracker_modified_sources,
			 grl_metadata_source_get_id (GRL_METADATA_SOURCE (source)));
    priv->state = GRL_TRACKER_SOURCE_STATE_RUNNING;
    grl_plugin_registry_register_source (grl_plugin_registry_get_default (),
					 grl_tracker_plugin,
					 GRL_MEDIA_PLUGIN (source),
					 NULL);
  }
}

void
grl_tracker_del_source (GrlTrackerSource *source)
{
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (source);

  GRL_DEBUG ("==================>del source '%s' count=%u",
             grl_metadata_source_get_name (GRL_METADATA_SOURCE (source)),
             priv->notification_ref);
  if (priv->notification_ref > 0) {
    priv->notification_ref--;
  }
  if (priv->notification_ref == 0) {
    g_hash_table_remove (grl_tracker_modified_sources,
			 grl_metadata_source_get_id (GRL_METADATA_SOURCE (source)));
    priv->state = GRL_TRACKER_SOURCE_STATE_DELETED;
    grl_plugin_registry_unregister_source (grl_plugin_registry_get_default (),
					   GRL_MEDIA_PLUGIN (source),
					   NULL);
  }
}

gboolean
grl_tracker_source_can_notify (GrlTrackerSource *source)
{
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (source);

  if (priv->state == GRL_TRACKER_SOURCE_STATE_RUNNING)
    return priv->notify_changes;

  return FALSE;
}

GrlTrackerSource *
grl_tracker_source_find (const gchar *id)
{
  GrlMediaPlugin *source;

  source = grl_plugin_registry_lookup_source (grl_plugin_registry_get_default (),
					      id);

  if (source && GRL_IS_TRACKER_SOURCE (source))
    return (GrlTrackerSource *) source;

  return (GrlTrackerSource *) g_hash_table_lookup (grl_tracker_modified_sources,
						   id);
}

static void
tracker_get_datasource_cb (GObject             *object,
                           GAsyncResult        *result,
                           TrackerSparqlCursor *cursor)
{
  const gchar *type, *datasource, *datasource_name, *uri;
  gboolean volume_mounted, upnp_available, source_available;
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

  type = tracker_sparql_cursor_get_string (cursor, 0, NULL);
  datasource = tracker_sparql_cursor_get_string (cursor, 1, NULL);
  datasource_name = tracker_sparql_cursor_get_string (cursor, 2, NULL);
  uri = tracker_sparql_cursor_get_string (cursor, 3, NULL);
  volume_mounted = tracker_sparql_cursor_get_boolean (cursor, 4);
  upnp_available = tracker_sparql_cursor_get_boolean (cursor, 5);

  source_available = volume_mounted | upnp_available;

  source = grl_tracker_source_find (datasource);

  if ((source == NULL) && source_available) {
    gchar *source_name = grl_tracker_get_source_name (type, uri, datasource,
                                                      datasource_name);
    GRL_DEBUG ("\tnew datasource: urn=%s name=%s uri=%s\n",
	       datasource, datasource_name, uri);
    source = g_object_new (GRL_TRACKER_SOURCE_TYPE,
                           "source-id", datasource,
                           "source-name", source_name,
                           "source-desc", GRL_TRACKER_SOURCE_DESC,
                           "tracker-connection", grl_tracker_connection,
                           NULL);
    grl_tracker_add_source (source);
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

  cursor = tracker_sparql_connection_query_finish (grl_tracker_connection,
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

  grl_tracker_connection = tracker_sparql_connection_get_finish (res, NULL);

  if (grl_tracker_connection != NULL) {
    grl_tracker_dbus_start_watch ();

    if (grl_tracker_per_device_source == TRUE) {
      /* Let's discover available data sources. */
      GRL_DEBUG ("\tper device source mode request: '"
                 TRACKER_DATASOURCES_REQUEST "'");

      tracker_sparql_connection_query_async (grl_tracker_connection,
                                             TRACKER_DATASOURCES_REQUEST,
                                             NULL,
                                             (GAsyncReadyCallback) tracker_get_datasources_cb,
                                             NULL);
    } else {
      /* One source to rule them all. */
      grl_tracker_add_source (grl_tracker_source_new (grl_tracker_connection));
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

  GRL_DEBUG ("%s", __FUNCTION__);

  GRL_LOG_DOMAIN_INIT (tracker_general_log_domain, "tracker-general");
  grl_tracker_init_notifs ();
  grl_tracker_init_requests ();

  grl_tracker_plugin = plugin;
  grl_tracker_item_cache = grl_tracker_item_cache_new (TRACKER_ITEM_CACHE_SIZE);
  grl_tracker_modified_sources = g_hash_table_new (g_str_hash, g_str_equal);

  if (!configs) {
    GRL_WARNING ("\tConfiguration not provided! Using default configuration.");
  } else {
    config_count = g_list_length (configs);
    if (config_count > 1) {
      GRL_WARNING ("\tProvided %i configs, but will only use one", config_count);
    }

    config = GRL_CONFIG (configs->data);

    grl_tracker_per_device_source =
      grl_config_get_boolean (config, "per-device-source");
  }

  tracker_sparql_connection_get_async (NULL,
                                       (GAsyncReadyCallback) tracker_get_connection_cb,
                                       (gpointer) plugin);
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_tracker_plugin_init,
                     NULL,
                     GRL_TRACKER_PLUGIN_ID);

/* ================== Tracker GObject ================ */

static GrlTrackerSource *
grl_tracker_source_new (TrackerSparqlConnection *connection)
{
  GRL_DEBUG ("%s", __FUNCTION__);

  return g_object_new (GRL_TRACKER_SOURCE_TYPE,
                       "source-id", GRL_TRACKER_SOURCE_ID,
                       "source-name", GRL_TRACKER_SOURCE_NAME,
                       "source-desc", GRL_TRACKER_SOURCE_DESC,
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

  source_class->query               = grl_tracker_source_query;
  source_class->metadata            = grl_tracker_source_metadata;
  source_class->search              = grl_tracker_source_search;
  source_class->browse              = grl_tracker_source_browse;
  source_class->cancel              = grl_tracker_source_cancel;
  source_class->notify_change_start = grl_tracker_source_change_start;
  source_class->notify_change_stop  = grl_tracker_source_change_stop;

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

  grl_tracker_setup_key_mappings ();
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

  if (grl_tracker_per_device_source)
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

