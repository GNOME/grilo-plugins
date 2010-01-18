/*
 * Copyright (C) 2010 Igalia S.L.
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

#ifndef _MS_LASTFM_ALBUMART_SOURCE_H_
#define _MS_LASTFM_ALBUMART_SOURCE_H_

#include <media-store.h>

#define MS_LASTFM_ALBUMART_SOURCE_TYPE          \
  (ms_lastfm_albumart_source_get_type ())
#define MS_LASTFM_ALBUMART_SOURCE(obj)                                  \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MS_LASTFM_ALBUMART_SOURCE_TYPE, MsLastfmAlbumartSource))
#define MS_IS_LASTFM_ALBUMART_SOURCE(obj)                               \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MS_LASTFM_ALBUMART_SOURCE_TYPE))
#define MS_LASTFM_ALBUMART_SOURCE_CLASS(klass)                          \
  (G_TYPE_CHECK_CLASS_CAST((klass), MS_LASTFM_ALBUMART_SOURCE_TYPE,  MsLastfmAlbumartSourceClass))
#define MS_IS_LASTFM_ALBUMART_SOURCE_CLASS(klass)                       \
  (G_TYPE_CHECK_CLASS_TYPE((klass), MS_LASTFM_ALBUMART_SOURCE_TYPE))
#define MS_LASTFM_ALBUMART_SOURCE_GET_CLASS(obj)                        \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MS_LASTFM_ALBUMART_SOURCE_TYPE, MsLastfmAlbumartSourceClass))

typedef struct _MsLastfmAlbumartSource MsLastfmAlbumartSource;

struct _MsLastfmAlbumartSource {

  MsMetadataSource parent;

};

typedef struct _MsLastfmAlbumartSourceClass MsLastfmAlbumartSourceClass;

struct _MsLastfmAlbumartSourceClass {

  MsMetadataSourceClass parent_class;

};

GType ms_lastfm_albumart_source_get_type (void);

#endif /* _MS_LASTFM_ALBUMART_SOURCE_H_ */
