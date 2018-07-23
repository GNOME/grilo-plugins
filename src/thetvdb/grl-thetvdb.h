/*
 * Copyright (C) 2014 Victor Toso.
 *
 * Contact: Victor Toso <me@victortoso.com>
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

#ifndef _GRL_THETVDB_SOURCE_H_
#define _GRL_THETVDB_SOURCE_H_

#include <grilo.h>

#define GRL_THETVDB_SOURCE_TYPE                         \
  (grl_thetvdb_source_get_type ())

#define GRL_THETVDB_SOURCE(obj)                        \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),                   \
                              GRL_THETVDB_SOURCE_TYPE, \
                              GrlTheTVDBSource))

#define GRL_THETVDB_SOURCE_CLASS(klass)                \
  (G_TYPE_CHECK_CLASS_CAST((klass),                    \
                           GRL_THETVDB_SOURCE_TYPE,    \
                           GrlTheTVDBSourceClass))

#define GRL_IS_THETVDB_SOURCE_CLASS(klass)             \
  (G_TYPE_CHECK_CLASS_TYPE((klass),                    \
                           GRL_THETVDB_SOURCE_TYPE))

#define GRL_THETVDB_SOURCE_GET_CLASS(obj)              \
  (G_TYPE_INSTANCE_GET_CLASS((obj),                    \
                             GRL_THETVDB_SOURCE_TYPE,  \
                             GrlTheTVDBSourceClass))

typedef struct _GrlTheTVDBPrivate GrlTheTVDBSourcePrivate;
typedef struct _GrlTheTVDBSource  GrlTheTVDBSource;

struct _GrlTheTVDBSource {
  GrlSource parent;
  GrlTheTVDBSourcePrivate *priv;
};

typedef struct _GrlTheTVDBSourceClass GrlTheTVDBSourceClass;

struct _GrlTheTVDBSourceClass {
  GrlSourceClass parent_class;
};

GType grl_thetvdb_source_get_type (void);

#endif /* _GRL_THETVDB_SOURCE_H_ */
