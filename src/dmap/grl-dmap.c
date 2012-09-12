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
#include <gio/gio.h>
#include <string.h>
#include <stdlib.h>
#include <libdmapsharing/dmap.h>

#include "grl-dmap.h"
#include "simple-dmap-db.h"
#include "simple-daap-record.h"
#include "simple-daap-record-factory.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT dmap_log_domain
GRL_LOG_DOMAIN_STATIC(dmap_log_domain);

/* --- Plugin information --- */

#define PLUGIN_ID   DMAP_PLUGIN_ID

#define SOURCE_ID_TEMPLATE   "grl-dmap-%s"
#define SOURCE_DESC_TEMPLATE "A source for browsing the DMAP server '%s'"

/* --- Grilo DMAP Private --- */

#define GRL_DMAP_SOURCE_GET_PRIVATE(object)           \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),              \
                               GRL_DMAP_SOURCE_TYPE,  \
                               GrlDmapSourcePrivate))

struct _GrlDmapSourcePrivate {
  DMAPMdnsBrowserService *service;
};

/* --- Data types --- */

typedef struct _ResultCbAndArgs {
  GrlSourceResultCb callback;
  GrlSource *source;
  guint op_id;
  gint code_error;
  GHRFunc predicate;
  gchar *predicate_data;
  guint skip;
  guint count;
  guint remaining;
  gpointer user_data;
} ResultCbAndArgs;

typedef struct _ResultCbAndArgsAndDb {
  ResultCbAndArgs cb;
  SimpleDMAPDb *db;
} ResultCbAndArgsAndDb;

static GrlDmapSource *grl_dmap_source_new (DMAPMdnsBrowserService *service);

static void grl_dmap_source_finalize (GObject *object);

gboolean grl_dmap_plugin_init (GrlRegistry *registry,
                               GrlPlugin *plugin,
                               GList *configs);

static const GList *grl_dmap_source_supported_keys (GrlSource *source);

static void grl_dmap_source_browse (GrlSource *source,
                                    GrlSourceBrowseSpec *bs);

static void grl_dmap_source_search (GrlSource *source,
                                    GrlSourceSearchSpec *ss);


static void service_added_cb (DMAPMdnsBrowser *browser,
                              DMAPMdnsBrowserService *service,
                              GrlPlugin *plugin);

static void service_removed_cb (DMAPMdnsBrowser *browser,
                                const gchar *service_name,
                                GrlPlugin *plugin);

/* ===================== Globals  ======================= */
static DMAPMdnsBrowser *browser;
static GHashTable *connections; // Maps URIs to DBs.
static GHashTable *sources;     // Map DMAP services to Grilo media sources.

/* =================== DMAP Plugin ====================== */

gboolean
grl_dmap_plugin_init (GrlRegistry *registry,
                      GrlPlugin *plugin,
                      GList *configs)
{
  GError *error = NULL;

  GRL_LOG_DOMAIN_INIT (dmap_log_domain, "dmap");

  GRL_DEBUG ("dmap_plugin_init");

  browser     = dmap_mdns_browser_new (DMAP_MDNS_BROWSER_SERVICE_TYPE_DAAP);
  connections = g_hash_table_new (g_str_hash, g_str_equal);
  sources     = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  g_signal_connect (G_OBJECT (browser),
                    "service-added",
                    G_CALLBACK (service_added_cb),
                    (gpointer) plugin);

  g_signal_connect (G_OBJECT (browser),
                    "service-removed",
                    G_CALLBACK (service_removed_cb),
                    (gpointer) plugin);

  dmap_mdns_browser_start (browser, &error);
  if (error) {
    g_warning ("error starting browser. code: %d message: %s",
               error->code,
               error->message);
    return FALSE;
  }

  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_dmap_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== DMAP GObject ====================== */

G_DEFINE_TYPE (GrlDmapSource,
               grl_dmap_source,
               GRL_TYPE_SOURCE);

static GrlDmapSource *
grl_dmap_source_new (DMAPMdnsBrowserService *service)
{
  gchar *source_desc;
  GrlDmapSource *source;

  GRL_DEBUG ("grl_dmap_source_new");
  source_desc = g_strdup_printf (SOURCE_DESC_TEMPLATE, service->name);

  source = g_object_new (GRL_DMAP_SOURCE_TYPE,
                         "source-id",   service->name,
                         "source-name", service->name,
                         "source-desc", source_desc,
                         NULL);

  source->priv->service = service;

  g_free (source_desc);

  return source;
}

static void
grl_dmap_source_class_init (GrlDmapSourceClass * klass)
{
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

  source_class->browse = grl_dmap_source_browse;
  source_class->search = grl_dmap_source_search;
  source_class->supported_keys = grl_dmap_source_supported_keys;

  G_OBJECT_CLASS (source_class)->finalize = grl_dmap_source_finalize;

  g_type_class_add_private (klass, sizeof (GrlDmapSourcePrivate));
}

static void
grl_dmap_source_init (GrlDmapSource *source)
{
  source->priv = GRL_DMAP_SOURCE_GET_PRIVATE (source);
}

static void
grl_dmap_source_finalize (GObject *object)
{
  G_OBJECT_CLASS (grl_dmap_source_parent_class)->finalize (object);
}

/* ======================= Utilities ==================== */

static gchar *
build_url (DMAPMdnsBrowserService *service)
{
  return g_strdup_printf ("%s://%s:%u",
                          service->service_name,
                          service->host,
                          service->port);
}

static void
add_media_from_service (gpointer id,
                        DAAPRecord *record,
                        ResultCbAndArgs *cb)
{
  gchar *id_s   = NULL,
    *title  = NULL,
    *url    = NULL;
  int duration  = 0;
  gboolean has_video;
  GrlMedia *media;

  g_object_get (record,
                "title",
                &title,
                "location",
                &url,
                "has-video",
                &has_video,
                "duration",
                &duration,
                NULL);

  id_s = g_strdup_printf ("%u", GPOINTER_TO_UINT (id));

  if (has_video == TRUE) {
    media = grl_media_video_new ();
  } else {
    media = grl_media_audio_new ();
  }

  grl_media_set_id       (media, id_s);
  grl_media_set_duration (media, duration);

  if (title) {
    grl_media_set_title (media, title);
  }

  if (url) {
    // Replace URL's daap:// with http://.
    url[0] = 'h'; url[1] = 't'; url[2] = 't'; url[3] = 'p';
    grl_media_set_url (media, url);
  }

  g_free (id_s);

  cb->callback (cb->source,
                cb->op_id,
                media,
                --cb->remaining,
                cb->user_data,
                NULL);
}

static void
add_to_hash_table (gpointer key, gpointer value, GHashTable *hash_table)
{
  g_hash_table_insert (hash_table, key, value);
}

static void
add_filtered_media_from_service (ResultCbAndArgsAndDb *cb_and_db)
{
  GHashTable *hash_table;
  hash_table = g_hash_table_new (g_direct_hash, g_direct_equal);

  simple_dmap_db_filtered_foreach (cb_and_db->db,
                                   cb_and_db->cb.skip,
                                   cb_and_db->cb.count,
                                   (GHRFunc) cb_and_db->cb.predicate,
                                   cb_and_db->cb.predicate_data,
                                   (GHFunc) add_to_hash_table,
                                   hash_table);

  cb_and_db->cb.remaining = g_hash_table_size (hash_table);
  if (cb_and_db->cb.remaining > 0) {
    g_hash_table_foreach (hash_table, (GHFunc) add_media_from_service, &cb_and_db->cb);
  } else {
    cb_and_db->cb.callback (cb_and_db->cb.source,
                            cb_and_db->cb.op_id,
                            NULL,
                            0,
                            cb_and_db->cb.user_data,
                            NULL);
  }
  g_hash_table_destroy (hash_table);
  g_free (cb_and_db);
}

static void
connected_cb (DMAPConnection       *connection,
              gboolean              result,
              const char           *reason,
              ResultCbAndArgsAndDb *cb_and_db)
{
  GError *error;

  // NOTE: connection argument is required by API but ignored in this case.
  if (!result) {
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 cb_and_db->cb.code_error,
                                 reason);
    cb_and_db->cb.callback (cb_and_db->cb.source,
                            cb_and_db->cb.op_id,
                            NULL,
                            0,
                            cb_and_db->cb.user_data,
                            error);
    g_error_free (error);
  } else {
    add_filtered_media_from_service (cb_and_db);
  }
}

static void
service_added_cb (DMAPMdnsBrowser *browser,
                  DMAPMdnsBrowserService *service,
                  GrlPlugin *plugin)
{
  GrlRegistry   *registry = grl_registry_get_default ();
  GrlDmapSource *source   = grl_dmap_source_new (service);

  GRL_DEBUG (__FUNCTION__);

  grl_registry_register_source (registry,
                                plugin,
                                GRL_SOURCE (source),
                                NULL);

  g_hash_table_insert (sources, g_strdup (service->name), g_object_ref (source));
}

static void
service_removed_cb (DMAPMdnsBrowser *browser,
                    const gchar *service_name,
                    GrlPlugin *plugin)
{
  GrlRegistry   *registry = grl_registry_get_default ();
  GrlDmapSource *source   = g_hash_table_lookup (sources, service_name);

  GRL_DEBUG (__FUNCTION__);

  if (source) {
    grl_registry_unregister_source (registry, GRL_SOURCE (source), NULL);
    g_hash_table_remove (sources, service_name);
  }
}

static void
grl_dmap_connect (gchar *name, gchar *host, guint port, ResultCbAndArgsAndDb *cb_and_db, DMAPConnectionCallback callback)
{
  DMAPRecordFactory *factory;
  DMAPConnection *connection;

  factory = DMAP_RECORD_FACTORY (simple_daap_record_factory_new ());
  connection = DMAP_CONNECTION (daap_connection_new (name, host, port, DMAP_DB (cb_and_db->db), factory));
  dmap_connection_connect (connection, (DMAPConnectionCallback) callback, cb_and_db);
}

static gboolean
always_true (gpointer key, gpointer value, gpointer user_data)
{
  return TRUE;
}

static gboolean
match (gpointer key, DAAPRecord *record, gpointer user_data)
{
  char *title;
  g_object_get (record, "title", &title, NULL);
  return strstr (title, user_data) != NULL;
}

/* ================== API Implementation ================ */

static const GList *
grl_dmap_source_supported_keys (GrlSource *source)
{
  static GList *keys = NULL;

  GRL_DEBUG (__func__);

  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_DURATION,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_URL,
                                      NULL);
  }
  return keys;
}

static void
grl_dmap_source_browse (GrlSource *source,
                        GrlSourceBrowseSpec *bs)
{
  GrlDmapSource *dmap_source = GRL_DMAP_SOURCE (source);
  gchar *url = build_url (dmap_source->priv->service);

  GRL_DEBUG (__func__);

  ResultCbAndArgsAndDb *cb_and_db;

  cb_and_db = g_new (ResultCbAndArgsAndDb, 1);

  cb_and_db->cb.callback       = bs->callback;
  cb_and_db->cb.source         = bs->source;
  cb_and_db->cb.op_id          = bs->operation_id;
  cb_and_db->cb.code_error     = GRL_CORE_ERROR_BROWSE_FAILED;
  cb_and_db->cb.predicate      = always_true;
  cb_and_db->cb.predicate_data = NULL;
  cb_and_db->cb.skip           = grl_operation_options_get_skip (bs->options);
  cb_and_db->cb.count          = grl_operation_options_get_count (bs->options);
  cb_and_db->cb.user_data      = bs->user_data;

  if ((cb_and_db->db = g_hash_table_lookup (connections, url))) {
    // Just call directly; already connected, already populated database.
    connected_cb (NULL, TRUE, NULL, cb_and_db);
  } else {
    // Connect.
    cb_and_db->db = simple_dmap_db_new ();

    grl_dmap_connect (dmap_source->priv->service->name,
                      dmap_source->priv->service->host,
                      dmap_source->priv->service->port,
                      cb_and_db,
                      (DMAPConnectionCallback) connected_cb);

    g_hash_table_insert (connections, (gpointer) url, cb_and_db->db);
  }

  g_free (url);
}

static void grl_dmap_source_search (GrlSource *source,
                                    GrlSourceSearchSpec *ss)
{
  GrlDmapSource *dmap_source = GRL_DMAP_SOURCE (source);

  ResultCbAndArgsAndDb *cb_and_db;
  DMAPMdnsBrowserService *service = dmap_source->priv->service;
  gchar *url = build_url (service);

  cb_and_db = g_new (ResultCbAndArgsAndDb, 1);

  cb_and_db->cb.callback       = ss->callback;
  cb_and_db->cb.source         = ss->source;
  cb_and_db->cb.op_id          = ss->operation_id;
  cb_and_db->cb.code_error     = GRL_CORE_ERROR_SEARCH_FAILED;
  cb_and_db->cb.predicate      = (GHRFunc) match;
  cb_and_db->cb.predicate_data = ss->text;
  cb_and_db->cb.skip           = grl_operation_options_get_skip (ss->options);
  cb_and_db->cb.count          = grl_operation_options_get_count (ss->options);
  cb_and_db->cb.user_data      = ss->user_data;

  if ((cb_and_db->db = g_hash_table_lookup (connections, url))) {
    // Just call directly; already connected, already populated database.
    connected_cb (NULL, TRUE, NULL, cb_and_db);
  } else {
    // Connect.
    cb_and_db->db = simple_dmap_db_new ();
    grl_dmap_connect (service->name, service->host, service->port, cb_and_db, (DMAPConnectionCallback) connected_cb);
    g_hash_table_insert (connections, url, cb_and_db->db);
  }

  g_free (url);
}
