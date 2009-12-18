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

#ifndef _FAKE_MEDIA_SOURCE_H_
#define _FAKE_MEDIA_SOURCE_H_

#include "../src/ms-media-source.h"

#define FAKE_MEDIA_SOURCE_TYPE   (fake_media_source_get_type ())
#define FAKE_MEDIA_SOURCE(obj)  (G_TYPE_CHECK_INSTANCE_CAST ((obj), FAKE_MEDIA_SOURCE_TYPE, Fakemediasource))
#define IS_FAKE_MEDIA_SOURCE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FAKE_MEDIA_SOURCE_TYPE))
#define FAKE_MEDIA_SOURCE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), FAKE_MEDIA_SOURCE_TYPE,  FakeMediaSourceClass))
#define IS_FAKE_MEDIA_SOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), FAKE_MEDIA_SOURCE_TYPE))
#define FAKE_MEDIA_SOURCE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), FAKE_MEDIA_SOURCE_TYPE, FakeMediaSourceClass))

typedef struct _FakeMediaSource FakeMediaSource;

struct _FakeMediaSource {

  MsMediaSource parent;

};

typedef struct _FakeMediaSourceClass FakeMediaSourceClass;

struct _FakeMediaSourceClass {

  MsMediaSourceClass parent_class;

};

GType fake_media_source_get_type (void);

FakeMediaSource *fake_media_source_new (void);

#endif
