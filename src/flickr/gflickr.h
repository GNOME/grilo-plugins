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

#ifndef _G_FLICKR_H_
#define _G_FLICKR_H_

#include <glib-object.h>

#define G_FLICKR_TYPE                           \
  (g_flickr_get_type ())

#define G_FLICKR(obj)                                   \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                   \
                               G_FLICKR_TYPE,           \
                               GFlickr))

#define G_IS_FLICKR(obj)                                \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                   \
                               G_FLICKR_TYPE))

#define G_FLICKR_CLASS(klass)                           \
  (G_TYPE_CHECK_CLASS_CAST((klass),                     \
                           G_FLICKR_TYPE,               \
                           GFlickrClass))

#define G_IS_FLICKR_CLASS(klass)                        \
  (G_TYPE_CHECK_CLASS_TYPE((klass),                     \
                           G_FLICKR_TYPE))

#define G_FLICKR_GET_CLASS(obj)                         \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                    \
                              G_FLICKR_TYPE,            \
                              GFlickrClass))

typedef struct _GFlickr        GFlickr;
typedef struct _GFlickrPrivate GFlickrPrivate;

struct _GFlickr {

  GObject parent;

  /*< private >*/
  GFlickrPrivate *priv;
};

typedef struct _GFlickrClass GFlickrClass;

struct _GFlickrClass {

  GObjectClass parent_class;

};

typedef void (*GFlickrPhotoCb) (GFlickr *f, GHashTable *photo, gpointer user_data);

typedef void (*GFlickrPhotoListCb) (GFlickr *f, GList *photolist, gpointer user_data);

GType g_flickr_get_type (void);

GFlickr *g_flickr_new (const gchar *api_key, const gchar *auth_token, const gchar *auth_secret);

void g_flickr_set_per_page (GFlickr *f, gint per_page);

void
g_flickr_photos_getInfo (GFlickr *f,
                         glong photo_id,
                         GFlickrPhotoCb callback,
                         gpointer user_data);

void
g_flickr_photos_search (GFlickr *f,
                        const gchar *text,
                        gint page,
                        GFlickrPhotoListCb callback,
                        gpointer user_data);

gchar *
g_flickr_photo_url_original (GFlickr *f, GHashTable *photo);

gchar *
g_flickr_photo_url_thumbnail (GFlickr *f, GHashTable *photo);

#endif /* _G_FLICKR_H_ */
