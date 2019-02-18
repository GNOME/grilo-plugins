/*
 * Copyright (C) 2012 Bastien Nocera
 *
 * Contact: Bastien Nocera <hadess@hadess.net>
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

#ifndef _GRL_OPTICAL_MEDIA_SOURCE_H_
#define _GRL_OPTICAL_MEDIA_SOURCE_H_

#include <grilo.h>

#define GRL_OPTICAL_MEDIA_SOURCE_TYPE           \
  (grl_optical_media_source_get_type ())

#define GRL_OPTICAL_MEDIA_SOURCE(obj)                          \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                          \
                               GRL_OPTICAL_MEDIA_SOURCE_TYPE,  \
                               GrlOpticalMediaSource))

#define GRL_IS_OPTICAL_MEDIA_SOURCE(obj)                       \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                          \
                               GRL_OPTICAL_MEDIA_SOURCE_TYPE))

#define GRL_OPTICAL_MEDIA_SOURCE_CLASS(klass)               \
  (G_TYPE_CHECK_CLASS_CAST((klass),                         \
                           GRL_OPTICAL_MEDIA_SOURCE_TYPE,   \
                           GrlOpticalMediaSourceClass))

#define GRL_IS_OPTICAL_MEDIA_SOURCE_CLASS(klass)            \
  (G_TYPE_CHECK_CLASS_TYPE((klass)                          \
                           GRL_OPTICAL_MEDIA_SOURCE_TYPE))

#define GRL_OPTICAL_MEDIA_SOURCE_GET_CLASS(obj)                \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                           \
                              GRL_OPTICAL_MEDIA_SOURCE_TYPE,   \
                              GrlOpticalMediaSourceClass))

/* --- Grilo Configuration --- */
#define GRILO_CONF_IGNORED_SCHEME "ignored-scheme"

typedef struct _GrlOpticalMediaSource GrlOpticalMediaSource;
typedef struct _GrlOpticalMediaSourcePrivate GrlOpticalMediaSourcePrivate;

struct _GrlOpticalMediaSource {

  GrlSource parent;

  /*< private >*/
  GrlOpticalMediaSourcePrivate *priv;
};

typedef struct _GrlOpticalMediaSourceClass GrlOpticalMediaSourceClass;

struct _GrlOpticalMediaSourceClass {

  GrlSourceClass parent_class;

};

GType grl_optical_media_source_get_type (void);

#endif /* _GRL_OPTICAL_MEDIA_SOURCE_H_ */
