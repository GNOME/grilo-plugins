/*
 * Copyright (C) 2011-2012 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
 * Copyright (C) 2020 Red Hat Inc.
 *
 * Contact: Carlos Garnacho <carlosg@gnome.org>
 *
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 *          Juan A. Suarez Romero <jasuarez@igalia.com>
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
#include "grl-tracker-source.h"
#include "grl-tracker-source-priv.h"
#include "grl-tracker-source-api.h"
#include "grl-tracker-source-cache.h"
#include "grl-tracker-source-notif.h"
#include "grl-tracker-utils.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT tracker_source_log_domain
GRL_LOG_DOMAIN_STATIC(tracker_source_log_domain);

/* ------- Definitions ------- */

#define MEDIA_TYPE "grilo-media-type"

#define TRACKER_ITEM_CACHE_SIZE (10000)

#define WRITEBACK_DBUS_NAME "org.freedesktop.Tracker3.Writeback"
#define WRITEBACK_DBUS_PATH "/org/freedesktop/Tracker3/Writeback"
#define WRITEBACK_DBUS_IFACE "org.freedesktop.Tracker3.Writeback"

/* --- Other --- */

enum {
  PROP_0,
  PROP_TRACKER_CONNECTION,
};

static void grl_tracker_source_set_property (GObject      *object,
                                             guint         propid,
                                             const GValue *value,
                                             GParamSpec   *pspec);

static void grl_tracker_source_finalize (GObject *object);

/* ===================== Globals  ================= */

/* shared data across  */
GrlTrackerCache *grl_tracker_item_cache;

/* ================== TrackerSource GObject ================ */

G_DEFINE_TYPE_WITH_PRIVATE (GrlTrackerSource, grl_tracker_source, GRL_TYPE_SOURCE);

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

static void
grl_tracker_source_class_init (GrlTrackerSourceClass * klass)
{
  GObjectClass        *g_class      = G_OBJECT_CLASS (klass);
  GrlSourceClass      *source_class = GRL_SOURCE_CLASS (klass);

  g_class->finalize     = grl_tracker_source_finalize;
  g_class->set_property = grl_tracker_source_set_property;

  source_class->cancel              = grl_tracker_source_cancel;
  source_class->supported_keys      = grl_tracker_supported_keys;
  source_class->writable_keys       = grl_tracker_source_writable_keys;
  source_class->store_metadata      = grl_tracker_source_store_metadata;
  source_class->query               = grl_tracker_source_query;
  source_class->resolve             = grl_tracker_source_resolve;
  source_class->may_resolve         = grl_tracker_source_may_resolve;
  source_class->search              = grl_tracker_source_search;
  source_class->browse              = grl_tracker_source_browse;
  source_class->notify_change_start = grl_tracker_source_change_start;
  source_class->notify_change_stop  = grl_tracker_source_change_stop;
  source_class->supported_operations = grl_tracker_source_supported_operations;
  source_class->get_caps            = grl_tracker_source_get_caps;
  source_class->test_media_from_uri = grl_tracker_source_test_media_from_uri;
  source_class->media_from_uri      = grl_tracker_source_get_media_from_uri;

  g_object_class_install_property (g_class,
                                   PROP_TRACKER_CONNECTION,
                                   g_param_spec_object ("tracker-connection",
                                                        "tracker connection",
                                                        "A Tracker connection",
                                                        TRACKER_SPARQL_TYPE_CONNECTION,
                                                        G_PARAM_WRITABLE
                                                        | G_PARAM_CONSTRUCT_ONLY
                                                        | G_PARAM_STATIC_NAME));
}

static void
grl_tracker_source_init (GrlTrackerSource *source)
{
  GrlTrackerSourcePrivate *priv = grl_tracker_source_get_instance_private (source);
  GDBusConnection *connection;

  source->priv = priv;

  priv->operations = g_hash_table_new (g_direct_hash, g_direct_equal);

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  if (connection) {
    priv->writeback =
      g_dbus_proxy_new_sync (connection,
                             G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION,
                             NULL,
                             WRITEBACK_DBUS_NAME,
                             WRITEBACK_DBUS_PATH,
                             WRITEBACK_DBUS_IFACE,
                             NULL, NULL);
  }
}

static void
grl_tracker_source_finalize (GObject *object)
{
  GrlTrackerSource *self;

  self = GRL_TRACKER_SOURCE (object);

  g_clear_object (&self->priv->notifier);
  g_clear_object (&self->priv->tracker_connection);
  g_clear_object (&self->priv->writeback);

  G_OBJECT_CLASS (grl_tracker_source_parent_class)->finalize (object);
}

static void
grl_tracker_source_set_property (GObject      *object,
                                 guint         propid,
                                 const GValue *value,
                                 GParamSpec   *pspec)

{
  GrlTrackerSourcePrivate *priv = GRL_TRACKER_SOURCE (object)->priv;

  switch (propid) {
    case PROP_TRACKER_CONNECTION:
      g_clear_object (&priv->tracker_connection);
      priv->tracker_connection = g_object_ref (g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, propid, pspec);
  }
}

TrackerSparqlConnection *
grl_tracker_source_get_tracker_connection (GrlTrackerSource *source)
{
  GrlTrackerSourcePrivate *priv;

  g_return_val_if_fail (GRL_IS_TRACKER_SOURCE (source), NULL);

  priv = source->priv;

  return priv->tracker_connection;
}

/* =================== TrackerSource Plugin  =============== */

void
grl_tracker_add_source (GrlTrackerSource *source)
{
  GrlTrackerSourcePrivate *priv = source->priv;

  GRL_DEBUG ("====================>add source '%s'",
             grl_source_get_name (GRL_SOURCE (source)));

  priv->state = GRL_TRACKER_SOURCE_STATE_RUNNING;
  grl_registry_register_source (grl_registry_get_default (),
                                grl_tracker_plugin,
                                GRL_SOURCE (g_object_ref (source)),
                                NULL);
}

void
grl_tracker_del_source (GrlTrackerSource *source)
{
  GrlTrackerSourcePrivate *priv = source->priv;

  GRL_DEBUG ("==================>del source '%s'",
             grl_source_get_name (GRL_SOURCE (source)));

  grl_tracker_source_cache_del_source (grl_tracker_item_cache, source);
  priv->state = GRL_TRACKER_SOURCE_STATE_DELETED;
  grl_registry_unregister_source (grl_registry_get_default (),
                                  GRL_SOURCE (source),
                                  NULL);
}

gboolean
grl_tracker_source_can_notify (GrlTrackerSource *source)
{
  GrlTrackerSourcePrivate *priv = source->priv;

  if (priv->state == GRL_TRACKER_SOURCE_STATE_RUNNING)
    return priv->notifier != NULL;

  return FALSE;
}

void
grl_tracker_source_sources_init (void)
{

  GRL_LOG_DOMAIN_INIT (tracker_source_log_domain, "tracker-source");

  GRL_DEBUG ("%s", __FUNCTION__);

  grl_tracker_item_cache =
    grl_tracker_source_cache_new (TRACKER_ITEM_CACHE_SIZE);

  if (grl_tracker_connection != NULL) {
    GrlTrackerSource *source;

    /* One source to rule them all. */
    source = grl_tracker_source_new (grl_tracker_connection);
    grl_tracker_add_source (source);
    g_object_unref (source);
  }
}
