/*
 * Copyright (C) 2011-2012 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
 * Copyright (C) 2015 Collabora Ltd.
 * Copyright (C) 2020 Red Hat Inc.
 *
 * Contact: Carlos Garnacho <carlosg@gnome.org>
 *
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 *          Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
 *          Juan A. Suarez Romero <jasuarez@igalia.com>
 *          Xavier Claessens <xavier.claessens@collabora.com>
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

#include <libtracker-sparql/tracker-sparql.h>

#include "grl-tracker.h"
#include "grl-tracker-source-notif.h"
#include "grl-tracker-source-priv.h"
#include "grl-tracker-utils.h"

#define GRL_LOG_DOMAIN_DEFAULT tracker_notif_log_domain
GRL_LOG_DOMAIN_STATIC(tracker_notif_log_domain);

struct _GrlTrackerSourceNotify {
  GObject parent;
  TrackerSparqlConnection *connection;
  TrackerNotifier *notifier;
  GrlSource *source;
  guint events_signal_id;
};

typedef struct {
  GrlTrackerSourceNotify *notify;
  GPtrArray *events;
  GPtrArray *medias;
  GList *keys;
  GrlOperationOptions *options;
  guint cur_media;
} GrlTrackerChangeBatch;

enum {
  PROP_0,
  PROP_CONNECTION,
  PROP_SOURCE,
  N_PROPS
};

static GParamSpec *props[N_PROPS] = { 0, };

G_DEFINE_TYPE (GrlTrackerSourceNotify, grl_tracker_source_notify, G_TYPE_OBJECT)

static void resolve_medias (GrlTrackerChangeBatch *batch);

static GrlMedia *
media_for_event (GrlTrackerSourceNotify *self,
                 TrackerNotifierEvent   *event,
                 GrlMediaType            type)
{
  GrlMedia *media;

  media = grl_tracker_build_grilo_media (type);
  grl_media_set_id (media, tracker_notifier_event_get_urn (event));

  return media;
}

static GPtrArray *
create_medias (GrlTrackerSourceNotify *self,
               GPtrArray              *events,
               GrlMediaType            media_type)
{
  TrackerNotifierEvent *event;
  GPtrArray *medias;
  GrlMedia *media;
  gint i;

  medias = g_ptr_array_new_with_free_func (g_object_unref);

  for (i = 0; i < events->len; i++) {
    event = g_ptr_array_index (events, i);
    media = media_for_event (self, event, media_type);
    g_ptr_array_add (medias, media);
  }

  return medias;
}

static void
handle_changes (GrlTrackerSourceNotify   *self,
                GPtrArray                *events,
                GPtrArray                *medias,
                TrackerNotifierEventType  tracker_type,
                GrlSourceChangeType       change_type)
{
  TrackerNotifierEvent *event;
  GPtrArray *change_list;
  GrlMedia *media;
  gint i;

  change_list = g_ptr_array_new ();

  for (i = 0; i < events->len; i++) {
    event = g_ptr_array_index (events, i);
    media = g_ptr_array_index (medias, i);

    if (tracker_notifier_event_get_event_type (event) != tracker_type)
      continue;
    if (tracker_type != TRACKER_NOTIFIER_EVENT_DELETE &&
        grl_media_get_url (media) == NULL)
      continue;

    g_ptr_array_add (change_list, g_object_ref (media));
  }

  if (change_list->len == 0) {
    g_ptr_array_unref (change_list);
    return;
  }

  grl_source_notify_change_list (self->source, change_list,
                                 change_type, FALSE);
}

static void
free_batch (GrlTrackerChangeBatch *batch)
{
  g_ptr_array_unref (batch->events);
  g_ptr_array_unref (batch->medias);
  g_list_free (batch->keys);
  g_object_unref (batch->options);
  g_free (batch);
}

static void
resolve_event_cb (GrlSource    *source,
                  guint         operation_id,
                  GrlMedia     *media,
                  gpointer      user_data,
                  const GError *error)
{
  GrlTrackerChangeBatch *batch = user_data;

  batch->cur_media++;
  resolve_medias (batch);
}

static void
resolve_medias (GrlTrackerChangeBatch *batch)
{
  GrlTrackerSourceNotify *self = batch->notify;
  GrlMedia *media = NULL;

  while (batch->cur_media < batch->medias->len) {
    TrackerNotifierEvent *event = g_ptr_array_index (batch->events, batch->cur_media);
    /* Resolving a deleted resource will come up empty */
    if (tracker_notifier_event_get_event_type (event) == TRACKER_NOTIFIER_EVENT_DELETE) {
      batch->cur_media++;
      continue;
    }

    media = g_ptr_array_index (batch->medias, batch->cur_media);
    break;
  }

  if (media) {
    grl_source_resolve (self->source,
                        media,
                        batch->keys,
                        batch->options,
                        resolve_event_cb,
                        batch);
  } else {
    handle_changes (self,
                    batch->events, batch->medias,
                    TRACKER_NOTIFIER_EVENT_CREATE,
                    GRL_CONTENT_ADDED);
    handle_changes (self,
                    batch->events, batch->medias,
                    TRACKER_NOTIFIER_EVENT_UPDATE,
                    GRL_CONTENT_CHANGED);
    handle_changes (self,
                    batch->events, batch->medias,
                    TRACKER_NOTIFIER_EVENT_DELETE,
                    GRL_CONTENT_REMOVED);
    free_batch (batch);
  }
}

static GrlMediaType
media_type_from_graph (const gchar *graph)
{
  if (g_str_has_suffix (graph, "#Audio"))
    return GRL_MEDIA_TYPE_AUDIO;
  else if (g_str_has_suffix (graph, "#Video"))
    return GRL_MEDIA_TYPE_VIDEO;
  else if (g_str_has_suffix (graph, "#Pictures"))
    return GRL_MEDIA_TYPE_IMAGE;

  return GRL_MEDIA_TYPE_UNKNOWN;
}

static void
notifier_event_cb (GrlTrackerSourceNotify *self,
                   const gchar            *service,
                   const gchar            *graph,
                   GPtrArray              *events,
                   gpointer                user_data)
{
  GrlMediaType type = media_type_from_graph (graph);
  GrlTrackerChangeBatch *batch;

  if (type == GRL_MEDIA_TYPE_UNKNOWN)
    return;

  batch = g_new0 (GrlTrackerChangeBatch, 1);
  batch->notify = g_object_ref (self);
  batch->events = g_ptr_array_ref (events);
  batch->medias = create_medias (self, events, type);
  batch->keys = grl_metadata_key_list_new (GRL_METADATA_KEY_URL, NULL);
  batch->options = grl_operation_options_new (NULL);

  resolve_medias (batch);
}

static void
grl_tracker_source_notify_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GrlTrackerSourceNotify *self = GRL_TRACKER_SOURCE_NOTIFY (object);

  switch (prop_id) {
  case PROP_CONNECTION:
    g_value_set_object (value, self->connection);
    break;
  case PROP_SOURCE:
    g_value_set_object (value, self->source);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
grl_tracker_source_notify_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GrlTrackerSourceNotify *self = GRL_TRACKER_SOURCE_NOTIFY (object);

  switch (prop_id) {
  case PROP_CONNECTION:
    self->connection = g_value_get_object (value);
    break;
  case PROP_SOURCE:
    self->source = g_value_get_object (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
grl_tracker_source_notify_constructed (GObject *object)
{
  GrlTrackerSourceNotify *self = GRL_TRACKER_SOURCE_NOTIFY (object);
  GDBusConnection *bus_connection;

  self->notifier =
    tracker_sparql_connection_create_notifier (self->connection);

  bus_connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  tracker_notifier_signal_subscribe (self->notifier,
                                     bus_connection,
                                     grl_tracker_miner_service ?
                                     grl_tracker_miner_service :
                                     "org.freedesktop.Tracker3.Miner.Files",
                                     NULL,
                                     NULL);
  g_object_unref (bus_connection);

  self->events_signal_id =
    g_signal_connect_swapped (self->notifier, "events",
                              G_CALLBACK (notifier_event_cb), object);

  G_OBJECT_CLASS (grl_tracker_source_notify_parent_class)->constructed (object);
}

static void
grl_tracker_source_notify_finalize (GObject *object)
{
  GrlTrackerSourceNotify *self = GRL_TRACKER_SOURCE_NOTIFY (object);

  if (self->events_signal_id)
    g_signal_handler_disconnect (self->notifier, self->events_signal_id);
  g_clear_object (&self->notifier);
  G_OBJECT_CLASS (grl_tracker_source_notify_parent_class)->finalize (object);
}

static void
grl_tracker_source_notify_class_init (GrlTrackerSourceNotifyClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;

  GRL_LOG_DOMAIN_INIT (tracker_notif_log_domain, "tracker-notif");
  object_class->set_property = grl_tracker_source_notify_set_property;
  object_class->get_property = grl_tracker_source_notify_get_property;
  object_class->finalize = grl_tracker_source_notify_finalize;
  object_class->constructed = grl_tracker_source_notify_constructed;

  props[PROP_CONNECTION] =
    g_param_spec_object ("connection",
                         "SPARQL Connection",
                         "SPARQL Connection",
                         TRACKER_TYPE_SPARQL_CONNECTION,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);
  props[PROP_SOURCE] =
    g_param_spec_object ("source",
                         "Source",
                         "Source being notified",
                         GRL_TYPE_SOURCE,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
grl_tracker_source_notify_init (GrlTrackerSourceNotify *self)
{
}

GrlTrackerSourceNotify *
grl_tracker_source_notify_new (GrlSource               *source,
                               TrackerSparqlConnection *sparql_conn)
{
  return g_object_new (GRL_TRACKER_TYPE_SOURCE_NOTIFY,
                       "source", source,
                       "connection", sparql_conn,
                       NULL);
}
