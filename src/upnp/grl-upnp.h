/*
 * Copyright (C) 2010 Igalia S.L.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
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

#ifndef _GRL_UPNP_SOURCE_H_
#define _GRL_UPNP_SOURCE_H_

#include <grilo.h>

#define GRL_UPNP_SOURCE_TYPE                    \
  (grl_upnp_source_get_type ())

#define GRL_UPNP_SOURCE(obj)                            \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                   \
                               GRL_UPNP_SOURCE_TYPE,    \
                               GrlUpnpSource))

#define GRL_IS_UPNP_SOURCE(obj)                         \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                   \
                               GRL_UPNP_SOURCE_TYPE))

#define GRL_UPNP_SOURCE_CLASS(klass)                    \
  (G_TYPE_CHECK_CLASS_CAST((klass),                     \
                           GRL_UPNP_SOURCE_TYPE,        \
                           GrlUpnpSourceClass))

#define GRL_IS_UPNP_SOURCE_CLASS(klass)                 \
  (G_TYPE_CHECK_CLASS_TYPE((klass),                     \
                           GRL_UPNP_SOURCE_TYPE))

#define GRL_UPNP_SOURCE_GET_CLASS(obj)                  \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                    \
                              GRL_UPNP_SOURCE_TYPE,     \
                              GrlUpnpSourceClass))

typedef struct _GrlUpnpPrivate GrlUpnpPrivate;
typedef struct _GrlUpnpSource  GrlUpnpSource;

struct _GrlUpnpSource {

  GrlMediaSource parent;

  /*< private >*/
  GrlUpnpPrivate *priv;
};

typedef struct _GrlUpnpSourceClass GrlUpnpSourceClass;

struct _GrlUpnpSourceClass {

  GrlMediaSourceClass parent_class;

};

GType grl_upnp_source_get_type (void);

#endif /* _GRL_UPNP_SOURCE_H_ */
