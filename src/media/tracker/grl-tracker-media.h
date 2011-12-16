/*
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2011 Igalia S.L.
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

#ifndef _GRL_TRACKER_MEDIA_H_
#define _GRL_TRACKER_MEDIA_H_

#include <grilo.h>
#include <tracker-sparql.h>

#define GRL_TRACKER_MEDIA_TYPE                  \
  (grl_tracker_media_get_type ())

#define GRL_TRACKER_MEDIA(obj)                          \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                   \
                               GRL_TRACKER_MEDIA_TYPE,  \
                               GrlTrackerMedia))

#define GRL_IS_TRACKER_MEDIA(obj)                       \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                   \
                               GRL_TRACKER_MEDIA_TYPE))

#define GRL_TRACKER_MEDIA_CLASS(klass)                  \
  (G_TYPE_CHECK_CLASS_CAST((klass),                     \
                           GRL_TRACKER_MEDIA_TYPE,      \
                           GrlTrackerMediaClass))

#define GRL_IS_TRACKER_MEDIA_CLASS(klass)               \
  (G_TYPE_CHECK_CLASS_TYPE((klass),                     \
                           GRL_TRACKER_MEDIA_TYPE))

#define GRL_TRACKER_MEDIA_GET_CLASS(obj)                \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                    \
                              GRL_TRACKER_MEDIA_TYPE,   \
                              GrlTrackerMediaClass))

typedef struct _GrlTrackerMedia GrlTrackerMedia;
typedef struct _GrlTrackerMediaPriv GrlTrackerMediaPriv;

struct _GrlTrackerMedia {

  GrlMediaSource parent;

  /*< private >*/
  GrlTrackerMediaPriv *priv;

};

typedef struct _GrlTrackerMediaClass GrlTrackerMediaClass;

struct _GrlTrackerMediaClass {

  GrlMediaSourceClass parent_class;

};

GType grl_tracker_media_get_type (void);

gboolean grl_tracker_media_can_notify (GrlTrackerMedia *source);

const gchar *grl_tracker_media_get_tracker_source (GrlTrackerMedia *source);

TrackerSparqlConnection *grl_tracker_media_get_tracker_connection (GrlTrackerMedia *source);

/**/

void grl_tracker_media_sources_init (void);

void grl_tracker_add_source (GrlTrackerMedia *source);

void grl_tracker_del_source (GrlTrackerMedia *source);

GrlTrackerMedia *grl_tracker_media_find (const gchar *id);

GrlTrackerMedia *grl_tracker_media_find_source (const gchar *id);

#endif /* _GRL_TRACKER_MEDIA_H_ */
