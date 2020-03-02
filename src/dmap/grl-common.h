/*
 * Copyright (C) 2011 W. Michael Petullo
 * Copyright (C) 2012 Igalia S.L.
 *
 * Contact: W. Michael Petullo <mike@flyn.org>
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

#ifndef _GRL_COMMON_H_
#define _GRL_COMMON_H_

typedef struct {
  GrlSourceResultCb callback;
  GrlSource *source;
  GrlMedia *container;
  guint op_id;
  GHRFunc predicate;
  gchar *predicate_data;
  guint skip;
  guint count;
  gpointer user_data;
} ResultCbAndArgs;

typedef struct {
  ResultCbAndArgs cb;
  DmapDb *db;
} ResultCbAndArgsAndDb;

gchar *grl_dmap_build_url (DmapMdnsService *service);

#endif /* _GRL_COMMON_H_ */
