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

#ifndef _FLICKCURL_ASYNC_H_
#define _FLICKCURL_ASYNC_H_

#include <flickcurl.h>
#include <glib.h>

typedef void (*PhotosSearchParamsCb) (flickcurl_photos_list *photos_list,
                                      gpointer user_data);

void photos_search_params_async (flickcurl *fc,
                                 flickcurl_search_params *params,
                                 flickcurl_photos_list_params *list_params,
                                 PhotosSearchParamsCb callback,
                                 gpointer user_data);

#endif /* _FLICKCURL_ASYNC_H_ */
