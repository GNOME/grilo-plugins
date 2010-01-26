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

#ifndef _MS_YOUTUBE_SOURCE_H_
#define _MS_YOUTUBE_SOURCE_H_

#include <media-store.h>

#define MS_YOUTUBE_SOURCE_TYPE \
  (ms_youtube_source_get_type ())
#define MS_YOUTUBE_SOURCE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MS_YOUTUBE_SOURCE_TYPE, MsYoutubeSource))
#define MS_IS_YOUTUBE_SOURCE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MS_YOUTUBE_SOURCE_TYPE))
#define MS_YOUTUBE_SOURCE_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST((klass), MS_YOUTUBE_SOURCE_TYPE,  MsYoutubeSourceClass))
#define MS_IS_YOUTUBE_SOURCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), MS_YOUTUBE_SOURCE_TYPE))
#define MS_YOUTUBE_SOURCE_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MS_YOUTUBE_SOURCE_TYPE, MsYoutubeSourceClass))

typedef struct _MsYoutubeSource MsYoutubeSource;

struct _MsYoutubeSource {

  MsMediaSource parent;
};

typedef struct _MsYoutubeSourceClass MsYoutubeSourceClass;

struct _MsYoutubeSourceClass {

  MsMediaSourceClass parent_class;

};

GType ms_youtube_source_get_type (void);

#endif
