/*
 * Copyright (C) 2011-2012 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
 * Copyright (C) 2020 Red Hat Inc.
 *
 * Contact: Carlos Garnacho <carlosg@gnome.org>
 *
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 *          Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gio/gio.h>
#include <tracker-sparql.h>

#include "grl-tracker.h"
#include "grl-tracker-source-api.h"
#include "grl-tracker-source-cache.h"
#include "grl-tracker-source-priv.h"
#include "grl-tracker-source-statements.h"
#include "grl-tracker-utils.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT tracker_source_request_log_domain

GRL_LOG_DOMAIN_STATIC(tracker_source_request_log_domain);
GRL_LOG_DOMAIN_STATIC(tracker_source_result_log_domain);

/* Inputs/requests */
#define GRL_IDEBUG(args...)                     \
  GRL_LOG (tracker_source_request_log_domain,   \
           GRL_LOG_LEVEL_DEBUG, args)

/* Outputs/results */
#define GRL_ODEBUG(args...)                     \
  GRL_LOG (tracker_source_result_log_domain,    \
           GRL_LOG_LEVEL_DEBUG, args)

/**/

/**/

static GrlKeyID    grl_metadata_key_tracker_category;
static GHashTable *grl_tracker_operations;

typedef struct {
  GCancellable *cancel;
  const GList *keys;
  gpointer data;
  GrlTypeFilter type_filter;
} GrlTrackerOp;

/**/

static void
set_title_from_filename (GrlMedia *media)
{
  const gchar *url;
  gchar *path, *display_name, *ext, *title = NULL;
  guint suffix_len;

  /* Prefer the real title */
  if (grl_media_get_title (media))
    return;

  url = grl_media_get_url (media);
  if (url == NULL)
    return;

  path = g_filename_from_uri (url, NULL, NULL);
  if (!path)
    return;
  display_name = g_filename_display_basename (path);
  g_free (path);
  ext = strrchr (display_name, '.');
  if (ext) {
    suffix_len = strlen (ext);
    if (suffix_len != 4 && suffix_len != 5)
      goto out;

    title = g_strndup (display_name, ext - display_name);
  } else {
    title = g_strdup (display_name);
  }

  grl_data_set_string (GRL_DATA (media), GRL_METADATA_KEY_TITLE, title);
  grl_data_set_boolean (GRL_DATA (media), GRL_METADATA_KEY_TITLE_FROM_FILENAME, TRUE);

out:
  g_free (title);
  g_free (display_name);
}

static void
fill_grilo_media_from_sparql (GrlTrackerSource    *source,
                              GrlMedia            *media,
                              TrackerSparqlCursor *cursor,
                              gint                 column)
{
  const gchar *sparql_key = tracker_sparql_cursor_get_variable_name (cursor,
                                                                     column);
  tracker_grl_sparql_t *assoc =
    grl_tracker_get_mapping_from_sparql (sparql_key);
  union {
    gint64 int_val;
    gdouble double_val;
    const gchar *str_val;
  } val;

  GrlKeyID grl_key;

  if (assoc == NULL) {
    /* Maybe the user is setting the key */
    GrlRegistry *registry = grl_registry_get_default ();
    grl_key = grl_registry_lookup_metadata_key (registry, sparql_key);
    if (grl_key == GRL_METADATA_KEY_INVALID) {
      return;
    }
  } else {
    grl_key = assoc->grl_key;
  }

  GRL_ODEBUG ("\tSetting media prop (col=%i/var=%s/prop=%s) %s",
              column,
              sparql_key,
              GRL_METADATA_KEY_GET_NAME (grl_key),
              tracker_sparql_cursor_get_string (cursor, column, NULL));

  if (tracker_sparql_cursor_is_bound (cursor, column) == FALSE) {
    GRL_ODEBUG ("\t\tDropping, no data");
    return;
  }

  if (grl_data_has_key (GRL_DATA (media), grl_key)) {
    GRL_ODEBUG ("\t\tDropping, already here");
    return;
  }

  if (assoc && assoc->set_value) {
    assoc->set_value (cursor, column, media, assoc->grl_key);
  } else {
    GType grl_type = GRL_METADATA_KEY_GET_TYPE (grl_key);
    if (grl_type == G_TYPE_STRING) {
      /* Cache the source associated to this result. */
      if (grl_key == GRL_METADATA_KEY_ID) {
        grl_tracker_source_cache_add_item (grl_tracker_item_cache,
                                           tracker_sparql_cursor_get_integer (cursor,
                                                                              column),
                                           source);
      }
      val.str_val = tracker_sparql_cursor_get_string (cursor, column, NULL);
      if (val.str_val != NULL)
        grl_data_set_string (GRL_DATA (media), grl_key, val.str_val);
    } else if (grl_type == G_TYPE_INT) {
      val.int_val = tracker_sparql_cursor_get_integer (cursor, column);
      grl_data_set_int (GRL_DATA (media), grl_key, val.int_val);
    } else if (grl_type == G_TYPE_INT64) {
      val.int_val = tracker_sparql_cursor_get_integer (cursor, column);
      grl_data_set_int64 (GRL_DATA (media), grl_key, val.int_val);
    } else if (grl_type == G_TYPE_FLOAT) {
      val.double_val = tracker_sparql_cursor_get_double (cursor, column);
      grl_data_set_float (GRL_DATA (media), grl_key, (gfloat) val.double_val);
    } else if (grl_type == G_TYPE_DATE_TIME) {
      val.str_val = tracker_sparql_cursor_get_string (cursor, column, NULL);
      GDateTime *date_time = grl_date_time_from_iso8601 (val.str_val);
      grl_data_set_boxed (GRL_DATA (media), grl_key, date_time);
      g_date_time_unref (date_time);
    } else {
      GRL_ODEBUG ("\t\tUnexpected data type");
    }
  }
}

static GrlTrackerOp *
grl_tracker_op_new (GrlTypeFilter  type_filter,
                    gpointer       data)
{
  GrlTrackerOp *os;

  os = g_new0 (GrlTrackerOp, 1);
  os->cancel = g_cancellable_new ();
  os->type_filter = type_filter;
  os->data = data;

  return os;
}

static void
grl_tracker_op_free (GrlTrackerOp *os)
{
  g_object_unref (os->cancel);
  g_free (os);
}

/* I can haz templatze ?? */
#define TRACKER_QUERY_CB(spec_type,name,error)                          \
                                                                        \
  static void                                                           \
  tracker_##name##_result_cb (GObject      *source_object,              \
                              GAsyncResult *result,                     \
                              GrlTrackerOp *os)                         \
  {                                                                     \
    TrackerSparqlCursor *cursor = TRACKER_SPARQL_CURSOR (source_object);\
    gint         col, type;                                             \
    GError      *tracker_error = NULL, *error = NULL;                   \
    GrlMedia    *media;                                                 \
    spec_type   *spec =                                                 \
      (spec_type *) os->data;                                           \
                                                                        \
    GRL_ODEBUG ("%s", __FUNCTION__);                                    \
                                                                        \
    if (!tracker_sparql_cursor_next_finish (cursor,                     \
                                            result,                     \
                                            &tracker_error)) {          \
      if (tracker_error != NULL) {                                      \
        GRL_WARNING ("\terror in parsing query id=%u : %s",             \
                     spec->operation_id, tracker_error->message);       \
                                                                        \
        if (!g_error_matches (tracker_error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) \
          error = g_error_new (GRL_CORE_ERROR,                          \
                               GRL_CORE_ERROR_##error##_FAILED,         \
                               _("Failed to query: %s"),                \
                               tracker_error->message);                 \
                                                                        \
        spec->callback (spec->source,                      \
                        spec->operation_id,                             \
                        NULL, 0,                                        \
                        spec->user_data, error);                        \
                                                                        \
        g_clear_error (&error);                                         \
        g_error_free (tracker_error);                                   \
      } else {                                                          \
        GRL_ODEBUG ("\tend of parsing id=%u :)", spec->operation_id);   \
                                                                        \
        spec->callback (spec->source,                                   \
                        spec->operation_id,                             \
                        NULL, 0,                                        \
                        spec->user_data, NULL);                         \
      }                                                                 \
                                                                        \
      grl_tracker_op_free (os);                                         \
      g_object_unref (cursor);                                          \
      return;                                                           \
    }                                                                   \
                                                                        \
    type = tracker_sparql_cursor_get_integer (cursor, 0);               \
                                                                        \
    GRL_ODEBUG ("\tParsing line of type %x",                            \
                type);                                                  \
                                                                        \
    media = grl_tracker_build_grilo_media ((GrlMediaType ) type);       \
                                                                        \
    if (media != NULL) {                                                \
      for (col = 1 ;                                                    \
           col < tracker_sparql_cursor_get_n_columns (cursor) ;         \
           col++) {                                                     \
        fill_grilo_media_from_sparql (GRL_TRACKER_SOURCE (spec->source), \
                                      media, cursor, col);              \
      }                                                                 \
      set_title_from_filename (media);                                  \
                                                                        \
      spec->callback (spec->source,                                     \
                      spec->operation_id,                               \
                      media,                                            \
                      GRL_SOURCE_REMAINING_UNKNOWN,                     \
                      spec->user_data,                                  \
                      NULL);                                            \
    }                                                                   \
                                                                        \
    /* Schedule the next row to parse */                                \
    tracker_sparql_cursor_next_async (cursor, os->cancel,               \
                                      (GAsyncReadyCallback) tracker_##name##_result_cb, \
                                      (gpointer) os);                   \
  }                                                                     \
                                                                        \
  static void                                                           \
  tracker_##name##_cb (GObject      *source_object,                     \
                       GAsyncResult *result,                            \
                       GrlTrackerOp *os)                                \
  {                                                                     \
    TrackerSparqlStatement *statement = TRACKER_SPARQL_STATEMENT (source_object); \
    GError *tracker_error = NULL, *error = NULL;                        \
    spec_type *spec = (spec_type *) os->data;                           \
    TrackerSparqlCursor *cursor;                                        \
                                                                        \
    GRL_ODEBUG ("%s", __FUNCTION__);                                    \
                                                                        \
    cursor =                                                            \
      tracker_sparql_statement_execute_finish (statement,               \
                                               result, &tracker_error); \
                                                                        \
    if (tracker_error) {                                                \
      GRL_WARNING ("Could not execute sparql query id=%u: %s",          \
                   spec->operation_id, tracker_error->message);         \
                                                                        \
      error = g_error_new (GRL_CORE_ERROR,                              \
                           GRL_CORE_ERROR_##error##_FAILED,             \
                           _("Failed to query: %s"),                    \
                           tracker_error->message);                     \
                                                                        \
      spec->callback (spec->source, spec->operation_id, NULL, 0,           \
                      spec->user_data, error);                          \
                                                                        \
      g_error_free (tracker_error);                                     \
      g_error_free (error);                                             \
      grl_tracker_op_free (os);                                         \
                                                                        \
      return;                                                           \
    }                                                                   \
                                                                        \
    /* Start parsing results */                                         \
    tracker_sparql_cursor_next_async (cursor, NULL,                     \
                                      (GAsyncReadyCallback) tracker_##name##_result_cb, \
                                      (gpointer) os);                   \
  }

TRACKER_QUERY_CB(GrlSourceQuerySpec, query, QUERY)
TRACKER_QUERY_CB(GrlSourceBrowseSpec, browse, BROWSE)
TRACKER_QUERY_CB(GrlSourceSearchSpec, search, SEARCH)

static void
tracker_resolve_result_cb (GObject      *source_object,
                           GAsyncResult *result,
                           GrlTrackerOp *os)
{
  TrackerSparqlCursor  *cursor = TRACKER_SPARQL_CURSOR (source_object);
  gint                  col;
  GError               *tracker_error = NULL, *error = NULL;
  GrlSourceResolveSpec *rs = (GrlSourceResolveSpec *) os->data;

  GRL_ODEBUG ("%s", __FUNCTION__);

  if (tracker_sparql_cursor_next_finish (cursor, result, &tracker_error)) {
    GRL_ODEBUG ("\tend of parsing id=%u :)", rs->operation_id);

    /* Translate Sparql result into Grilo result */
    for (col = 0 ; col < tracker_sparql_cursor_get_n_columns (cursor) ; col++) {
      fill_grilo_media_from_sparql (GRL_TRACKER_SOURCE (rs->source),
                                    rs->media, cursor, col);
    }
    set_title_from_filename (rs->media);

    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
  } else if (!tracker_error) {
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
  }

  if (tracker_error) {
    GRL_WARNING ("\terror in parsing resolve id=%u : %s",
                 rs->operation_id, tracker_error->message);

    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_RESOLVE_FAILED,
                         _("Failed to resolve: %s"),
                         tracker_error->message);

    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);

    g_clear_error (&tracker_error);
    g_error_free (error);
  }

  g_clear_object (&cursor);
  grl_tracker_op_free (os);
}

static void
tracker_resolve_cb (GObject      *source_object,
                    GAsyncResult *result,
                    GrlTrackerOp *os)
{
  TrackerSparqlStatement *statement = TRACKER_SPARQL_STATEMENT (source_object);
  GrlSourceResolveSpec *rs = (GrlSourceResolveSpec *) os->data;
  GError               *tracker_error = NULL, *error = NULL;
  TrackerSparqlCursor  *cursor;

  GRL_ODEBUG ("%s", __FUNCTION__);

  cursor = tracker_sparql_statement_execute_finish (statement,
                                                    result, &tracker_error);

  if (!cursor) {
    if (tracker_error) {
      GRL_WARNING ("Could not execute sparql resolve query : %s",
                   tracker_error->message);

      error = g_error_new (GRL_CORE_ERROR,
                           GRL_CORE_ERROR_RESOLVE_FAILED,
                           _("Failed to resolve: %s"),
                           tracker_error->message);

      rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);

      g_clear_error (&tracker_error);
      g_error_free (error);
    } else {
      rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
    }

    g_clear_object (&cursor);
    grl_tracker_op_free (os);

    return;
  }

  tracker_sparql_cursor_next_async (cursor, NULL,
                                    (GAsyncReadyCallback) tracker_resolve_result_cb,
                                    (gpointer) os);
}

static void
tracker_media_from_uri_cb (GObject      *source_object,
                           GAsyncResult *result,
                           GrlTrackerOp *os)
{
  TrackerSparqlStatement    *statement = TRACKER_SPARQL_STATEMENT (source_object); \
  GrlSourceMediaFromUriSpec *mfus = (GrlSourceMediaFromUriSpec *) os->data;
  GError                    *tracker_error = NULL, *error = NULL;
  GrlMedia                  *media;
  TrackerSparqlCursor       *cursor;
  gint                       col, type;

  GRL_ODEBUG ("%s", __FUNCTION__);

  cursor = tracker_sparql_statement_execute_finish (statement,
                                                    result, &tracker_error);

  if (tracker_error) {
    GRL_WARNING ("Could not execute sparql media from uri query : %s",
                 tracker_error->message);

    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_MEDIA_FROM_URI_FAILED,
                         _("Failed to get media from uri: %s"),
                         tracker_error->message);

    mfus->callback (mfus->source, mfus->operation_id, NULL, mfus->user_data, error);

    g_error_free (tracker_error);
    g_error_free (error);

    goto end_operation;
  }


  if (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
    /* Build grilo media */
    type = tracker_sparql_cursor_get_integer (cursor, 0);
    media = grl_tracker_build_grilo_media ((GrlMediaType) type);

    /* Translate Sparql result into Grilo result */
    for (col = 0 ; col < tracker_sparql_cursor_get_n_columns (cursor) ; col++) {
      fill_grilo_media_from_sparql (GRL_TRACKER_SOURCE (mfus->source),
                                    media, cursor, col);
    }
    set_title_from_filename (media);

    mfus->callback (mfus->source, mfus->operation_id, media, mfus->user_data, NULL);
  } else {
    mfus->callback (mfus->source, mfus->operation_id, NULL, mfus->user_data, NULL);
  }

 end_operation:
  g_clear_object (&cursor);

  grl_tracker_op_free (os);
}

static void
tracker_store_metadata_cb (GObject      *source_object,
                           GAsyncResult *result,
                           GrlTrackerOp *os)
{
  GrlSourceStoreMetadataSpec *sms =
    (GrlSourceStoreMetadataSpec *) os->data;
  GError *tracker_error = NULL, *error = NULL;

  g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                            result, &tracker_error);

  if (tracker_error) {
    GRL_WARNING ("Could not writeback metadata: %s",
                 tracker_error->message);

    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_STORE_METADATA_FAILED,
                         _("Failed to update metadata: %s"),
                         tracker_error->message);

    sms->callback (sms->source, sms->media, NULL, sms->user_data, error);

    g_error_free (tracker_error);
    g_error_free (error);
  } else {
    sms->callback (sms->source, sms->media, NULL, sms->user_data, NULL);
  }

  grl_tracker_op_free (os);
}

/**/

const GList *
grl_tracker_source_writable_keys (GrlSource *source)
{
  static GList *keys = NULL;
  GrlRegistry *registry;
  GrlKeyID grl_metadata_key_chromaprint;

  if (!keys) {
    registry = grl_registry_get_default ();
    grl_metadata_key_chromaprint = grl_registry_lookup_metadata_key (registry, "chromaprint");

    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ALBUM,
                                      GRL_METADATA_KEY_ALBUM_DISC_NUMBER,
                                      GRL_METADATA_KEY_ARTIST,
                                      GRL_METADATA_KEY_ALBUM_ARTIST,
                                      GRL_METADATA_KEY_AUTHOR,
                                      GRL_METADATA_KEY_COMPOSER,
                                      GRL_METADATA_KEY_CREATION_DATE,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_SEASON,
                                      GRL_METADATA_KEY_EPISODE,
                                      GRL_METADATA_KEY_TRACK_NUMBER,
                                      GRL_METADATA_KEY_MB_RELEASE_ID,
                                      GRL_METADATA_KEY_MB_RELEASE_GROUP_ID,
                                      GRL_METADATA_KEY_MB_RECORDING_ID,
                                      GRL_METADATA_KEY_MB_TRACK_ID,
                                      GRL_METADATA_KEY_MB_ARTIST_ID,
                                      GRL_METADATA_KEY_PUBLICATION_DATE,
                                      grl_metadata_key_chromaprint,
                                      GRL_METADATA_KEY_INVALID);
  }
  return keys;
}

/**
 * Query is a SPARQL query.
 *
 * Columns must be named with the Grilo key name that the column
 * represent. Unnamed or unknown columns will be ignored.
 *
 * First column must be grilo media enum value, and it does not need to be named.
 * An example for searching all songs:
 *
 * <informalexample>
 *   <programlisting>
 *     SELECT 1
 *            ?song            AS ?id
 *            nie:title(?song) AS ?title
 *            nie:isStoredAs(?song) AS ?url
 *     WHERE { ?song a nmm:MusicPiece }
 *   </programlisting>
 * </informalexample>
 *
 * Alternatively, we can use a partial SPARQL query: just specify the sentence
 * in the WHERE part. In this case, "?urn" is the ontology concept to be used in
 * the clause.
 *
 * An example of such partial query:
 *
 * <informalexample>
 *   <programlisting>
 *     ?urn a nfo:Media
 *   </programlisting>
 * </informalexample>
 *
 * In this case, all data required to build a full SPARQL query will be get from
 * the query spec.
 */
void
grl_tracker_source_query (GrlSource *source,
                          GrlSourceQuerySpec *qs)
{
  GError               *error = NULL;
  GrlTrackerOp         *os;
  TrackerSparqlStatement *statement;

  GRL_IDEBUG ("%s: id=%u", __FUNCTION__, qs->operation_id);

  if (!qs->query || qs->query[0] == '\0') {
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_QUERY_FAILED,
                                 _("Empty query"));
    goto send_error;
  }

  if (g_ascii_strncasecmp (qs->query, "select ", 7) == 0) {
    statement =
      tracker_sparql_connection_query_statement (GRL_TRACKER_SOURCE (source)->priv->tracker_connection,
                                                 qs->query,
                                                 NULL, &error);
  } else {
    statement =
      grl_tracker_source_create_statement (GRL_TRACKER_SOURCE (source),
                                           GRL_TRACKER_QUERY_ALL,
                                           qs->options,
                                           qs->keys,
                                           qs->query,
                                           &error);
  }

  if (!statement)
    goto send_error;

  os = grl_tracker_op_new (grl_operation_options_get_type_filter (qs->options), qs);

  tracker_sparql_statement_execute_async (statement,
                                          os->cancel,
                                          (GAsyncReadyCallback) tracker_query_cb,
                                          os);

  g_clear_object (&statement);

  return;

 send_error:
  qs->callback (qs->source, qs->operation_id, NULL, 0, qs->user_data, error);
  g_error_free (error);
}

void
grl_tracker_source_resolve (GrlSource *source,
                            GrlSourceResolveSpec *rs)
{
  GrlTrackerOp         *os;
  GrlTrackerQueryType   query_type;
  const gchar          *arg, *value;
  GError               *error = NULL;
  TrackerSparqlStatement *statement;

  GRL_IDEBUG ("%s: id=%i", __FUNCTION__, rs->operation_id);

  if (grl_media_get_id (rs->media) != NULL) {
    query_type = GRL_TRACKER_QUERY_RESOLVE;
    arg = "resource";
    value = grl_media_get_id (rs->media);
  } else if (grl_media_get_url (rs->media) != NULL) {
    query_type = GRL_TRACKER_QUERY_RESOLVE_URI;
    arg = "uri";
    value = grl_media_get_url (rs->media);
  } else {
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
    return;
  }

  statement =
    grl_tracker_source_create_statement (GRL_TRACKER_SOURCE (source),
                                         query_type, NULL,
                                         rs->keys, NULL, &error);
  if (!statement) {
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);
    g_error_free (error);
    return;
  }

  os = grl_tracker_op_new (GRL_TYPE_FILTER_ALL, rs);

  tracker_sparql_statement_bind_string (statement, arg, value);
  tracker_sparql_statement_execute_async (statement,
                                          os->cancel,
                                          (GAsyncReadyCallback) tracker_resolve_cb,
                                          os);
  g_clear_object (&statement);
}

gboolean
grl_tracker_source_may_resolve (GrlSource *source,
                                GrlMedia  *media,
                                GrlKeyID   key_id,
                                GList    **missing_keys)
{
  GRL_IDEBUG ("%s: key=%s", __FUNCTION__, GRL_METADATA_KEY_GET_NAME (key_id));

  if (!grl_tracker_key_is_supported (key_id)) {
    return FALSE;
  }

  if (media) {
    if (grl_media_get_id (media) || grl_media_get_url (media)) {
      return TRUE;
    } else {
      if (missing_keys) {
        *missing_keys = g_list_append (*missing_keys,
                                       GRLKEYID_TO_POINTER (GRL_METADATA_KEY_URL));
      }
    }
  }

  return FALSE;
}

void
grl_tracker_source_store_metadata (GrlSource *source,
                                   GrlSourceStoreMetadataSpec *sms)
{
  GrlTrackerSourcePrivate *priv = GRL_TRACKER_SOURCE (source)->priv;
  TrackerResource *resource;
  GrlTrackerOp *os;

  resource = grl_tracker_build_resource_from_media (sms->media, sms->keys);

  os = grl_tracker_op_new (GRL_TYPE_FILTER_ALL, sms);

  g_dbus_proxy_call (priv->writeback,
                     "Writeback",
                     g_variant_new ("(@a{sv})",
                                    tracker_resource_serialize (resource)),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     os->cancel,
                     (GAsyncReadyCallback) tracker_store_metadata_cb,
                     os);
  g_object_unref (resource);
}

void
grl_tracker_source_search (GrlSource *source, GrlSourceSearchSpec *ss)
{
  GrlTrackerOp         *os;
  GrlTrackerQueryType   query_type;
  GError               *error = NULL;
  TrackerSparqlStatement *statement;

  GRL_IDEBUG ("%s: id=%u", __FUNCTION__, ss->operation_id);

  if (!ss->text || ss->text[0] == '\0')
    query_type = GRL_TRACKER_QUERY_ALL;
  else
    query_type = GRL_TRACKER_QUERY_FTS_SEARCH;

  statement =
    grl_tracker_source_create_statement (GRL_TRACKER_SOURCE (source),
                                         query_type,
                                         ss->options,
                                         ss->keys,
                                         NULL,
                                         &error);

  if (!statement) {
    ss->callback (ss->source, ss->operation_id, NULL, 0, ss->user_data, error);
    g_error_free (error);
    return;
  }

  os = grl_tracker_op_new (grl_operation_options_get_type_filter (ss->options), ss);

  if (ss->text && *ss->text) {
    /* Make it a prefix search */
    gchar *match = g_strdup_printf ("%s*", ss->text);
    tracker_sparql_statement_bind_string (statement, "match", match);
    g_free (match);
  }

  tracker_sparql_statement_execute_async (statement,
                                          os->cancel,
                                          (GAsyncReadyCallback) tracker_search_cb,
                                          os);
  g_clear_object (&statement);
}

static gboolean
is_root_box (GrlMedia *container)
{
  if (container == NULL)
    return TRUE;
  if (!grl_media_get_id (container))
    return TRUE;
  return FALSE;
}

static void
grl_tracker_source_browse_category (GrlSource *source,
                                    GrlSourceBrowseSpec *bs)
{
  GrlTrackerOp         *os;
  GrlMedia             *media;
  const gchar          *category;
  GError               *error = NULL;
  gint remaining;
  GrlTypeFilter filter = grl_operation_options_get_type_filter (bs->options);
  TrackerSparqlStatement *statement;

  GRL_IDEBUG ("%s: id=%u", __FUNCTION__, bs->operation_id);

  /* If the category is missing, try to get it from the
   * container's ID */
  if (!is_root_box (bs->container) &&
      !grl_data_has_key (GRL_DATA (bs->container),
                         grl_metadata_key_tracker_category)) {
    const char *id;

    id = grl_media_get_id (bs->container);
    if (g_strcmp0 (id, "music") == 0)
      category = "nmm:MusicPiece";
    else if (g_strcmp0 (id, "photos") == 0)
      category = "nmm:Photo";
    else if (g_strcmp0 (id, "videos") == 0)
      category = "nmm:Video";
    else {
      GError *error;

      error = g_error_new (GRL_CORE_ERROR,
                           GRL_CORE_ERROR_BROWSE_FAILED,
                           _("ID “%s” is not known in this source"),
                           id);

      bs->callback (bs->source, bs->operation_id, NULL, 0,
                    bs->user_data, error);

      g_error_free (error);
      return;
    }

    grl_data_set_string (GRL_DATA (bs->container),
                         grl_metadata_key_tracker_category,
                         category);
  }

  if (is_root_box (bs->container)) {
    /* Hardcoded categories */
    if (filter == GRL_TYPE_FILTER_ALL) {
      remaining = 3;
    } else {
      remaining = 0;
      if (filter & GRL_TYPE_FILTER_AUDIO) {
        remaining++;
      }
      if (filter & GRL_TYPE_FILTER_VIDEO) {
        remaining++;
      }
      if (filter & GRL_TYPE_FILTER_IMAGE) {
        remaining++;
      }
    }

    if (remaining == 0) {
      bs->callback (bs->source, bs->operation_id, NULL, 0,
                    bs->user_data, NULL);
      return;
    }

    /* Special case: if everthing is filtered except one category, then skip the
       intermediate level and go straightly to the elements */
    if (remaining > 1) {
      if (filter & GRL_TYPE_FILTER_AUDIO) {
        media = grl_media_container_new ();
        grl_media_set_title (media, "Music");
        grl_media_set_id (media, "music");
        grl_data_set_string (GRL_DATA (media),
                             grl_metadata_key_tracker_category,
                             "nmm:MusicPiece");
        bs->callback (bs->source, bs->operation_id, media, --remaining,
                      bs->user_data, NULL);
      }

      if (filter & GRL_TYPE_FILTER_IMAGE) {
        media = grl_media_container_new ();
        grl_media_set_title (media, "Photos");
        grl_media_set_id (media, "photos");
        grl_data_set_string (GRL_DATA (media),
                             grl_metadata_key_tracker_category,
                             "nmm:Photo");
        bs->callback (bs->source, bs->operation_id, media, --remaining,
                      bs->user_data, NULL);
      }

      if (filter & GRL_TYPE_FILTER_VIDEO) {
        media = grl_media_container_new ();
        grl_media_set_title (media, "Videos");
        grl_media_set_id (media, "videos");
        grl_data_set_string (GRL_DATA (media),
                             grl_metadata_key_tracker_category,
                             "nmm:Video");
        bs->callback (bs->source, bs->operation_id, media, --remaining,
                      bs->user_data, NULL);
      }
      return;
    }
  } else if (grl_data_has_key (GRL_DATA (bs->container),
                               grl_metadata_key_tracker_category)) {
    category = grl_data_get_string (GRL_DATA (bs->container),
                                    grl_metadata_key_tracker_category);

    if (g_strcmp0 (category, "nmm:MusicPiece") == 0)
      grl_operation_options_set_type_filter (bs->options, GRL_TYPE_FILTER_AUDIO);
    else if (g_strcmp0 (category, "nmm:Video") == 0)
      grl_operation_options_set_type_filter (bs->options, GRL_TYPE_FILTER_VIDEO);
    else if (g_strcmp0 (category, "nmm:Photo") == 0)
      grl_operation_options_set_type_filter (bs->options, GRL_TYPE_FILTER_IMAGE);
    else {
      bs->callback (bs->source, bs->operation_id, NULL, 0,
                    bs->user_data, error);
      return;
    }
  } else {
    GError *error;

    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_BROWSE_FAILED,
                         _("ID “%s” is not known in this source"),
                         grl_media_get_id (bs->container));

    bs->callback (bs->source, bs->operation_id, NULL, 0,
                  bs->user_data, error);

    g_error_free (error);
    return;
  }

  /* Use QUERY_ALL here, we use the filter type to browse specific categories */
  statement = grl_tracker_source_create_statement (GRL_TRACKER_SOURCE (source),
                                                   GRL_TRACKER_QUERY_ALL,
                                                   bs->options,
                                                   bs->keys,
                                                   NULL,
                                                   &error);
  if (!statement) {
    bs->callback (bs->source, bs->operation_id, NULL, 0,
                  bs->user_data, error);
    g_error_free (error);
    return;
  }

  os = grl_tracker_op_new (grl_operation_options_get_type_filter (bs->options), bs);

  tracker_sparql_statement_execute_async (statement,
                                          os->cancel,
                                          (GAsyncReadyCallback) tracker_browse_cb,
                                          os);
  g_clear_object (&statement);
}

void
grl_tracker_source_browse (GrlSource *source,
                           GrlSourceBrowseSpec *bs)
{
  /* Ensure GRL_METADATA_KEY_ID is always requested */
  if (!g_list_find (bs->keys, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ID)))
    bs->keys = g_list_prepend (bs->keys, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ID));

  grl_tracker_source_browse_category (source, bs);
}

void
grl_tracker_source_cancel (GrlSource *source, guint operation_id)
{
  GrlTrackerOp *os;

  GRL_IDEBUG ("%s: id=%u", __FUNCTION__, operation_id);

  os = g_hash_table_lookup (grl_tracker_operations,
                            GSIZE_TO_POINTER (operation_id));

  if (os != NULL)
    g_cancellable_cancel (os->cancel);
}

gboolean
grl_tracker_source_change_start (GrlSource *source, GError **error)
{
  GrlTrackerSourcePrivate *priv = GRL_TRACKER_SOURCE (source)->priv;

  priv->notifier =
    grl_tracker_source_notify_new (source, priv->tracker_connection);

  return TRUE;
}

gboolean
grl_tracker_source_change_stop (GrlSource *source, GError **error)
{
  GrlTrackerSourcePrivate *priv = GRL_TRACKER_SOURCE (source)->priv;

  g_clear_object (&priv->notifier);

  return TRUE;
}

void
grl_tracker_source_init_requests (void)
{
  GrlRegistry *registry = grl_registry_get_default ();

  grl_metadata_key_tracker_category =
    grl_registry_lookup_metadata_key (registry, "tracker-category");

  grl_tracker_operations = g_hash_table_new (g_direct_hash, g_direct_equal);

  GRL_LOG_DOMAIN_INIT (tracker_source_request_log_domain,
                       "tracker-source-request");
  GRL_LOG_DOMAIN_INIT (tracker_source_result_log_domain,
                       "tracker-source-result");
}

GrlCaps *
grl_tracker_source_get_caps (GrlSource *source,
                             GrlSupportedOps operation)
{
  static GrlCaps *caps;

  if (!caps) {
    GList *range_list;
    caps = grl_caps_new ();
    grl_caps_set_type_filter (caps, GRL_TYPE_FILTER_ALL);
    range_list = grl_metadata_key_list_new (GRL_METADATA_KEY_DURATION,
                                            GRL_METADATA_KEY_INVALID);
    grl_caps_set_key_range_filter (caps, range_list);
    g_list_free (range_list);
  }

  return caps;
}

GrlSupportedOps
grl_tracker_source_supported_operations (GrlSource *source)
{
  gboolean is_extractor;
  GrlSupportedOps ops;

  /* Always supported operations. */
  ops = GRL_OP_RESOLVE | GRL_OP_MEDIA_FROM_URI | GRL_OP_SEARCH | GRL_OP_QUERY |
        GRL_OP_STORE_METADATA | GRL_OP_NOTIFY_CHANGE;

  /* The extractor doesn’t support browsing; only resolving. */
  is_extractor = g_str_has_prefix (grl_source_get_id (source),
                                   "http://www.tracker-project.org"
                                   "/ontologies/tracker"
                                   "#extractor-data-source,");
  if (!is_extractor) {
    ops |= GRL_OP_BROWSE;
  }

  return ops;
}

gboolean
grl_tracker_source_test_media_from_uri (GrlSource *source,
                                        const gchar *uri)
{
  GError               *error = NULL;
  TrackerSparqlCursor  *cursor;
  TrackerSparqlStatement *statement;
  gboolean              empty;

  statement = grl_tracker_source_create_statement (GRL_TRACKER_SOURCE (source),
                                                   GRL_TRACKER_QUERY_MEDIA_FROM_URI,
                                                   NULL, NULL, NULL,
                                                   &error);
  if (!statement) {
    g_critical ("Error creating statement: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  tracker_sparql_statement_bind_string (statement, "uri", uri);
  cursor = tracker_sparql_statement_execute (statement, NULL, &error);
  g_object_unref (statement);

  if (error) {
    GRL_WARNING ("Error when executig sparql query: %s",
                 error->message);
    g_error_free (error);
    return FALSE;
  }

  /* Check if there are results */
  if (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
    empty = FALSE;
  } else {
    empty = TRUE;
  }

  g_object_unref (cursor);

  return !empty;
}

void
grl_tracker_source_get_media_from_uri (GrlSource *source,
                                       GrlSourceMediaFromUriSpec *mfus)
{
  GError               *error = NULL;
  GrlTrackerOp         *os;
  TrackerSparqlStatement *statement;

  GRL_IDEBUG ("%s: id=%u", __FUNCTION__, mfus->operation_id);

  /* Ensure GRL_METADATA_KEY_ID is always requested */
  if (!g_list_find (mfus->keys, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ID)))
    mfus->keys = g_list_prepend (mfus->keys, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ID));

  statement = grl_tracker_source_create_statement (GRL_TRACKER_SOURCE (source),
                                                   GRL_TRACKER_QUERY_MEDIA_FROM_URI,
                                                   mfus->options,
                                                   mfus->keys,
                                                   NULL,
                                                   &error);
  if (!statement) {
    mfus->callback (source, mfus->operation_id, NULL, NULL, error);
    g_error_free (error);
    return;
  }

  os = grl_tracker_op_new (GRL_TYPE_FILTER_ALL, mfus);

  tracker_sparql_statement_bind_string (statement, "uri", mfus->uri);
  tracker_sparql_statement_execute_async (statement,
                                          os->cancel,
                                          (GAsyncReadyCallback) tracker_media_from_uri_cb,
                                          os);

  g_clear_object (&statement);
}
