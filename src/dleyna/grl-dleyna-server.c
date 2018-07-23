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

#include "grl-dleyna-server.h"

#include <grilo.h>

typedef enum
{
  INIT_NOTHING   = 0,
  INIT_DEVICE    = 1 << 0,
  INIT_OBJECT    = 1 << 1,
  INIT_CONTAINER = 1 << 2,
  INIT_READY     = INIT_DEVICE | INIT_OBJECT | INIT_CONTAINER
} InitStatus;

struct _GrlDleynaServerPrivate
{
  GBusType bus_type;
  GDBusProxyFlags flags;
  gchar *object_path;
  gchar *well_known_name;

  GrlDleynaMediaDevice *media_device;
  GrlDleynaMediaObject2 *media_object;
  GrlDleynaMediaContainer2 *media_container;

  InitStatus init_status;
};

enum
{
  PROP_0,
  PROP_BUS_TYPE,
  PROP_WELL_KNOWN_NAME,
  PROP_FLAGS,
  PROP_OBJECT_PATH
};

static void grl_dleyna_server_async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GrlDleynaServer, grl_dleyna_server, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GrlDleynaServer)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, grl_dleyna_server_async_initable_iface_init))

static void
grl_dleyna_server_dispose (GObject *object)
{
  GrlDleynaServer *self = GRL_DLEYNA_SERVER (object);
  GrlDleynaServerPrivate *priv = self->priv;

  g_clear_object (&priv->media_device);
  g_clear_object (&priv->media_object);
  g_clear_object (&priv->media_container);

  G_OBJECT_CLASS (grl_dleyna_server_parent_class)->dispose (object);
}

static void
grl_dleyna_server_finalize (GObject *object)
{
  GrlDleynaServer *self = GRL_DLEYNA_SERVER (object);
  GrlDleynaServerPrivate *priv = self->priv;

  g_free (priv->well_known_name);
  g_free (priv->object_path);

  G_OBJECT_CLASS (grl_dleyna_server_parent_class)->finalize (object);
}

static void
grl_dleyna_server_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GrlDleynaServer *self = GRL_DLEYNA_SERVER (object);
  GrlDleynaServerPrivate *priv = self->priv;

  switch (prop_id) {
    case PROP_FLAGS:
      g_value_set_flags (value, priv->flags);
      break;

    case PROP_BUS_TYPE:
      g_value_set_enum (value, priv->bus_type);
      break;

    case PROP_WELL_KNOWN_NAME:
      g_value_set_string (value, priv->well_known_name);
      break;

    case PROP_OBJECT_PATH:
      g_value_set_string (value, priv->object_path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
grl_dleyna_server_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GrlDleynaServer *self = GRL_DLEYNA_SERVER (object);
  GrlDleynaServerPrivate *priv = self->priv;

  switch (prop_id) {
    case PROP_FLAGS:
      priv->flags = g_value_get_flags (value);
      break;

    case PROP_BUS_TYPE:
      priv->bus_type = g_value_get_enum (value);
      break;

    case PROP_WELL_KNOWN_NAME:
      priv->well_known_name = g_value_dup_string (value);
      break;

    case PROP_OBJECT_PATH:
      priv->object_path = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
grl_dleyna_server_init (GrlDleynaServer *self)
{
  GrlDleynaServerPrivate *priv;

  self->priv = priv = grl_dleyna_server_get_instance_private (self);
}

static void
grl_dleyna_server_class_init (GrlDleynaServerClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->dispose = grl_dleyna_server_dispose;
  gobject_class->finalize = grl_dleyna_server_finalize;
  gobject_class->get_property = grl_dleyna_server_get_property;
  gobject_class->set_property = grl_dleyna_server_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_FLAGS,
                                   g_param_spec_flags ("flags",
                                                       "Flags",
                                                       "Proxy flags",
                                                       G_TYPE_DBUS_PROXY_FLAGS,
                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                       G_PARAM_READABLE |
                                                       G_PARAM_WRITABLE |
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_BLURB |
                                                       G_PARAM_STATIC_NICK));

  g_object_class_install_property (gobject_class,
                                   PROP_BUS_TYPE,
                                   g_param_spec_enum ("bus-type",
                                                      "Bus Type",
                                                      "The bus to connect to, defaults to the session one",
                                                      G_TYPE_BUS_TYPE,
                                                      G_BUS_TYPE_SESSION,
                                                      G_PARAM_WRITABLE |
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_BLURB |
                                                      G_PARAM_STATIC_NICK));

  g_object_class_install_property (gobject_class,
                                   PROP_WELL_KNOWN_NAME,
                                   g_param_spec_string ("well-known-name",
                                                        "Well-Known Name",
                                                        "The well-known name of the service",
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  g_object_class_install_property (gobject_class,
                                   PROP_OBJECT_PATH,
                                   g_param_spec_string ("object-path",
                                                        "object-path",
                                                        "The object path the proxy is for",
                                                        NULL,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));
}

void
grl_dleyna_server_new_for_bus (GBusType             bus_type,
                               GDBusProxyFlags      flags,
                               const gchar         *well_known_name,
                               const gchar         *object_path,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_async_initable_new_async (GRL_TYPE_DLEYNA_SERVER, G_PRIORITY_DEFAULT, cancellable,
                              callback, user_data, "flags", flags, "bus-type", bus_type,
                              "well-known-name", well_known_name, "object-path", object_path, NULL);
}

/* grl_dleyna_server_init_check_complete:
 *
 * Check that all the async subtasks have completed and return the result.
 *
 * Note that if the caller does not take ownership of the object (eg. by not
 * calling g_task_propagate_pointer() or in case of errors) this may lead to
 * the object being disposed.
 */
static gboolean
grl_dleyna_server_init_check_complete (GrlDleynaServer *self,
                                       GTask           *init_task)
{
  GError *error;

  g_return_val_if_fail (g_task_is_valid (init_task, self), TRUE);

  if (self->priv->init_status != INIT_READY) {
    return FALSE;
  }

  error = g_task_get_task_data (init_task);

  if (error != NULL) {
    g_task_return_error (init_task, error);
  }
  else {
    g_task_return_boolean (init_task, TRUE);
  }

  g_object_unref (init_task);

  return TRUE;
}

static void
grl_dleyna_server_media_device_proxy_new_cb (GObject      *source_object,
                                             GAsyncResult *res,
                                             gpointer      user_data)
{
  GTask *init_task = G_TASK (user_data);
  GrlDleynaServer *self;
  GError *error = NULL;

  self = GRL_DLEYNA_SERVER (g_task_get_source_object (init_task));

  self->priv->init_status |= INIT_DEVICE;
  self->priv->media_device = grl_dleyna_media_device_proxy_new_for_bus_finish (res, &error);

  if (error != NULL) {
    GRL_WARNING ("Unable to load the MediaDevice interface: %s", error->message);
    g_task_set_task_data (init_task, error, (GDestroyNotify) g_error_free);
  }

  grl_dleyna_server_init_check_complete (self, init_task);
}

static void
grl_dleyna_server_media_object2_proxy_new_cb (GObject      *source_object,
                                              GAsyncResult *res,
                                              gpointer      user_data)
{
  GTask *init_task = G_TASK (user_data);
  GrlDleynaServer *self;
  GError *error = NULL;

  self = GRL_DLEYNA_SERVER (g_task_get_source_object (init_task));

  self->priv->init_status |= INIT_OBJECT;
  self->priv->media_object = grl_dleyna_media_object2_proxy_new_for_bus_finish (res, &error);

  if (error != NULL) {
    GRL_WARNING ("Unable to load the MediaObjetc2 interface: %s", error->message);
    g_task_set_task_data (init_task, error, (GDestroyNotify) g_error_free);
  }

  grl_dleyna_server_init_check_complete (self, init_task);
}

static void
grl_dleyna_server_media_container2_proxy_new_cb (GObject      *source_object,
                                                 GAsyncResult *res,
                                                 gpointer      user_data)
{
  GTask *init_task = G_TASK (user_data);
  GrlDleynaServer *self;
  GError *error = NULL;

  self = GRL_DLEYNA_SERVER (g_task_get_source_object (init_task));

  self->priv->init_status |= INIT_CONTAINER;
  self->priv->media_container = grl_dleyna_media_container2_proxy_new_for_bus_finish (res, &error);

  if (error != NULL) {
    GRL_WARNING ("Unable to load the MediaContainer2 interface: %s", error->message);
    g_task_set_task_data (init_task, error, (GDestroyNotify) g_error_free);
  }

  grl_dleyna_server_init_check_complete (self, init_task);
}

static void
grl_dleyna_server_init_async (GAsyncInitable      *initable,
                              int                  io_priority,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  GrlDleynaServer *self = GRL_DLEYNA_SERVER (initable);
  GrlDleynaServerPrivate *priv = self->priv;
  GTask *init_task;

  init_task = g_task_new (initable, cancellable, callback, user_data);
  g_task_set_priority (init_task, io_priority);

  /* Load all three DBus interface proxies asynchronously and then complete
   * the initialization when all of them are ready */
  grl_dleyna_media_device_proxy_new_for_bus (priv->bus_type, priv->flags, priv->well_known_name,
                                             priv->object_path, cancellable,
                                             grl_dleyna_server_media_device_proxy_new_cb, init_task);
  grl_dleyna_media_object2_proxy_new_for_bus (priv->bus_type, priv->flags, priv->well_known_name,
                                              priv->object_path, cancellable,
                                              grl_dleyna_server_media_object2_proxy_new_cb, init_task);
  grl_dleyna_media_container2_proxy_new_for_bus (priv->bus_type, priv->flags, priv->well_known_name,
                                                 priv->object_path, cancellable,
                                                 grl_dleyna_server_media_container2_proxy_new_cb, init_task);
}

static gboolean
grl_dleyna_server_init_finish (GAsyncInitable *initable,
                               GAsyncResult   *result,
                               GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, G_OBJECT (initable)), FALSE);

  return g_task_propagate_pointer (G_TASK (result), error) != NULL;
}

GrlDleynaServer*
grl_dleyna_server_new_for_bus_finish (GAsyncResult *result,
                                      GError      **error)
{
  GObject *object, *source_object;
  GError *err = NULL;

  source_object = g_async_result_get_source_object (result);
  object = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object), result, &err);
  g_object_unref (source_object);

  if (err != NULL) {
    g_clear_object (&object);
    g_propagate_error (error, err);
    return NULL;
  }

  return GRL_DLEYNA_SERVER (object);
}

static void
grl_dleyna_server_async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = grl_dleyna_server_init_async;
  iface->init_finish = grl_dleyna_server_init_finish;
}

const gchar *
grl_dleyna_server_get_object_path (GrlDleynaServer *self)
{
  return self->priv->object_path;
}

GrlDleynaMediaDevice *
grl_dleyna_server_get_media_device (GrlDleynaServer *self)
{
  return self->priv->media_device;
}

GrlDleynaMediaObject2 *
grl_dleyna_server_get_media_object (GrlDleynaServer *self)
{
  return self->priv->media_object;
}

GrlDleynaMediaContainer2 *
grl_dleyna_server_get_media_container (GrlDleynaServer *self)
{
  return self->priv->media_container;
}
