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

#ifndef _YOUTUBE_SOURCE_H_
#define _YOUTUBE_SOURCE_H_

#include "../src/media-source.h"

#define YOUTUBE_SOURCE_TYPE   (youtube_source_get_type ())
#define YOUTUBE_SOURCE(obj)  (G_TYPE_CHECK_INSTANCE_CAST ((obj), YOUTUBE_SOURCE_TYPE, YoutubeSource))
#define IS_YOUTUBE_SOURCE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), YOUTUBE_SOURCE_TYPE))
#define YOUTUBE_SOURCE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), YOUTUBE_SOURCE_TYPE,  YoutubeSourceClass))
#define IS_YOUTUBE_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), YOUTUBE_SOURCE_TYPE))
#define YOUTUBE_SOURCE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), YOUTUBE_SOURCE_TYPE, YoutubeSourceClass))

typedef struct _YoutubeSource YoutubeSource;

struct _YoutubeSource {

  MediaSource parent;

};

typedef struct _YoutubeSourceClass YoutubeSourceClass;

struct _YoutubeSourceClass {

  MediaSourceClass parent_class;

};

GType youtube_source_get_type (void);

YoutubeSource *youtube_source_new (void);

#endif
