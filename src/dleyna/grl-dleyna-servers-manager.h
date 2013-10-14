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

#ifndef GRL_DLEYNA_SERVERS_MANAGER_H
#define GRL_DLEYNA_SERVERS_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GRL_TYPE_DLEYNA_SERVERS_MANAGER (grl_dleyna_servers_manager_get_type ())

#define GRL_DLEYNA_SERVERS_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
   GRL_TYPE_DLEYNA_SERVERS_MANAGER, GrlDleynaServersManager))

#define GRL_DLEYNA_SERVERS_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
   GRL_TYPE_DLEYNA_SERVERS_MANAGER, GrlDleynaServersManagerClass))

#define GRL_IS_DLEYNA_SERVERS_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
   GRL_TYPE_DLEYNA_SERVERS_MANAGER))

#define GRL_IS_DLEYNA_SERVERS_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
   GRL_TYPE_DLEYNA_SERVERS_MANAGER))

#define GRL_DLEYNA_SERVERS_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
   GRL_TYPE_DLEYNA_SERVERS_MANAGER, GrlDleynaServersManagerClass))

typedef struct _GrlDleynaServersManager        GrlDleynaServersManager;
typedef struct _GrlDleynaServersManagerClass   GrlDleynaServersManagerClass;
typedef struct _GrlDleynaServersManagerPrivate GrlDleynaServersManagerPrivate;

struct _GrlDleynaServersManager
{
  GObject parent_instance;
  GrlDleynaServersManagerPrivate *priv;
};

struct _GrlDleynaServersManagerClass
{
  GObjectClass parent_class;
};

GType                    grl_dleyna_servers_manager_get_type      (void) G_GNUC_CONST;

GrlDleynaServersManager *grl_dleyna_servers_manager_dup_singleton (void);

gboolean                 grl_dleyna_servers_manager_is_available  (void);

G_END_DECLS

#endif /* GRL_DLEYNA_SERVERS_MANAGER_H */
