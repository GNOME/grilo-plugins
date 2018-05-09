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

struct AuthCb {
  DMAPConnection *connection;
  SoupSession *session;
  SoupMessage *msg;
  SoupAuth *auth;
};

gchar *
grl_dmap_build_url (DMAPMdnsBrowserService *service)
{
  return g_strdup_printf ("%s://%s:%u",
                          service->service_name,
                          service->host,
                          service->port);
}

void
grl_dmap_auth_cb (DMAPConnection *connection,
                  const char *name,
                  SoupSession *session,
                  SoupMessage *msg,
                  SoupAuth *auth,
                  gboolean retrying,
                  GrlSource *source)
{
  g_return_if_fail (GRL_IS_SOURCE (source));

  struct AuthCb *auth_data = g_new(struct AuthCb, 1);
  auth_data->connection    = connection;
  auth_data->session       = session;
  auth_data->msg           = msg;
  auth_data->auth          = auth;

  grl_source_notify_authenticate (source, auth_data);
}

void grl_dmap_source_continue_with_password (GrlSource *source,
                                             gpointer opaque,
                                             gchar *password)
{
  struct AuthCb *auth = opaque;

  dmap_connection_authenticate_message(auth->connection,
                                       auth->session,
                                       auth->msg,
                                       auth->auth,
                                       password);

  g_warning("%s\n", "authentication complete");

  g_free (auth);
}
