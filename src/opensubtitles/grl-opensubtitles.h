/*
 * Copyright (C) 2014 Bastien Nocera <hadess@hadess.net>
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

#ifndef _GRL_OPENSUBTITLES_SOURCE_H_
#define _GRL_OPENSUBTITLES_SOURCE_H_

#include <grilo.h>

#define GRL_OPENSUBTITLES_SOURCE_TYPE           \
  (grl_opensubtitles_source_get_type ())

#define GRL_OPENSUBTITLES_SOURCE(obj)                           \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                           \
                               GRL_OPENSUBTITLES_SOURCE_TYPE,   \
                               GrlOpenSubtitlesSource))

#define GRL_IS_OPENSUBTITLES_SOURCE(obj)                        \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                           \
                               GRL_OPENSUBTITLES_SOURCE_TYPE))

#define GRL_OPENSUBTITLES_SOURCE_CLASS(klass)                   \
  (G_TYPE_CHECK_CLASS_CAST((klass),                             \
                           GRL_OPENSUBTITLES_SOURCE_TYPE,       \
                           GrlOpenSubtitlesSourceClass))

#define GRL_IS_OPENSUBTITLES_SOURCE_CLASS(klass)                \
  (G_TYPE_CHECK_CLASS_TYPE((klass),                             \
                           GRL_OPENSUBTITLES_SOURCE_TYPE))

#define GRL_OPENSUBTITLES_SOURCE_GET_CLASS(obj)                 \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                            \
                              GRL_OPENSUBTITLES_SOURCE_TYPE,    \
                              GrlOpenSubtitlesSourceClass))

typedef struct _GrlOpenSubtitlesSource     GrlOpenSubtitlesSource;
typedef struct _GrlOpenSubtitlesSourcePriv GrlOpenSubtitlesSourcePrivate;

struct _GrlOpenSubtitlesSource {

  GrlSource parent;

  /*< private >*/
  GrlOpenSubtitlesSourcePrivate *priv;

};

typedef struct _GrlOpenSubtitlesSourceClass GrlOpenSubtitlesSourceClass;

struct _GrlOpenSubtitlesSourceClass {

  GrlSourceClass parent_class;

};

GType grl_opensubtitles_source_get_type (void);

#endif /* _GRL_OPENSUBTITLES_SOURCE_H_ */
