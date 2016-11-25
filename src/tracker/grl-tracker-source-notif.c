/*
 * Copyright (C) 2011-2012 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
 * Copyright (C) 2015 Collabora Ltd.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
 *          Juan A. Suarez Romero <jasuarez@igalia.com>
 *          Xavier Claessens <xavier.claessens@collabora.com>
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

#include <tracker-sparql.h>

#include "grl-tracker.h"
#include "grl-tracker-source-notif.h"
#include "grl-tracker-source-priv.h"
#include "grl-tracker-utils.h"

#define GRL_LOG_DOMAIN_DEFAULT tracker_notif_log_domain
GRL_LOG_DOMAIN_STATIC(tracker_notif_log_domain);

#define GRL_TRACKER_TYPE_SOURCE_NOTIFY grl_tracker_source_notify_get_type ()
G_DECLARE_FINAL_TYPE (GrlTrackerSourceNotify, grl_tracker_source_notify, GRL_TRACKER, SOURCE_NOTIFY, GObject)

struct _GrlTrackerSourceNotify {
  GObject parent;

  GDBusConnection *conn;

  /* subject id -> GrlSourceChangeType */
  GHashTable *updates;
  /* Number of updates being queried */
  guint updates_count;
  /* subject id -> MediaInfo (NULL means it's being queried) */
  GHashTable *cache;

  guint graph_updated_id;
  gint rdf_type_id;
};

static void grl_tracker_source_notify_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GrlTrackerSourceNotify, grl_tracker_source_notify, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, grl_tracker_source_notify_initable_iface_init))

static GrlTrackerSourceNotify *singleton = NULL;

typedef struct {
  gchar *type;
  gchar *datasource;
  gchar *url;
} MediaInfo;

static MediaInfo *
media_info_new (const gchar *type,
                const gchar *datasource,
                const gchar *url)
{
  MediaInfo *info;

  info = g_slice_new (MediaInfo);
  info->type = g_strdup (type);
  info->datasource = g_strdup (datasource);
  info->url = g_strdup (url);

  return info;
}

static void
media_info_free (MediaInfo *info)
{
  if (info == NULL)
    return;

  g_free (info->type);
  g_free (info->datasource);
  g_free (info->url);

  g_slice_free (MediaInfo, info);
}

static void
notify_change (GrlTrackerSourceNotify *self,
               gint                    id,
               GrlSourceChangeType     change_type)
{
  GrlTrackerSource *source = NULL;
  gchar *id_str = NULL;
  GrlMedia *media = NULL;
  MediaInfo *info;

  info = g_hash_table_lookup (self->cache, GINT_TO_POINTER (id));
  if (info == NULL)
    goto out;

  source = grl_tracker_source_find ("");

  if (!source || !grl_tracker_source_can_notify (source))
    goto out;

  id_str = g_strdup_printf ("%i", id);
  media = grl_tracker_build_grilo_media (info->type, GRL_TYPE_FILTER_NONE);
  grl_media_set_id (media, id_str);
  grl_media_set_url (media, info->url);

  GRL_DEBUG ("Notify: source=%s, change_type=%d, url=%s",
             grl_source_get_name (GRL_SOURCE (source)),
             change_type, info->url);

  grl_source_notify_change (GRL_SOURCE (source), media, change_type, FALSE);

out:
  if (change_type == GRL_CONTENT_REMOVED)
    g_hash_table_remove (self->cache, GINT_TO_POINTER (id));
  g_clear_object (&media);
  g_free (id_str);
}

static void
update_query_done (GrlTrackerSourceNotify *self)
{
  GHashTableIter iter;
  gpointer key, value;

  /* If more updates came while we were querying, wait for them so we can
   * aggregate notifications. */
  self->updates_count--;
  if (self->updates_count > 0)
    return;

  g_hash_table_iter_init (&iter, self->updates);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    gint id = GPOINTER_TO_INT (key);
    GrlSourceChangeType change_type = GPOINTER_TO_INT (value);

    notify_change (self, id, change_type);
  }

  g_hash_table_remove_all (self->updates);
}

static void
update_cursor_next_cb (GObject      *source_object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  GrlTrackerSourceNotify *self = user_data;
  TrackerSparqlCursor *cursor = (TrackerSparqlCursor *) source_object;
  const gchar *type;
  const gchar *datasource;
  const gchar *url;
  gint id;
  GError *error = NULL;

  if (!tracker_sparql_cursor_next_finish (cursor, result, &error)) {
    if (error != NULL) {
      GRL_WARNING ("Error: %s", error->message);
      g_clear_error (&error);
    }
    update_query_done (self);
    g_object_unref (self);
    return;
  }

  type = tracker_sparql_cursor_get_string (cursor, 0, NULL);
  id = tracker_sparql_cursor_get_integer (cursor, 1);
  datasource = tracker_sparql_cursor_get_string (cursor, 2, NULL);
  url = tracker_sparql_cursor_get_string (cursor, 3, NULL);

  g_hash_table_insert (self->cache,
                       GINT_TO_POINTER (id),
                       media_info_new (type, datasource, url));

  tracker_sparql_cursor_next_async (cursor,
                                    NULL,
                                    update_cursor_next_cb,
                                    self);
}

static void
update_query_cb (GObject      *source,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  GrlTrackerSourceNotify *self = user_data;
  TrackerSparqlCursor *cursor;
  GError *error = NULL;

  cursor = tracker_sparql_connection_query_finish (grl_tracker_connection,
                                                   result,
                                                   &error);
  if (cursor == NULL) {
    if (error != NULL) {
      GRL_WARNING ("Error: %s", error->message);
      g_clear_error (&error);
    }
    update_query_done (self);
    g_object_unref (self);
    return;
  }

  tracker_sparql_cursor_next_async (cursor,
                                    NULL,
                                    update_cursor_next_cb,
                                    self);
}

#define CHANGED GINT_TO_POINTER (GRL_CONTENT_CHANGED)
#define ADDED   GINT_TO_POINTER (GRL_CONTENT_ADDED)
#define REMOVED GINT_TO_POINTER (GRL_CONTENT_REMOVED)

static void
graph_updated_cb (GDBusConnection *connection,
                  const gchar     *sender_name,
                  const gchar     *object_path,
                  const gchar     *interface_name,
                  const gchar     *signal_name,
                  GVariant        *parameters,
                  gpointer         user_data)

{
  GrlTrackerSourceNotify *self = user_data;
  const gchar *class_name;
  GVariantIter *iter1, *iter2;
  gint graph, subject, predicate, object;
  GString *query;

  g_variant_get (parameters, "(&sa(iiii)a(iiii))", &class_name, &iter1, &iter2);

  GRL_DEBUG ("Tracker update event for class=%s ins=%"G_GSIZE_FORMAT" del=%"G_GSIZE_FORMAT,
             class_name,
             g_variant_iter_n_children (iter2),
             g_variant_iter_n_children (iter1));

  query = g_string_new (NULL);

  /* DELETE */
  while (g_variant_iter_loop (iter1, "(iiii)", &graph, &subject, &predicate, &object)) {
    gpointer key = GINT_TO_POINTER (subject);

    /* If a rdf:type is removed, it probably means everything is going to be
     * removed. The media has been deleted. If some other property is deleted
     * it means the media already existed but changed.
     */

    if (predicate == self->rdf_type_id) {
      g_hash_table_insert (self->updates, key, REMOVED);
    } else if (g_hash_table_lookup (self->updates, key) != REMOVED) {
      g_hash_table_insert (self->updates, key, CHANGED);
    }
  }

  /* UPDATE */
  while (g_variant_iter_loop (iter2, "(iiii)", &graph, &subject, &predicate, &object)) {
    gpointer key = GINT_TO_POINTER (subject);

    /* If a rdf:type is added it means it's a new media, otherwise it's an
     * update of an existing media
     */

    if (predicate == self->rdf_type_id) {
      g_hash_table_insert (self->updates, key, ADDED);
    } else if (g_hash_table_lookup (self->updates, key) != ADDED) {
      g_hash_table_insert (self->updates, key, CHANGED);
    }

    /* If we don't yet have info about this subject query it and add NULL in
     * the table so we won't query it twice. */
    if (!g_hash_table_contains (self->cache, key)) {
      g_string_append_printf (query, "%d,", subject);
      g_hash_table_insert (self->cache, key, NULL);
    }
  }

  self->updates_count++;

  if (query->len > 0) {
    /* Remove trailing coma */
    g_string_truncate (query, query->len - 1);

    g_string_prepend (query,
      "SELECT rdf:type(?urn) tracker:id(?urn) nie:dataSource(?urn) nie:url(?urn) "
      "WHERE { ?urn a nfo:FileDataObject . "
      "FILTER (tracker:id(?urn) IN (");
    g_string_append (query, "))}");

    GRL_DEBUG ("Query: %s", query->str);
    tracker_sparql_connection_query_async (grl_tracker_connection,
                                           query->str,
                                           NULL,
                                           update_query_cb,
                                           g_object_ref (self));
  } else {
    update_query_done (self);
  }

  g_variant_iter_free (iter1);
  g_variant_iter_free (iter2);
  g_string_free (query, TRUE);
}

static gboolean
grl_tracker_source_notify_initable_init (GInitable    *initable,
                                         GCancellable *cancellable,
                                         GError      **error)
{
  GrlTrackerSourceNotify *self = GRL_TRACKER_SOURCE_NOTIFY (initable);
  TrackerSparqlCursor *cursor;

  self->conn = g_bus_get_sync (G_BUS_TYPE_SESSION, cancellable, error);
  if (self->conn == NULL)
    return FALSE;

  self->graph_updated_id = g_dbus_connection_signal_subscribe (
      self->conn,
      TRACKER_DBUS_SERVICE,
      TRACKER_DBUS_INTERFACE_RESOURCES,
      "GraphUpdated",
      TRACKER_DBUS_OBJECT_RESOURCES,
      NULL,
      G_DBUS_SIGNAL_FLAGS_NONE,
      graph_updated_cb,
      self, NULL);

  /* Query tracker to know the id of the rdf:type predicate so we can easily
   * identify them in graph_updated_cb(). */
  cursor = tracker_sparql_connection_query (grl_tracker_connection,
      "select tracker:id(rdf:type) tracker:id(nfo:FileDataObject) {}",
      NULL, error);
  if (cursor == NULL)
    return FALSE;

  if (!tracker_sparql_cursor_next (cursor, NULL, error)) {
    g_object_unref (cursor);
    return FALSE;
  }

  self->rdf_type_id = tracker_sparql_cursor_get_integer (cursor, 0);

  g_object_unref (cursor);

  return TRUE;
}

static void
grl_tracker_source_notify_finalize (GObject *object)
{
  GrlTrackerSourceNotify *self = GRL_TRACKER_SOURCE_NOTIFY (object);

  if (self->conn && self->graph_updated_id)
    g_dbus_connection_signal_unsubscribe (self->conn, self->graph_updated_id);

  g_clear_object (&self->conn);
  g_clear_pointer (&self->updates, g_hash_table_unref);
  g_clear_pointer (&self->cache, g_hash_table_unref);

  G_OBJECT_CLASS (grl_tracker_source_notify_parent_class)->finalize (object);
}

static void
grl_tracker_source_notify_class_init (GrlTrackerSourceNotifyClass *klass)
{
  GObjectClass *object_class = (GObjectClass *) klass;

  GRL_LOG_DOMAIN_INIT (tracker_notif_log_domain, "tracker-notif");
  object_class->finalize = grl_tracker_source_notify_finalize;
}

static void
grl_tracker_source_notify_init (GrlTrackerSourceNotify *self)
{
  self->updates = g_hash_table_new (NULL, NULL);
  self->cache = g_hash_table_new_full (NULL, NULL,
      NULL,
      (GDestroyNotify) media_info_free);
}

static void
grl_tracker_source_notify_initable_iface_init (GInitableIface *iface)
{
  iface->init = grl_tracker_source_notify_initable_init;
}

void
grl_tracker_source_dbus_start_watch (void)
{
  GError *error = NULL;

  if (singleton != NULL)
    return;

  singleton = g_initable_new (GRL_TRACKER_TYPE_SOURCE_NOTIFY, NULL, &error, NULL);
  if (singleton == NULL) {
    GRL_WARNING ("Error: %s", error->message);
    g_clear_error (&error);
  }
}

void
grl_tracker_source_dbus_stop_watch (void)
{
  g_clear_object (&singleton);
}
