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

#ifndef GRL_DLEYNA_SERVER_H
#define GRL_DLEYNA_SERVER_H

#include <glib-object.h>
#include <gio/gio.h>

#include "grl-dleyna-proxy-mediadevice.h"
#include "grl-dleyna-proxy-mediaserver2.h"

G_BEGIN_DECLS

#define GRL_TYPE_DLEYNA_SERVER (grl_dleyna_server_get_type ())

#define GRL_DLEYNA_SERVER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   GRL_TYPE_DLEYNA_SERVER, GrlDleynaServer))

#define GRL_DLEYNA_SERVER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   GRL_TYPE_DLEYNA_SERVER, GrlDleynaServerClass))

#define GRL_IS_DLEYNA_SERVER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   GRL_TYPE_DLEYNA_SERVER))

#define GRL_IS_DLEYNA_SERVER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   GRL_TYPE_DLEYNA_SERVER))

#define GRL_DLEYNA_SERVER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   GRL_TYPE_DLEYNA_SERVER, GrlDleynaServerClass))

typedef struct _GrlDleynaServer        GrlDleynaServer;
typedef struct _GrlDleynaServerClass   GrlDleynaServerClass;
typedef struct _GrlDleynaServerPrivate GrlDleynaServerPrivate;

struct _GrlDleynaServer
{
  GObject parent_instance;
  GrlDleynaServerPrivate *priv;
};

struct _GrlDleynaServerClass
{
  GObjectClass parent_class;
};

GType                    grl_dleyna_server_get_type            (void) G_GNUC_CONST;

void                     grl_dleyna_server_new_for_bus         (GBusType            bus_type,
                                                                GDBusProxyFlags     flags,
                                                                const gchar        *name,
                                                                const gchar        *object_path,
                                                                GCancellable       *cancellable,
                                                                GAsyncReadyCallback callback,
                                                                gpointer            user_data);

GrlDleynaServer          *grl_dleyna_server_new_for_bus_finish  (GAsyncResult      *res,
                                                                 GError           **error);

const gchar              *grl_dleyna_server_get_object_path     (GrlDleynaServer   *server);

GrlDleynaMediaDevice     *grl_dleyna_server_get_media_device    (GrlDleynaServer   *server);

GrlDleynaMediaObject2    *grl_dleyna_server_get_media_object    (GrlDleynaServer   *server);

GrlDleynaMediaContainer2 *grl_dleyna_server_get_media_container (GrlDleynaServer   *server);


G_END_DECLS

#endif /* GRL_DLEYNA_SERVER_H */
