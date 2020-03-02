/*
 * Copyright (C) 2019 W. Michael Petullo
 * Copyright (C) 2019 Igalia S.L.
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

#ifndef _GRL_DMAP_COMPAT_H_
#define _GRL_DMAP_COMPAT_H_

#ifdef LIBDMAPSHARING_COMPAT

/* Building against libdmapsharing 3 API. */

#define DmapConnection DMAPConnection
#define DmapConnectionFunc DMAPConnectionCallback
#define dmap_connection_start dmap_connection_connect
#define DmapDb DMAPDb
#define DmapDbInterface DMAPDbIface
#define DmapIdRecordFunc GHFunc
#define DmapMdnsBrowser DMAPMdnsBrowser
#define DmapMdnsService DMAPMdnsBrowserService
#define DMAP_MDNS_SERVICE_TYPE_DAAP DMAP_MDNS_BROWSER_SERVICE_TYPE_DAAP
#define DMAP_MDNS_SERVICE_TYPE_DPAP DMAP_MDNS_BROWSER_SERVICE_TYPE_DPAP
#define DmapRecord DMAPRecord
#define DmapRecordFactory DMAPRecordFactory
#define DmapRecordFactoryInterface DMAPRecordFactoryIface
#define DmapRecordInterface DMAPRecordIface

static inline gchar *
grl_dmap_service_get_name (DmapMdnsService *service)
{
  return g_strdup (service->name);
}

static inline gchar *
grl_dmap_service_get_service_name (DmapMdnsService *service)
{
  return g_strdup (service->service_name);
}

static inline gchar *
grl_dmap_service_get_host (DmapMdnsService *service)
{
  return g_strdup (service->host);
}

static inline guint
grl_dmap_service_get_port (DmapMdnsService *service)
{
  return service->port;
}

#else

/* Building against libdmapsharing 4 API. */

static inline gchar *
grl_dmap_service_get_name (DmapMdnsService *service)
{
  gchar *name;
  g_object_get (service, "name", &name, NULL);
  return name;
}

static inline gchar *
grl_dmap_service_get_service_name (DmapMdnsService *service)
{
  gchar *service_name;
  g_object_get (service, "service-name", &service_name, NULL);
  return service_name;
}

static inline gchar *
grl_dmap_service_get_host (DmapMdnsService *service)
{
  gchar *host;
  g_object_get (service, "host", &host, NULL);
  return host;
}

static inline guint
grl_dmap_service_get_port (DmapMdnsService *service)
{
  guint port;
  g_object_get (service, "port", &port, NULL);
  return port;
}

#endif

#endif /* _GRL_DMAP_COMPAT_H_ */
