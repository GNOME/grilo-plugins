/*
 * Copyright (C) 2010, 2011 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
 * Copyright (C) 2013 Intel Corporation.
 *
 * This component is based on the grl-upnp source code.
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

#include "config.h"

#include <grilo.h>
#include <glib/gi18n-lib.h>

#include "grl-dleyna-source.h"
#include "grl-dleyna-servers-manager.h"

#define GRL_LOG_DOMAIN_DEFAULT dleyna_log_domain
GRL_LOG_DOMAIN(dleyna_log_domain);

/* Globals */
static GrlDleynaServersManager *servers = NULL;


static void
server_found_cb (GrlDleynaServersManager *serversmgr,
                 GrlDleynaServer         *server,
                 gpointer                *user_data)
{
  GrlPlugin *plugin = GRL_PLUGIN (user_data);
  GrlDleynaMediaDevice *device;
  GrlSource *source;
  GrlRegistry *registry;
  GError *error = NULL;

  GRL_DEBUG (G_STRFUNC);
  device = grl_dleyna_server_get_media_device (server);
  GRL_DEBUG ("%s udn: %s ", G_STRFUNC, grl_dleyna_media_device_get_udn (device));

  registry = grl_registry_get_default ();

  source = GRL_SOURCE (grl_dleyna_source_new (server));
  GRL_DEBUG ("%s id: %s ", G_STRFUNC, grl_source_get_id (source));
  grl_registry_register_source (registry, plugin, GRL_SOURCE (source), &error);

  if (error != NULL) {
    GRL_WARNING ("Failed to register source for DLNA device %s: %s",
                 grl_dleyna_media_device_get_udn (device), error->message);
    g_error_free (error);
  }
}

static void
server_lost_cb (GrlDleynaServersManager *serversmgr,
                GrlDleynaServer         *server,
                gpointer                *user_data)
{
  GrlDleynaMediaDevice *device;
  GrlSource *source;
  GrlRegistry *registry;
  const gchar* udn;
  gchar *source_id;

  GRL_DEBUG (G_STRFUNC);
  device = grl_dleyna_server_get_media_device (server);
  udn = grl_dleyna_media_device_get_udn (device);
  GRL_DEBUG ("%s udn: %s ", G_STRFUNC, udn);

  registry = grl_registry_get_default ();
  source_id = grl_dleyna_source_build_id (udn);

  GRL_DEBUG ("%s id: %s ", G_STRFUNC, source_id);

  source = grl_registry_lookup_source (registry, source_id);
  if (source != NULL) {
    GError *error = NULL;
    GRL_DEBUG ("%s unregistered %s", G_STRFUNC, source_id);
    grl_registry_unregister_source (registry, source, &error);
    if (error != NULL) {
      GRL_WARNING ("Failed to unregister source %s: %s", udn, error->message);
      g_error_free (error);
    }
  }

  g_free (source_id);
}

static gboolean
grl_dleyna_plugin_init (GrlRegistry *registry,
                        GrlPlugin *plugin,
                        GList *configs)
{
  GRL_LOG_DOMAIN_INIT (dleyna_log_domain, "dleyna");

  GRL_DEBUG (G_STRFUNC);

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  servers = grl_dleyna_servers_manager_dup_singleton ();
  g_signal_connect_object (servers, "server-found", G_CALLBACK (server_found_cb), plugin, 0);
  g_signal_connect_object (servers, "server-lost", G_CALLBACK (server_lost_cb), plugin, 0);

  /* Not immensely useful, since most of the errors will be detected only when
   * the underlying async DBus calls will complete. */
  return grl_dleyna_servers_manager_is_available ();
}

static void
grl_dleyna_plugin_deinit (GrlPlugin *plugin)
{
  GRL_DEBUG (G_STRFUNC);

  g_clear_object (&servers);
}

GRL_PLUGIN_DEFINE (GRL_MAJOR,
                   GRL_MINOR,
                   DLEYNA_PLUGIN_ID,
                   "dLeyna",
                   "A plugin for browsing DLNA servers",
                   "Intel Corp.",
                   VERSION,
                   "LGPL-2.1-or-later",
                   "https://01.org/dleyna",
                   grl_dleyna_plugin_init,
                   grl_dleyna_plugin_deinit,
                   NULL);
