/*
 * Copyright (C) 2011 Intel Corp
 * Copyright (C) 2013 Bastien Nocera <hadess@hadess.net>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option) any
 * later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this package; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <string.h>
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-common/simple-watch.h>

#include "freebox-monitor.h"

struct _FreeboxMonitorPrivate {
  /* Avahi <-> GLib adaptors */
  AvahiGLibPoll *poll;
  /* Avahi client */
  AvahiClient *client;
  /* Service browser */
  AvahiServiceBrowser *browser;
};

G_DEFINE_TYPE_WITH_PRIVATE (FreeboxMonitor, freebox_monitor, G_TYPE_OBJECT)

enum {
  FOUND,
  LOST,
  NUM_SIGS
};

static guint signals[NUM_SIGS] = {0,};

static void
on_browse_callback (AvahiServiceBrowser *b,
                    AvahiIfIndex interface, AvahiProtocol protocol,
                    AvahiBrowserEvent event,
                    const char *name,
                    const char *type,
                    const char *domain,
                    AvahiLookupResultFlags flags,
                    void* userdata)
{
  FreeboxMonitor *self = FREEBOX_MONITOR (userdata);

  switch (event) {
  case AVAHI_BROWSER_NEW:
    /* Emit the found signal */
    g_signal_emit (self, signals[FOUND], 0, name);
    break;
  case AVAHI_BROWSER_REMOVE:
    /* Emit the lost signal */
    g_signal_emit (self, signals[LOST], 0, name);
    break;
  default:
    /* Nothing */
    ;
  }
}

static void
on_client_state_changed (AvahiClient *client, AvahiClientState state, void *user_data)
{
  FreeboxMonitor *self = FREEBOX_MONITOR (user_data);
  FreeboxMonitorPrivate *priv = self->priv;

  switch (state) {
  case AVAHI_CLIENT_S_RUNNING:
    {
      priv->browser = avahi_service_browser_new (client,
                                                 AVAHI_IF_UNSPEC,
                                                 AVAHI_PROTO_UNSPEC,
                                                 "_fbx-api._tcp",
                                                 NULL, 0,
                                                 on_browse_callback, self);
    }
    break;
  case AVAHI_CLIENT_S_REGISTERING:
  case AVAHI_CLIENT_CONNECTING:
    /* Silently do nothing */
    break;
  case AVAHI_CLIENT_S_COLLISION:
  case AVAHI_CLIENT_FAILURE:
  default:
    g_warning ("Cannot connect to Avahi: state %d", state);
    break;
  }
}

static void
freebox_monitor_finalize (GObject *object)
{
  FreeboxMonitorPrivate *priv = FREEBOX_MONITOR(object)->priv;

  if (priv->browser) {
    avahi_service_browser_free (priv->browser);
    priv->browser = NULL;
  }
  if (priv->client) {
    avahi_client_free (priv->client);
    priv->client = NULL;
  }
  if (priv->poll) {
    avahi_glib_poll_free (priv->poll);
    priv->poll = NULL;
  }
}

static void
freebox_monitor_class_init (FreeboxMonitorClass *klass)
{
    GObjectClass *o_class = (GObjectClass *)klass;

    o_class->finalize = freebox_monitor_finalize;

    signals[FOUND] = g_signal_new ("found",
                                   FREEBOX_TYPE_MONITOR,
                                   G_SIGNAL_RUN_FIRST,
                                   0, NULL, NULL,
                                   g_cclosure_marshal_VOID__STRING,
                                   G_TYPE_NONE,
                                   1, G_TYPE_STRING);

    signals[LOST] = g_signal_new ("lost",
                                  FREEBOX_TYPE_MONITOR,
                                  G_SIGNAL_RUN_FIRST,
                                  0, NULL, NULL,
                                  g_cclosure_marshal_VOID__STRING,
                                  G_TYPE_NONE,
                                  1, G_TYPE_STRING);

}

static void
freebox_monitor_init (FreeboxMonitor *self)
{
  FreeboxMonitorPrivate *priv;
  int error;

  priv = self->priv = freebox_monitor_get_instance_private (self);

  priv->poll = avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);
  priv->client = avahi_client_new (avahi_glib_poll_get (priv->poll),
                                   AVAHI_CLIENT_NO_FAIL,
                                   on_client_state_changed,
                                   self,
                                   &error);
}

FreeboxMonitor *
freebox_monitor_new (void)
{
  return g_object_new (FREEBOX_TYPE_MONITOR, NULL);
}
