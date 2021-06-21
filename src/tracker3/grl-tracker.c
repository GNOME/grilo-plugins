/*
 * Copyright (C) 2011-2012 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <grilo.h>
#include <string.h>
#include <libtracker-sparql/tracker-sparql.h>

#include "grl-tracker.h"
#include "grl-tracker-source.h"
#include "grl-tracker-source-api.h"
#include "grl-tracker-source-notif.h"
#include "grl-tracker-utils.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT tracker_general_log_domain
GRL_LOG_DOMAIN_STATIC(tracker_general_log_domain);

/* --- Other --- */

gboolean grl_tracker3_plugin_init (GrlRegistry *registry,
                                   GrlPlugin *plugin,
                                   GList *configs);

/* ===================== Globals  ================= */

TrackerSparqlConnection *grl_tracker_connection = NULL;
GrlPlugin *grl_tracker_plugin;
GCancellable *grl_tracker_plugin_init_cancel = NULL;

/* tracker plugin config */
gchar *grl_tracker_store_path = NULL;
gchar *grl_tracker_miner_service = NULL;

/* =================== Tracker Plugin  =============== */

static void
init_sources (void)
{
  grl_tracker_setup_key_mappings ();

  if (grl_tracker_connection != NULL)
    grl_tracker_source_sources_init ();
}

static void
tracker_new_connection_cb (GObject      *object,
                           GAsyncResult *res,
                           GrlPlugin    *plugin)
{
  GError *error = NULL;

  GRL_DEBUG ("%s", __FUNCTION__);

  grl_tracker_connection = tracker_sparql_connection_new_finish (res, &error);

  if (error) {
    GRL_INFO ("Could not get connection to Tracker: %s", error->message);
    g_error_free (error);
    return;
  }

  init_sources ();
}

static void
set_miner_service (void)
{
  g_autoptr(GKeyFile) keyfile = NULL;
  const char *value;

  if (!g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS))
    return;

  keyfile = g_key_file_new ();
  if (!g_key_file_load_from_file (keyfile, "/.flatpak-info", G_KEY_FILE_NONE, NULL))
    return;

  value = g_key_file_get_value (keyfile, "Policy Tracker3", "dbus:org.freedesktop.Tracker3.Miner.Files", NULL);
  if (value)
    return;

  value = g_key_file_get_string (keyfile, "Application", "name", NULL);
  grl_tracker_miner_service = g_strdup_printf ("%s.Tracker3.Miner.Files", value);
  GRL_INFO("\tRunning in sandboxed mode, using %s as miner service",
           grl_tracker_miner_service);
}

gboolean
grl_tracker3_plugin_init (GrlRegistry *registry,
                          GrlPlugin *plugin,
                          GList *configs)
{
  GrlConfig *config;
  gint config_count;
  GFile *store = NULL, *ontology;
  TrackerSparqlConnectionFlags flags = TRACKER_SPARQL_CONNECTION_FLAGS_NONE;

  GRL_LOG_DOMAIN_INIT (tracker_general_log_domain, "tracker3-general");

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  grl_tracker_source_init_requests ();

  grl_tracker_plugin = plugin;

  if (!configs) {
    GRL_INFO ("\tConfiguration not provided! Using default configuration.");
  } else {
    config_count = g_list_length (configs);
    if (config_count > 1) {
      GRL_INFO ("\tProvided %i configs, but will only use one", config_count);
    }

    config = GRL_CONFIG (configs->data);

    grl_tracker_store_path =
      grl_config_get_string (config, "store-path");
    grl_tracker_miner_service =
      grl_config_get_string (config, "miner-service");
  }

  if (!grl_tracker_miner_service)
    set_miner_service ();

  grl_tracker_plugin_init_cancel = g_cancellable_new ();
  if (grl_tracker_store_path) {
    store = g_file_new_for_path (grl_tracker_store_path);
    flags = TRACKER_SPARQL_CONNECTION_FLAGS_READONLY;
  }

  ontology = tracker_sparql_get_ontology_nepomuk ();
  tracker_sparql_connection_new_async (flags,
                                       store,
                                       ontology,
                                       grl_tracker_plugin_init_cancel,
                                       (GAsyncReadyCallback) tracker_new_connection_cb,
                                       plugin);
  g_clear_object (&store);
  g_object_unref (ontology);
  return TRUE;
}

static void
grl_tracker3_plugin_deinit (GrlPlugin *plugin)
{
  g_cancellable_cancel (grl_tracker_plugin_init_cancel);
  g_clear_object (&grl_tracker_plugin_init_cancel);
  g_clear_object (&grl_tracker_connection);
}

static void
grl_tracker3_plugin_register_keys (GrlRegistry *registry,
                                   GrlPlugin   *plugin)
{
  grl_registry_register_metadata_key (grl_registry_get_default (),
                                      g_param_spec_string ("tracker-category",
                                                           "Tracker category",
                                                           "Category a media belongs to",
                                                           NULL,
                                                           G_PARAM_STATIC_STRINGS |
                                                           G_PARAM_READWRITE),
                                      GRL_METADATA_KEY_INVALID,
                                      NULL);
  grl_registry_register_metadata_key (grl_registry_get_default (),
                                      g_param_spec_string ("gibest-hash",
                                                           "Gibest hash",
                                                           "Gibest hash of the video file",
                                                           NULL,
                                                           G_PARAM_STATIC_STRINGS |
                                                           G_PARAM_READWRITE),
                                      GRL_METADATA_KEY_INVALID,
                                      NULL);
  grl_registry_register_metadata_key (grl_registry_get_default (),
                                      g_param_spec_string ("tracker-urn",
                                                           "Tracker URN",
                                                           "Universal resource number in Tracker's store",
                                                           NULL,
                                                           G_PARAM_STATIC_STRINGS |
                                                           G_PARAM_READWRITE),
                                      GRL_METADATA_KEY_INVALID,
                                      NULL);
}

GRL_PLUGIN_DEFINE (GRL_MAJOR,
                   GRL_MINOR,
                   GRL_TRACKER_PLUGIN_ID,
                   "Tracker3",
                   "A plugin for searching multimedia content using Tracker Miners 3.x",
                   "Igalia S.L.",
                   VERSION,
                   "LGPL-2.1-or-later",
                   "http://www.igalia.com",
                   grl_tracker3_plugin_init,
                   grl_tracker3_plugin_deinit,
                   grl_tracker3_plugin_register_keys);
