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

#ifndef _GRL_FLICKR_AUTH_SOURCE_H_
#define _GRL_FLICKR_AUTH_SOURCE_H_

#include <glib.h>

typedef enum {
  FLICKR_PERM_READ,
  FLICKR_PERM_WRITE,
  FLICKR_PERM_DELETE
} FlickrPerm;

gchar * grl_flickr_get_frob (const gchar *api_key,
                             const gchar *secret);

gchar *grl_flickr_get_login_link (const gchar *api_key,
                                  const gchar *secret,
                                  const gchar *frob,
                                  FlickrPerm perm);

gchar *grl_flickr_get_token (const gchar *api_key,
                             const gchar *secret,
                             const gchar *frob);

#endif /* _GRL_FLICKR_AUTH_SOURCE_H_ */
