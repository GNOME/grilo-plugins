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

#ifndef _GRL_DPAP_SOURCE_H_
#define _GRL_DPAP_SOURCE_H_

#include <grilo.h>

#include "grl-dpap-compat.h"

#define GRL_DPAP_SOURCE_TYPE (grl_dpap_source_get_type ())

#define GRL_DPAP_SOURCE(obj)                                                   \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                          \
                                GRL_DPAP_SOURCE_TYPE,                          \
                                GrlDpapSource))

#define GRL_IS_DPAP_SOURCE(obj)                                                \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                          \
                                GRL_DPAP_SOURCE_TYPE))

#define GRL_DPAP_SOURCE_CLASS(klass)                                           \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                                           \
                             GRL_DPAP_SOURCE_TYPE,                             \
                             GrlDpapSourceClass))

#define GRL_IS_DPAP_SOURCE_CLASS(klass)                                        \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                                           \
                             GRL_DPAP_SOURCE_TYPE))

#define GRL_DPAP_SOURCE_GET_CLASS(obj)                                         \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                                           \
                               GRL_DPAP_SOURCE_TYPE,                           \
                               GrlDpapSourceClass))

typedef struct _GrlDpapSourcePrivate GrlDpapSourcePrivate;
typedef struct _GrlDpapSource  GrlDpapSource;

struct _GrlDpapSource {
  GrlSource parent;

  /*< private >*/
  GrlDpapSourcePrivate *priv;
};

typedef struct _GrlDpapSourceClass GrlDpapSourceClass;

struct _GrlDpapSourceClass {
  GrlSourceClass parent_class;
};

GType grl_dpap_source_get_type (void);

#endif /* _GRL_DPAP_SOURCE_H_ */
