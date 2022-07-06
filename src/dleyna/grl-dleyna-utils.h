/*
 * Copyright (C) 2014 Giovanni Campagna <scampa.giovanni@gmail.com>
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

#ifndef _GRL_DLEYNA_UTILS_H_
#define _GRL_DLEYNA_UTILS_H_

G_BEGIN_DECLS

void grl_dleyna_util_uri_is_localhost (const gchar *uri,
                                       gboolean *localhost,
                                       gboolean *localuser);

G_END_DECLS

#endif
