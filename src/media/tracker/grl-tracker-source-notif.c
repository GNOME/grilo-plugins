/*
 * Copyright (C) 2011-2012 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
 *          Juan A. Suarez Romero <jasuarez@igalia.com>
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

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT tracker_notif_log_domain
GRL_LOG_DOMAIN_STATIC(tracker_notif_log_domain);

/* ------- Definitions ------- */

#define TRACKER_MEDIA_ITEM                                              \
  "SELECT rdf:type(?urn) tracker:id(?urn) nie:dataSource(?urn) "        \
  "WHERE { ?urn a nfo:FileDataObject . "                                \
  "FILTER (tracker:id(?urn) IN (%s)) }"

/**/

typedef struct {
  /* tables of items for which we know the source */
  GHashTable *inserted_items;
  GHashTable *deleted_items;
  GHashTable *updated_items;

  /* table of items for which we don't know the source */
  GHashTable *orphan_items;

  /* List of new/old sources */
  GList *new_sources;
  GList *old_sources;

  /* Convenient stuff (for tracker/own callbacks...) */
  TrackerSparqlCursor  *cursor;
  gboolean             in_use;
  GrlSourceChangeType  change_type;
} tracker_evt_update_t;

/**/

static guint tracker_dbus_signal_id = 0;

/**/
static tracker_evt_update_t *
tracker_evt_update_new (void)
{
  tracker_evt_update_t *evt = g_slice_new0 (tracker_evt_update_t);

  evt->inserted_items = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
  evt->deleted_items = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
  evt->updated_items = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);

  evt->orphan_items = g_hash_table_new (g_direct_hash, g_direct_equal);

  return evt;
}

static void
tracker_evt_update_free (tracker_evt_update_t *evt)
{
  if (!evt)
    return;

  GRL_DEBUG ("free evt=%p", evt);

  g_hash_table_destroy (evt->inserted_items);
  g_hash_table_destroy (evt->deleted_items);
  g_hash_table_destroy (evt->updated_items);

  g_hash_table_destroy (evt->orphan_items);

  g_list_free (evt->new_sources);
  g_list_free (evt->old_sources);

  g_slice_free (tracker_evt_update_t, evt);
}

static void
tracker_evt_update_source_add (tracker_evt_update_t *evt,
                               const gchar *id,
                               const gchar *source_name)
{
  GrlTrackerSource *source;
  GrlTrackerSourcePriv *priv;

  source = g_hash_table_lookup (grl_tracker_source_sources_modified, id);
  if (!source) {
    source = g_object_new (GRL_TRACKER_SOURCE_TYPE,
			   "source-id", id,
			   "source-name", source_name,
			   "source-desc", GRL_TRACKER_SOURCE_DESC,
			   "tracker-connection", grl_tracker_connection,
                           "tracker-datasource", id,
			   NULL);
    g_hash_table_insert (grl_tracker_source_sources_modified,
                         (gpointer) grl_tracker_source_get_tracker_source (source),
                        source);
  }

  priv = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  priv->state = GRL_TRACKER_SOURCE_STATE_INSERTING;
  priv->notification_ref++;

  evt->new_sources = g_list_append (evt->new_sources, source);

  GRL_DEBUG ("Preadd source p=%p name=%s id=%s count=%u",
             source, source_name, id, priv->notification_ref);
}

static void
tracker_evt_update_source_del (tracker_evt_update_t *evt,
                               GrlTrackerSource *source)
{
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (source);

  priv->notification_ref++;
  priv->state = GRL_TRACKER_SOURCE_STATE_DELETING;

  evt->old_sources = g_list_append (evt->old_sources, source);

  GRL_DEBUG ("Predel source p=%p name=%s id=%s count=%u", source,
             grl_source_get_name (GRL_SOURCE (source)),
             grl_tracker_source_get_tracker_source (source),
             priv->notification_ref);
}

static void
tracker_evt_postupdate_sources (tracker_evt_update_t *evt)
{
  GList *source;

  GRL_DEBUG ("%s: evt=%p", __FUNCTION__, evt);

  source = evt->old_sources;
  while (source != NULL) {
    grl_tracker_del_source (GRL_TRACKER_SOURCE (source->data));
    source = source->next;
  }

  source = evt->new_sources;
  while (source != NULL) {
    grl_tracker_add_source (GRL_TRACKER_SOURCE (source->data));
    source = source->next;
  }

  tracker_evt_update_free (evt);
}

static void
tracker_evt_update_orphan_item_cb (GObject              *object,
                                   GAsyncResult         *result,
                                   tracker_evt_update_t *evt)
{
  guint id;
  const gchar *type, *datasource;
  GrlTrackerSource *source = NULL;
  GError *error = NULL;

  GRL_DEBUG ("%s: evt=%p", __FUNCTION__, evt);

  if (!tracker_sparql_cursor_next_finish (evt->cursor, result, &error)) {
    if (error != NULL) {
      GRL_DEBUG ("\terror in parsing : %s", error->message);
      g_error_free (error);
    } else {
      GRL_DEBUG ("\tend of parsing...");
    }

    g_object_unref (evt->cursor);
    evt->cursor = NULL;

    if (grl_tracker_per_device_source) {
      /* Once all items have been processed, add new sources and we're
	 done. */
      tracker_evt_postupdate_sources (evt);
    } else {
      tracker_evt_update_free (evt);
    }

    return;
  }

  type = tracker_sparql_cursor_get_string (evt->cursor, 0, NULL);
  id = tracker_sparql_cursor_get_integer (evt->cursor, 1);
  datasource = tracker_sparql_cursor_get_string (evt->cursor, 2, NULL);

  GRL_DEBUG ("\tOrphan item: id=%u datasource=%s", id, datasource);

  if (datasource)
    source = grl_tracker_source_find (datasource);

  if (source && GRL_IS_TRACKER_SOURCE (source)) {
    GrlMedia *media;

    GRL_DEBUG (" \tAdding to cache id=%u", id);
    grl_tracker_source_cache_add_item (grl_tracker_item_cache, id, source);

    if (grl_tracker_source_can_notify (source)) {
      media = grl_tracker_build_grilo_media (type);
      if (media) {
        gchar *str_id = g_strdup_printf ("%i", id);
        gint change_type =
          GPOINTER_TO_INT (g_hash_table_lookup (evt->orphan_items,
                                                GSIZE_TO_POINTER (id)));

        grl_media_set_id (media, str_id);
        g_free (str_id);

        GRL_DEBUG ("\tNotify id=%u source=%s p=%p", id,
                   grl_source_get_name (GRL_SOURCE (source)),
                   source);
        grl_source_notify_change (GRL_SOURCE (source),
                                  media, change_type, FALSE);

        g_object_unref (media);
      }
    }
  }

  tracker_sparql_cursor_next_async (evt->cursor, NULL,
                                    (GAsyncReadyCallback) tracker_evt_update_orphan_item_cb,
                                    (gpointer) evt);
}

static void
tracker_evt_update_orphans_cb (GObject              *object,
                               GAsyncResult         *result,
                               tracker_evt_update_t *evt)
{
  GError *error = NULL;

  GRL_DEBUG ("%s: evt=%p", __FUNCTION__, evt);

  evt->cursor = tracker_sparql_connection_query_finish (grl_tracker_connection,
                                                        result, &error);

  if (error != NULL) {
    GRL_WARNING ("Could not execute sparql query: %s", error->message);

    g_error_free (error);
    tracker_evt_postupdate_sources (evt);
    return;
  }

  tracker_sparql_cursor_next_async (evt->cursor, NULL,
                                    (GAsyncReadyCallback) tracker_evt_update_orphan_item_cb,
                                    (gpointer) evt);
}

static void
tracker_evt_update_orphans (tracker_evt_update_t *evt)
{
  gboolean first = TRUE;
  GString *request_str;
  GList *subject, *subjects;
  GList *source, *sources;

  GRL_DEBUG ("%s: evt=%p", __FUNCTION__, evt);

  if (g_hash_table_size (evt->orphan_items) < 1) {
    tracker_evt_postupdate_sources (evt);
    return;
  }

  sources = grl_registry_get_sources (grl_registry_get_default (),
                                      FALSE);

  request_str = g_string_new ("");
  subjects = g_hash_table_get_keys (evt->orphan_items);

  subject = subjects;
  while (subject != NULL) {
    guint id = GPOINTER_TO_INT (subject->data);
    if (GPOINTER_TO_INT (g_hash_table_lookup (evt->orphan_items,
                                              subject->data)) != GRL_CONTENT_REMOVED) {
      if (first) {
        g_string_append_printf (request_str, "%u", id);
        first = FALSE;
      }
      else {
        g_string_append_printf (request_str, ", %u", id);
      }
    } else {
      /* Notify all sources that a media been removed */
      source = sources;
      while (source != NULL) {
        if (GRL_IS_TRACKER_SOURCE (source->data)) {
          GRL_DEBUG ("\tNotify id=%u source=%s p=%p", id,
                     grl_source_get_name (GRL_SOURCE (source->data)),
                     source->data);
          if (grl_tracker_source_can_notify (GRL_TRACKER_SOURCE (source->data))) {
            GrlMedia *media = grl_media_new ();
            gchar *str_id = g_strdup_printf ("%u", id);

            grl_media_set_id (media, str_id);
            g_free (str_id);

            grl_source_notify_change (GRL_SOURCE (source->data),
                                      media,
                                      GRL_CONTENT_REMOVED,
                                      FALSE);
            g_object_unref (media);
          }
        }
        source = source->next;
      }
    }
    subject = subject->next;
  }

  g_list_free (subjects);

  if (request_str->len > 0) {
    gchar *sparql_final = g_strdup_printf (TRACKER_MEDIA_ITEM, request_str->str);

    GRL_DEBUG ("\trequest : '%s'", sparql_final);

    tracker_sparql_connection_query_async (grl_tracker_connection,
                                           sparql_final,
                                           NULL,
                                           (GAsyncReadyCallback) tracker_evt_update_orphans_cb,
                                           evt);

    g_free (sparql_final);
  } else {
    tracker_evt_postupdate_sources (evt);
  }

  g_string_free (request_str, TRUE);
}

static void
tracker_evt_update_items_cb (gpointer              key,
                             gpointer              value,
                             tracker_evt_update_t *evt)
{
  guint id = GPOINTER_TO_INT (key);
  gchar *str_id;
  GrlTrackerSource *source = (GrlTrackerSource *) value;
  GrlMedia *media;

  GRL_DEBUG ("%s: evt=%p", __FUNCTION__, evt);

  if (!source) {
    g_assert ("\tnot in cache ???");
    return;
  }

  if (!grl_tracker_source_can_notify (source)) {
    GRL_DEBUG ("\tno notification for source %s...",
	       grl_source_get_name (GRL_SOURCE (source)));
    return;
  }

  media = grl_media_new ();
  str_id = g_strdup_printf ("%i", id);
  grl_media_set_id (media, str_id);
  g_free (str_id);

  GRL_DEBUG ("\tNotify id=%u source=%s", id,
             grl_source_get_name (GRL_SOURCE (source)));
  grl_source_notify_change (GRL_SOURCE (source), media, evt->change_type, FALSE);

  g_object_unref (media);
}

static void
tracker_evt_update_items (tracker_evt_update_t *evt)
{
  evt->change_type = GRL_CONTENT_REMOVED;
  g_hash_table_foreach (evt->deleted_items,
                        (GHFunc) tracker_evt_update_items_cb, evt);
  evt->change_type = GRL_CONTENT_ADDED;
  g_hash_table_foreach (evt->inserted_items,
                        (GHFunc) tracker_evt_update_items_cb, evt);
  evt->change_type = GRL_CONTENT_CHANGED;
  g_hash_table_foreach (evt->updated_items,
                        (GHFunc) tracker_evt_update_items_cb, evt);
}

static void
tracker_evt_preupdate_sources_item_cb (GObject              *object,
                                       GAsyncResult         *result,
                                       tracker_evt_update_t *evt)
{
  const gchar *type, *datasource, *uri, *datasource_name;
  gboolean source_available = FALSE;
  GrlTrackerSource *source;
  GError *error = NULL;

  GRL_DEBUG ("%s: evt=%p", __FUNCTION__, evt);

  if (!tracker_sparql_cursor_next_finish (evt->cursor, result, &error)) {
    if (error != NULL) {
      GRL_DEBUG ("\terror in parsing : %s", error->message);
      g_error_free (error);
    } else {
      GRL_DEBUG ("\tend of parsing... start notifying sources");
    }

    g_object_unref (evt->cursor);
    evt->cursor = NULL;

    /* Once all sources have been preupdated, start items
       updates. */
    tracker_evt_update_items (evt);
    tracker_evt_update_orphans (evt);

    return;
  }

  type = tracker_sparql_cursor_get_string (evt->cursor, 0, NULL);
  datasource = tracker_sparql_cursor_get_string (evt->cursor, 1, NULL);
  datasource_name = tracker_sparql_cursor_get_string (evt->cursor, 2, NULL);
  uri = tracker_sparql_cursor_get_string (evt->cursor, 3, NULL);
  if (tracker_sparql_cursor_is_bound (evt->cursor, 4))
    source_available = tracker_sparql_cursor_get_boolean (evt->cursor, 4);

  source = grl_tracker_source_find (datasource);

  GRL_DEBUG ("\tdatasource=%s uri=%s available=%i source=%p",
             datasource, uri, source_available, source);

  if (source_available) {
    if (source == NULL) {
      gchar *source_name = grl_tracker_get_source_name (type, uri, datasource,
                                                        datasource_name);
      /* Defer source creation until we have processed all sources */
      if (source_name) {
        tracker_evt_update_source_add (evt, datasource, source_name);
        g_free (source_name);
      }
    } else {
      GRL_DEBUG ("\tChanges on source %p / %s", source, datasource);
    }
  } else if (!source_available && source != NULL) {
    tracker_evt_update_source_del (evt, GRL_TRACKER_SOURCE (source));
  }

  tracker_sparql_cursor_next_async (evt->cursor, NULL,
                                    (GAsyncReadyCallback) tracker_evt_preupdate_sources_item_cb,
                                    (gpointer) evt);
}

static void
tracker_evt_preupdate_sources_cb (GObject              *object,
                                  GAsyncResult         *result,
                                  tracker_evt_update_t *evt)
{
  GError *error = NULL;

  GRL_DEBUG ("%s: evt=%p", __FUNCTION__, evt);

  evt->cursor = tracker_sparql_connection_query_finish (grl_tracker_connection,
                                                        result, &error);

  if (error != NULL) {
    GRL_WARNING ("\tCannot handle datasource request : %s", error->message);

    g_error_free (error);

    tracker_evt_update_items (evt);
    tracker_evt_update_orphans (evt);
    return;
  }

  tracker_sparql_cursor_next_async (evt->cursor, NULL,
                                    (GAsyncReadyCallback) tracker_evt_preupdate_sources_item_cb,
                                    (gpointer) evt);
}

static void
tracker_evt_preupdate_sources (tracker_evt_update_t *evt)
{
  tracker_sparql_connection_query_async (grl_tracker_connection,
                                         TRACKER_DATASOURCES_REQUEST,
                                         NULL,
                                         (GAsyncReadyCallback) tracker_evt_preupdate_sources_cb,
                                         evt);
}

static void
tracker_dbus_signal_cb (GDBusConnection *connection,
                        const gchar     *sender_name,
                        const gchar     *object_path,
                        const gchar     *interface_name,
                        const gchar     *signal_name,
                        GVariant        *parameters,
                        gpointer         user_data)

{
  gchar *class_name;
  gint graph = 0, subject = 0, predicate = 0, object = 0;
  GVariantIter *iter1, *iter2;
  tracker_evt_update_t *evt = tracker_evt_update_new ();

  g_variant_get (parameters, "(&sa(iiii)a(iiii))", &class_name, &iter1, &iter2);

  GRL_DEBUG ("Tracker update event for class=%s ins=%lu del=%lu evt=%p",
             class_name,
             (unsigned long) g_variant_iter_n_children (iter1),
             (unsigned long) g_variant_iter_n_children (iter2),
	     evt);

  /* Process deleted items */
  while (g_variant_iter_loop (iter1, "(iiii)", &graph,
                              &subject, &predicate, &object)) {
    gpointer psubject = GSIZE_TO_POINTER (subject);
    GrlTrackerSource *source =
      grl_tracker_source_cache_get_source (grl_tracker_item_cache, subject);

    /* GRL_DEBUG ("\tdelete=> subject=%i", subject); */

    if (source) {
      g_hash_table_insert (evt->deleted_items, psubject,
                           g_object_ref (source));
    } else {
        g_hash_table_insert (evt->orphan_items, psubject,
                             GSIZE_TO_POINTER (GRL_CONTENT_REMOVED));
    }
  }

  while (g_variant_iter_loop (iter2, "(iiii)", &graph,
                              &subject, &predicate, &object)) {
    gpointer psubject = GSIZE_TO_POINTER (subject);
    GrlTrackerSource *source =
      grl_tracker_source_cache_get_source (grl_tracker_item_cache, subject);

    /* GRL_DEBUG ("\tinsert=> subject=%i", subject); */

    if (source) {
      /* Removed & inserted items are probably just renamed items... */
      if (g_hash_table_lookup (evt->deleted_items, psubject)) {
        g_hash_table_remove (evt->deleted_items, psubject);
        g_hash_table_insert (evt->updated_items, psubject,
                             g_object_ref (source));
      } else if (!g_hash_table_lookup (evt->updated_items, psubject)) {
        g_hash_table_insert (evt->inserted_items, psubject,
                             g_object_ref (source));
      }
    } else {
      gpointer state;

      if (g_hash_table_lookup_extended (evt->orphan_items, psubject,
                                        NULL, &state) &&
            (GPOINTER_TO_INT (state) == GRL_CONTENT_REMOVED)) {
        g_hash_table_insert (evt->orphan_items, psubject,
                             GSIZE_TO_POINTER (GRL_CONTENT_CHANGED));
      } else {
        g_hash_table_insert (evt->orphan_items, psubject,
                             GSIZE_TO_POINTER (GRL_CONTENT_ADDED));
      }
    }
  }

  g_variant_iter_free (iter1);
  g_variant_iter_free (iter2);

  GRL_DEBUG ("\tinserted=%i deleted=%i updated=%i orphan=%i",
             g_hash_table_size (evt->inserted_items),
             g_hash_table_size (evt->deleted_items),
             g_hash_table_size (evt->updated_items),
	     g_hash_table_size (evt->orphan_items));

  if (grl_tracker_per_device_source) {
    tracker_evt_preupdate_sources (evt);
  } else {
    tracker_evt_update_items (evt);
    tracker_evt_update_orphans (evt);
  }
}

void
grl_tracker_source_dbus_start_watch (void)
{
  GDBusConnection *connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  tracker_dbus_signal_id = g_dbus_connection_signal_subscribe (connection,
                                                               TRACKER_DBUS_SERVICE,
                                                               TRACKER_DBUS_INTERFACE_RESOURCES,
                                                               "GraphUpdated",
                                                               TRACKER_DBUS_OBJECT_RESOURCES,
                                                               NULL,
                                                               G_DBUS_SIGNAL_FLAGS_NONE,
                                                               tracker_dbus_signal_cb,
                                                               NULL,
                                                               NULL);
}

void
grl_tracker_source_init_notifs (void)
{
  GRL_LOG_DOMAIN_INIT (tracker_notif_log_domain, "tracker-notif");
}
