/*
 * GrlDAAPRecord factory class
 *
 * Copyright (C) 2008 W. Michael Petullo <mike@flyn.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __SIMPLE_DAAP_RECORD_FACTORY
#define __SIMPLE_DAAP_RECORD_FACTORY

#include <libdmapsharing/dmap.h>

G_BEGIN_DECLS

#define TYPE_SIMPLE_DAAP_RECORD_FACTORY         \
  (grl_daap_record_factory_get_type ())

#define SIMPLE_DAAP_RECORD_FACTORY(o)                          \
  (G_TYPE_CHECK_INSTANCE_CAST((o),                             \
                              TYPE_SIMPLE_DAAP_RECORD_FACTORY, \
                              GrlDAAPRecordFactory))

#define SIMPLE_DAAP_RECORD_FACTORY_CLASS(k)                 \
  (G_TYPE_CHECK_CLASS_CAST((k),                             \
                           TYPE_SIMPLE_DAAP_RECORD_FACTORY, \
                           GrlDAAPRecordFactoryClass))

#define IS_SIMPLE_DAAP_RECORD_FACTORY(o)                          \
  (G_TYPE_CHECK_INSTANCE_TYPE((o),                                \
                              TYPE_SIMPLE_DAAP_RECORD_FACTORY))

#define IS_SIMPLE_DAAP_RECORD_FACTORY_CLASS(k)                       \
  (G_TYPE_CHECK_CLASS_TYPE((k),                                      \
                           TYPE_SIMPLE_DAAP_RECORD_FACTORY_CLASS))

#define SIMPLE_DAAP_RECORD_FACTORY_GET_CLASS(o)                \
  (G_TYPE_INSTANCE_GET_CLASS((o),                              \
                             TYPE_SIMPLE_DAAP_RECORD_FACTORY,  \
                             GrlDAAPRecordFactoryClass))

typedef struct GrlDAAPRecordFactoryPrivate GrlDAAPRecordFactoryPrivate;

typedef struct {
  GObject parent;
} GrlDAAPRecordFactory;

typedef struct {
  GObjectClass parent;
} GrlDAAPRecordFactoryClass;

GType                    grl_daap_record_factory_get_type (void);

GrlDAAPRecordFactory *grl_daap_record_factory_new      (void);

DMAPRecord              *grl_daap_record_factory_create   (DMAPRecordFactory *factory, gpointer user_data);

#endif /* __SIMPLE_DAAP_RECORD_FACTORY */

G_END_DECLS
