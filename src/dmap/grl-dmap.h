/*
 * Copyright (C) 2011 W. Michael Petullo
 * Copyright (C) 2012 Igalia S.L.
 *
 * Contact: W. Michael Petullo <mike@flyn.org>
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

#ifndef _GRL_DMAP_SOURCE_H_
#define _GRL_DMAP_SOURCE_H_

#include <grilo.h>

#define GRL_DMAP_SOURCE_TYPE                    \
  (grl_dmap_source_get_type ())

#define GRL_DMAP_SOURCE(obj)                          \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                 \
                               GRL_DMAP_SOURCE_TYPE,  \
                               GrlDmapSource))

#define GRL_IS_DMAP_SOURCE(obj)                       \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                 \
                               GRL_DMAP_SOURCE_TYPE))

#define GRL_DMAP_SOURCE_CLASS(klass)               \
  (G_TYPE_CHECK_CLASS_CAST((klass),                \
                           GRL_DMAP_SOURCE_TYPE,   \
                           GrlDmapSourceClass))

#define GRL_IS_DMAP_SOURCE_CLASS(klass)            \
  (G_TYPE_CHECK_CLASS_TYPE((klass),                \
                           GRL_DMAP_SOURCE_TYPE))

#define GRL_DMAP_SOURCE_GET_CLASS(obj)                \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                  \
                              GRL_DMAP_SOURCE_TYPE,   \
                              GrlDmapSourceClass))

typedef struct _GrlDmapSourcePrivate GrlDmapSourcePrivate;
typedef struct _GrlDmapSource  GrlDmapSource;

struct _GrlDmapSource {

  GrlSource parent;

  /*< private >*/
  GrlDmapSourcePrivate *priv;
};

typedef struct _GrlDmapSourceClass GrlDmapSourceClass;

struct _GrlDmapSourceClass {

  GrlSourceClass parent_class;

};

GType grl_dmap_source_get_type (void);

#endif /* _GRL_DMAP_SOURCE_H_ */
