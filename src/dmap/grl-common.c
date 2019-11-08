/*
 * Copyright (C) 2011 W. Michael Petullo.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <grilo.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <string.h>
#include <stdlib.h>
#include <libdmapsharing/dmap.h>

#include "grl-common.h"

gchar *
grl_dmap_build_url (DmapMdnsService *service)
{
  gchar *url = NULL;
  gchar *service_name, *host;
  guint port;

  g_object_get(service, "service-name", &service_name,
                        "host", &host,
                        "port", &port, NULL);

  url = g_strdup_printf ("%s://%s:%u",
                          service_name,
                          host,
                          port);

  g_free(service_name);
  g_free(host);

  return url;
}
