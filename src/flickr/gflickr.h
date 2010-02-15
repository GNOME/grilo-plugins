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

#include <glib.h>

typedef void (*GFlickrPhotoCb) (gpointer f, GHashTable *photo, gpointer user_data);

typedef void (*GFlickrPhotoListCb) (gpointer f, GList *photolist, gpointer user_data);

void
g_flickr_photos_getInfo (gpointer f,
                         glong photo_id,
                         GFlickrPhotoCb callback,
                         gpointer user_data);

void
g_flickr_photos_search (gpointer f,
                        const gchar *text,
                        GFlickrPhotoListCb callback,
                        gpointer user_data);

gchar *
g_flickr_photo_url_original (gpointer f, GHashTable *photo);

gchar *
g_flickr_photo_url_thumbnail (gpointer f, GHashTable *photo);

#endif /* _G_FLICKR_H_ */
