/*
 * Copyright (C) 2011-2012 Igalia S.L.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Juan A. Suarez Romero <jasuarez@igalia.com>
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

#ifndef _GRL_TRACKER_SOURCE_H_
#define _GRL_TRACKER_SOURCE_H_

#include <grilo.h>
#include <tracker-sparql.h>

#define GRL_TRACKER_SOURCE_TYPE                 \
  (grl_tracker_source_get_type ())

#define GRL_TRACKER_SOURCE(obj)                         \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                   \
                               GRL_TRACKER_SOURCE_TYPE, \
                               GrlTrackerSource))

#define GRL_IS_TRACKER_SOURCE(obj)                      \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                   \
                               GRL_TRACKER_SOURCE_TYPE))

#define GRL_TRACKER_SOURCE_CLASS(klass)                 \
  (G_TYPE_CHECK_CLASS_CAST((klass),                     \
                           GRL_TRACKER_SOURCE_TYPE,     \
                           GrlTrackerSourceClass))

#define GRL_IS_TRACKER_SOURCE_CLASS(klass)              \
  (G_TYPE_CHECK_CLASS_TYPE((klass),                     \
                           GRL_TRACKER_SOURCE_TYPE))

#define GRL_TRACKER_SOURCE_GET_CLASS(obj)               \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                    \
                              GRL_TRACKER_SOURCE_TYPE,  \
                              GrlTrackerSourceClass))

typedef struct _GrlTrackerSource GrlTrackerSource;
typedef struct _GrlTrackerSourcePriv GrlTrackerSourcePriv;

struct _GrlTrackerSource {

  GrlSource parent;

  /*< private >*/
  GrlTrackerSourcePriv *priv;

};

typedef struct _GrlTrackerSourceClass GrlTrackerSourceClass;

struct _GrlTrackerSourceClass {

  GrlSourceClass parent_class;

};

GType grl_tracker_source_get_type (void);

gboolean grl_tracker_source_can_notify (GrlTrackerSource *source);

const gchar *grl_tracker_source_get_tracker_source (GrlTrackerSource *source);

TrackerSparqlConnection *grl_tracker_source_get_tracker_connection (GrlTrackerSource *source);

/**/

void grl_tracker_source_sources_init (void);

void grl_tracker_add_source (GrlTrackerSource *source);

GrlTrackerSource *grl_tracker_source_find (const gchar *id);

GrlTrackerSource *grl_tracker_source_find_source (const gchar *id);

#endif /* _GRL_TRACKER_SOURCE_H_ */
