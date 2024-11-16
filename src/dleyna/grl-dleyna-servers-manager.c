/*
 * Copyright © 2013 Intel Corporation. All rights reserved.
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "grl-dleyna-servers-manager.h"

#include <gio/gio.h>
#include <grilo.h>

#include "grl-dleyna-proxy-manager.h"
#include "grl-dleyna-server.h"

#define GRL_LOG_DOMAIN_DEFAULT dleyna_log_domain
GRL_LOG_DOMAIN_EXTERN(dleyna_log_domain);

struct _GrlDleynaServersManagerPrivate
{
  GrlDleynaManager *proxy;
  GHashTable *servers;

  gboolean got_error;
};

enum
{
  SERVER_FOUND,
  SERVER_LOST,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GObject *grl_dleyna_servers_manager_singleton = NULL;

G_DEFINE_TYPE_WITH_PRIVATE (GrlDleynaServersManager, grl_dleyna_servers_manager, G_TYPE_OBJECT)

static void
grl_dleyna_servers_manager_dispose (GObject *object)
{
  GrlDleynaServersManager *self = GRL_DLEYNA_SERVERS_MANAGER (object);
  GrlDleynaServersManagerPrivate *priv = self->priv;

  g_clear_object (&priv->proxy);
  g_clear_pointer (&priv->servers, g_hash_table_unref);

  G_OBJECT_CLASS (grl_dleyna_servers_manager_parent_class)->dispose (object);
}

static void
grl_dleyna_servers_manager_server_new_cb (GObject      *source_object,
                                          GAsyncResult *res,
                                          gpointer      user_data)
{
  GrlDleynaServersManager *self = GRL_DLEYNA_SERVERS_MANAGER (user_data);
  GrlDleynaServersManagerPrivate *priv = self->priv;
  GrlDleynaServer *server;
  GrlDleynaMediaDevice *device;
  const gchar *object_path;
  GError *error = NULL;

  GRL_DEBUG (G_STRFUNC);
  server = grl_dleyna_server_new_for_bus_finish (res, &error);
  if (error != NULL) {
    GRL_WARNING ("Unable to load server object: %s", error->message);
    g_error_free (error);
    return;
  }

  device = grl_dleyna_server_get_media_device (server);
  object_path = grl_dleyna_server_get_object_path (server);
  GRL_DEBUG ("%s '%s' %s %s", G_STRFUNC,
             grl_dleyna_media_device_get_friendly_name (device),
             grl_dleyna_media_device_get_udn (device),
             object_path);
  g_hash_table_insert (priv->servers, (gpointer) object_path, server);
  g_signal_emit (self, signals[SERVER_FOUND], 0, server);
}

static void
grl_dleyna_servers_manager_server_found_cb (GrlDleynaServersManager *self,
                                            const gchar             *object_path,
                                            gpointer                *data)
{
  grl_dleyna_server_new_for_bus (G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE,
                                 DLEYNA_DBUS_NAME, object_path, NULL,
                                 grl_dleyna_servers_manager_server_new_cb, self);
}

static void
grl_dleyna_servers_manager_server_lost_cb (GrlDleynaServersManager *self,
                                           const gchar             *object_path,
                                           gpointer                *data)
{
  GrlDleynaServersManagerPrivate *priv = self->priv;
  GrlDleynaServer *server;
  GrlDleynaMediaDevice *device;

  server = GRL_DLEYNA_SERVER (g_hash_table_lookup (priv->servers, object_path));
  g_return_if_fail (server != NULL);

  g_hash_table_steal (priv->servers, object_path);
  device = grl_dleyna_server_get_media_device (server);
  GRL_DEBUG ("%s '%s' %s %s", G_STRFUNC,
             grl_dleyna_media_device_get_friendly_name (device),
             grl_dleyna_media_device_get_udn (device),
             object_path);
  g_signal_emit (self, signals[SERVER_LOST], 0, server);
  g_object_unref (server);
}

static void
grl_dleyna_servers_manager_proxy_get_servers_cb (GObject      *source_object,
                                                 GAsyncResult *res,
                                                 gpointer      user_data)
{
  GrlDleynaServersManager *self = user_data;
  GrlDleynaServersManagerPrivate *priv = self->priv;
  gchar **object_paths, **path;
  GError *error = NULL;

  grl_dleyna_manager_call_get_servers_finish (priv->proxy, &object_paths, res, &error);
  if (error != NULL) {
    if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN))
      GRL_DEBUG ("Unable to fetch the list of available servers: %s", error->message);
    else
      GRL_WARNING ("Unable to fetch the list of available servers: %s", error->message);
    g_error_free (error);
    priv->got_error = TRUE;
    return;
  }

  for (path = object_paths; *path != NULL; path++) {
    grl_dleyna_servers_manager_server_found_cb (self, *path, NULL);
  }

  g_strfreev (object_paths);

  /* Release the ref taken in grl_dleyna_manager_proxy_new_for_bus() */
  g_object_unref (self);
}

static void
grl_dleyna_servers_manager_proxy_new_cb (GObject      *source_object,
                                         GAsyncResult *res,
                                         gpointer      user_data)
{
  GrlDleynaServersManager *self = user_data;
  GrlDleynaServersManagerPrivate *priv = self->priv;
  GError *error = NULL;

  priv->proxy = grl_dleyna_manager_proxy_new_for_bus_finish (res, &error);
  if (error != NULL) {
    GRL_WARNING ("Unable to connect to the dLeynaRenderer.Manager DBus object: %s", error->message);
    g_error_free (error);
    priv->got_error = TRUE;
    return;
  }

  GRL_DEBUG ("%s DLNA servers manager initialized", G_STRFUNC);

  g_object_connect (priv->proxy,
                    "swapped-object-signal::found-server", grl_dleyna_servers_manager_server_found_cb, self,
                    "swapped-object-signal::lost-server", grl_dleyna_servers_manager_server_lost_cb, self,
                    NULL);

  grl_dleyna_manager_call_get_servers (priv->proxy, NULL,
                                       grl_dleyna_servers_manager_proxy_get_servers_cb, self);
}

static GObject *
grl_dleyna_servers_manager_constructor (GType                  type,
                                        guint                  n_construct_params,
                                        GObjectConstructParam *construct_params)
{
  if (grl_dleyna_servers_manager_singleton != NULL) {
    return g_object_ref (grl_dleyna_servers_manager_singleton);
  }

  grl_dleyna_servers_manager_singleton =
      G_OBJECT_CLASS (grl_dleyna_servers_manager_parent_class)->constructor (type, n_construct_params,
                                                                             construct_params);

  g_object_add_weak_pointer (grl_dleyna_servers_manager_singleton, (gpointer) &grl_dleyna_servers_manager_singleton);

  return grl_dleyna_servers_manager_singleton;
}

static void
grl_dleyna_servers_manager_init (GrlDleynaServersManager *self)
{
  GrlDleynaServersManagerPrivate *priv;

  self->priv = priv = grl_dleyna_servers_manager_get_instance_private (self);

  grl_dleyna_manager_proxy_new_for_bus (G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE,
                                        DLEYNA_DBUS_NAME, "/com/intel/dLeynaServer", NULL,
                                        grl_dleyna_servers_manager_proxy_new_cb, g_object_ref (self));
  priv->servers = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
}

static void
grl_dleyna_servers_manager_class_init (GrlDleynaServersManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructor = grl_dleyna_servers_manager_constructor;
  object_class->dispose = grl_dleyna_servers_manager_dispose;

  signals[SERVER_FOUND] = g_signal_new ("server-found", G_TYPE_FROM_CLASS (class),
                                        G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                        g_cclosure_marshal_VOID__OBJECT,
                                        G_TYPE_NONE, 1, GRL_TYPE_DLEYNA_SERVER);

  signals[SERVER_LOST] = g_signal_new ("server-lost", G_TYPE_FROM_CLASS (class),
                                       G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                       g_cclosure_marshal_VOID__OBJECT,
                                       G_TYPE_NONE, 1, GRL_TYPE_DLEYNA_SERVER);
}

GrlDleynaServersManager *
grl_dleyna_servers_manager_dup_singleton (void)
{
  GRL_DEBUG (G_STRFUNC);
  return g_object_new (GRL_TYPE_DLEYNA_SERVERS_MANAGER, NULL);
}

gboolean
grl_dleyna_servers_manager_is_available (void)
{
  GrlDleynaServersManager *self;

  if (grl_dleyna_servers_manager_singleton == NULL)
    return FALSE;

  self = GRL_DLEYNA_SERVERS_MANAGER (grl_dleyna_servers_manager_singleton);

  return self->priv->got_error == FALSE;
}
