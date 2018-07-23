/*
 * Copyright (C) 2010-2011 Igalia S.L.
 *
 * Contact: Guillaume Emont <gemont@igalia.com>
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

#ifndef _GRL_LOCAL_METADATA_SOURCE_H_
#define _GRL_LOCAL_METADATA_SOURCE_H_

#include <grilo.h>

#define GRL_LOCAL_METADATA_SOURCE_TYPE           \
  (grl_local_metadata_source_get_type ())

#define GRL_LOCAL_METADATA_SOURCE(obj)                           \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                           \
                               GRL_LOCAL_METADATA_SOURCE_TYPE,   \
                               GrlLocalMetadataSource))

#define GRL_IS_LOCAL_METADATA_SOURCE(obj)                        \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                           \
                               GRL_LOCAL_METADATA_SOURCE_TYPE))

#define GRL_LOCAL_METADATA_SOURCE_CLASS(klass)                   \
  (G_TYPE_CHECK_CLASS_CAST((klass),                             \
                           GRL_LOCAL_METADATA_SOURCE_TYPE,       \
                           GrlLocalMetadataSourceClass))

#define GRL_IS_LOCAL_METADATA_SOURCE_CLASS(klass)                \
  (G_TYPE_CHECK_CLASS_TYPE((klass),                             \
                           GRL_LOCAL_METADATA_SOURCE_TYPE))

#define GRL_LOCAL_METADATA_SOURCE_GET_CLASS(obj)                 \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                            \
                              GRL_LOCAL_METADATA_SOURCE_TYPE,    \
                              GrlLocalMetadataSourceClass))

typedef struct _GrlLocalMetadataSource     GrlLocalMetadataSource;
typedef struct _GrlLocalMetadataSourcePriv GrlLocalMetadataSourcePrivate;

struct _GrlLocalMetadataSource {

  GrlSource parent;

  /*< private >*/
  GrlLocalMetadataSourcePrivate *priv;

};

typedef struct _GrlLocalMetadataSourceClass GrlLocalMetadataSourceClass;

struct _GrlLocalMetadataSourceClass {

  GrlSourceClass parent_class;

};

GType grl_local_metadata_source_get_type (void);

#endif /* _GRL_LOCAL_METADATA_SOURCE_H_ */
