/*
 * Copyright (C) 2010 Igalia S.L.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Xabier Rodriguez Calvar <xrcalvar@igalia.com>
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

#ifndef _GRL_GNOMEVFS_UTIL_H_
#define _GRL_GNOMEVFS_UTIL_H_

#include <glib.h>

typedef void (*GrlUtilGnomeVFSReadDoneCb) (gchar *contents, gpointer user_data);

void grl_plugins_gnome_vfs_read_url_async (const gchar *url,
                                           GrlUtilGnomeVFSReadDoneCb cb,
                                           gpointer user_data);

#endif /* _GRL_GNOMEVFS_UTIL_H_ */
