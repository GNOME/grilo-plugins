/*
 * Copyright (C) 2011-2012 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
 * Copyright (C) 2015 Collabora Ltd.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
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

#include <tracker-sparql.h>

#include "grl-tracker.h"
#include "grl-tracker-source-notif.h"
#include "grl-tracker-source-priv.h"
#include "grl-tracker-utils.h"

#define GRL_LOG_DOMAIN_DEFAULT tracker_notif_log_domain
GRL_LOG_DOMAIN_STATIC(tracker_notif_log_domain);

#define GRL_TRACKER_TYPE_SOURCE_NOTIFY grl_tracker_source_notify_get_type ()
G_DECLARE_FINAL_TYPE (GrlTrackerSourceNotify, grl_tracker_source_notify, GRL_TRACKER, SOURCE_NOTIFY, GObject)

struct _GrlTrackerSourceNotify {
  GObject parent;
  TrackerNotifier *notifier;
  guint events_signal_id;
};

static void grl_tracker_source_notify_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GrlTrackerSourceNotify, grl_tracker_source_notify, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, grl_tracker_source_notify_initable_iface_init))

static GrlTrackerSourceNotify *singleton = NULL;

static GrlMedia *
media_for_event (GrlTrackerSourceNotify *self,
                 TrackerNotifierEvent   *event)
{
  gchar *id_str;
  GrlMedia *media;

  id_str = g_strdup_printf ("%" G_GINT64_FORMAT, tracker_notifier_event_get_id (event));
  media = grl_tracker_build_grilo_media (tracker_notifier_event_get_type (event),
                                         GRL_TYPE_FILTER_NONE);
  grl_media_set_id (media, id_str);
  grl_media_set_url (media, tracker_notifier_event_get_location (event));

  g_free (id_str);

  return media;
}

static void
handle_changes (GrlTrackerSourceNotify   *self,
                GPtrArray                *events,
                TrackerNotifierEventType  tracker_type,
                GrlSourceChangeType       change_type)
{
  GrlTrackerSource *source = NULL;
  TrackerNotifierEvent *event;
  GPtrArray *change_list;
  GrlMedia *media;
  gint i;

  source = grl_tracker_source_find ("");

  if (!source || !grl_tracker_source_can_notify (source))
    return;

  change_list = g_ptr_array_new ();

  for (i = 0; i < events->len; i++) {
    event = g_ptr_array_index (events, i);
    if (tracker_notifier_event_get_event_type (event) != tracker_type)
      continue;

    media = media_for_event (self, event);
    g_ptr_array_add (change_list, media);
  }

  grl_source_notify_change_list (GRL_SOURCE (source), change_list,
                                 change_type, FALSE);
}

static void
notifier_event_cb (GrlTrackerSourceNotify *self,
                   GPtrArray              *events,
                   gpointer                user_data)
{
  handle_changes (self, events,
                  TRACKER_NOTIFIER_EVENT_CREATE,
                  GRL_CONTENT_ADDED);
  handle_changes (self, events,
                  TRACKER_NOTIFIER_EVENT_UPDATE,
                  GRL_CONTENT_CHANGED);
  handle_changes (self, events,
                  TRACKER_NOTIFIER_EVENT_DELETE,
                  GRL_CONTENT_REMOVED);
}

static gboolean
grl_tracker_source_notify_initable_init (GInitable    *initable,
                                         GCancellable *cancellable,
                                         GError      **error)
{
  GrlTrackerSourceNotify *self = GRL_TRACKER_SOURCE_NOTIFY (initable);

  self->notifier = tracker_notifier_new (NULL,
					 TRACKER_NOTIFIER_FLAG_QUERY_LOCATION,
					 cancellable, error);
  if (!self->notifier)
    return FALSE;

  self->events_signal_id =
    g_signal_connect_swapped (self->notifier, "events",
                              G_CALLBACK (notifier_event_cb), initable);
  return TRUE;
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
  object_class->finalize = grl_tracker_source_notify_finalize;
}

static void
grl_tracker_source_notify_init (GrlTrackerSourceNotify *self)
{
}

static void
grl_tracker_source_notify_initable_iface_init (GInitableIface *iface)
{
  iface->init = grl_tracker_source_notify_initable_init;
}

void
grl_tracker_source_dbus_start_watch (void)
{
  GError *error = NULL;

  if (singleton != NULL)
    return;

  singleton = g_initable_new (GRL_TRACKER_TYPE_SOURCE_NOTIFY, NULL, &error, NULL);
  if (singleton == NULL) {
    GRL_WARNING ("Error: %s", error->message);
    g_clear_error (&error);
  }
}

void
grl_tracker_source_dbus_stop_watch (void)
{
  g_clear_object (&singleton);
}
