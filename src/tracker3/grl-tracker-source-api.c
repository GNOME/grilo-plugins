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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gio/gio.h>
#include <tracker-sparql.h>

#include "grl-tracker.h"
#include "grl-tracker-source-api.h"
#include "grl-tracker-source-cache.h"
#include "grl-tracker-source-priv.h"
#include "grl-tracker-request-queue.h"
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

/* ------- Definitions ------- */

#define TRACKER_QUERY_LIMIT                     \
  "OFFSET %u "                                  \
  "LIMIT %u"

#define TRACKER_QUERY_PARTIAL_REQUEST           \
  "SELECT rdf:type(?urn) %s "                   \
  "WHERE { %s . %s } "                          \
  "ORDER BY DESC(nfo:fileLastModified(?urn)) "  \
  TRACKER_QUERY_LIMIT

#define TRACKER_QUERY_FULL_REQUEST              \
  "%s "                                         \
  TRACKER_QUERY_LIMIT

#define TRACKER_SEARCH_REQUEST                  \
  "SELECT DISTINCT rdf:type(?urn) %s "          \
  "WHERE "                                      \
  "{ "                                          \
  "%s "                                         \
  "?urn tracker:available ?tr . "               \
  "?urn fts:match \"%s\" . "                    \
  "%s %s "                                      \
  "} "                                          \
  "ORDER BY DESC(nfo:fileLastModified(?urn)) "  \
  "OFFSET %u "                                  \
  "LIMIT %u"

#define TRACKER_SEARCH_ALL_REQUEST              \
  "SELECT DISTINCT rdf:type(?urn) %s "          \
  "WHERE "                                      \
  "{ "                                          \
  "%s "                                         \
  "?urn tracker:available ?tr . "               \
  "%s %s "                                      \
  "} "                                          \
  "ORDER BY DESC(nfo:fileLastModified(?urn)) "  \
  "OFFSET %u "                                  \
  "LIMIT %u"

#define TRACKER_BROWSE_SHOW_DOCUMENTS           \
  "{ ?urn a nfo:Document } UNION"

#define TRACKER_BROWSE_CATEGORY_REQUEST         \
  "SELECT rdf:type(?urn) %s "                   \
  "WHERE "                                      \
  "{ "                                          \
  "?urn a %s . "                                \
  "?urn nie:isStoredAs ?file . "                \
  "?file tracker:available ?tr . "              \
  "%s "                                         \
  "%s "                                         \
  "} "                                          \
  "ORDER BY DESC(nfo:fileLastModified(?urn)) "  \
  "OFFSET %u "                                  \
  "LIMIT %u"

#define TRACKER_BROWSE_FILESYSTEM_ROOT_REQUEST          \
  "SELECT DISTINCT rdf:type(?urn) %s "                  \
  "WHERE "                                              \
  "{ "                                                  \
  "%s "                                                 \
  "{ ?urn a nfo:Folder } %s "                           \
  "%s "                                                 \
  "FILTER (!bound(nfo:belongsToContainer(?urn))) "      \
  "} "                                                  \
  "ORDER BY DESC(nfo:fileLastModified(?urn)) "          \
  "OFFSET %u "                                          \
  "LIMIT %u"

#define TRACKER_BROWSE_FILESYSTEM_REQUEST                       \
  "SELECT DISTINCT rdf:type(?urn) %s "                          \
  "WHERE "                                                      \
  "{ "                                                          \
    "%s "                                                       \
  "{ ?urn a nfo:Folder } %s "                                   \
  "%s "                                                         \
  "FILTER(tracker:id(nfo:belongsToContainer(?urn)) = %s) "      \
  "} "                                                          \
  "ORDER BY DESC(nfo:fileLastModified(?urn)) "                  \
  "OFFSET %u "                                                  \
  "LIMIT %u"

#define TRACKER_RESOLVE_REQUEST                 \
  "SELECT %s "                                  \
  "WHERE "                                      \
  "{ "                                          \
  "?urn a nie:InformationElement ; "            \
  "  nie:isStoredAs ?file . "                   \
  "FILTER (tracker:id(?urn) = %s) "             \
  "}"

#define TRACKER_RESOLVE_URL_REQUEST             \
  "SELECT %s "                                  \
  "WHERE "                                      \
  "{ "                                          \
  "?urn a nie:DataObject . "                    \
  "?urn nie:url \"%s\" "                        \
  "}"

#define TRACKER_DELETE_REQUEST                          \
  "DELETE { <%s> %s } WHERE { <%s> a nfo:Media . %s }"

#define TRACKER_SAVE_REQUEST                            \
  "DELETE { <%s> %s } WHERE { <%s> a nfo:Media . %s } " \
  "INSERT { <%s> a nfo:Media ; %s . }"

#define TRACKER_TEST_MEDIA_FROM_URI_REQUEST             \
  "SELECT ?urn "                                        \
  "WHERE "                                              \
  "{ "                                                  \
  "?urn nie:url \"%s\" ; "                              \
  "tracker:available true ; "                           \
  "a nfo:Media . "                                      \
  "%s "                                                 \
  "}"

#define TRACKER_TEST_MEDIA_FROM_URI_REQUEST_WITH_DOCUMENTS  \
  "SELECT ?urn "                                            \
  "WHERE "                                                  \
  "{ "                                                      \
  "?urn nie:url \"%s\" ; "                                  \
  "tracker:available true . "                               \
  "%s "                                                     \
  "FILTER (?type IN ( nfo:Media, nfo:Document ))"           \
  "}"

#define TRACKER_MEDIA_FROM_URI_REQUEST          \
  "SELECT rdf:type(?urn) %s "                   \
  "WHERE "                                      \
  "{ "                                          \
  "?urn nie:url \"%s\" ; "                      \
  "tracker:available ?tr . "                    \
  "%s "                                         \
  "} "                                          \

/**/

/**/

static GrlKeyID    grl_metadata_key_tracker_category;
static GHashTable *grl_tracker_operations;

/**/


/**/

/**/

static void
set_title_from_filename (GrlMedia *media)
{
  const gchar *url;
  gchar *path, *display_name, *ext, *title;
  guint suffix_len;

  url = grl_media_get_url (media);
  if (url == NULL)
    return;

  path = g_filename_from_uri (url, NULL, NULL);
  if (!path)
    return;
  display_name = g_filename_display_basename (path);
  g_free (path);
  ext = strrchr (display_name, '.');
  if (!ext)
    goto out;

  suffix_len = strlen (ext);
  if (suffix_len != 4 && suffix_len != 5)
    goto out;

  title = g_strndup (display_name, ext - display_name);
  if (g_strcmp0 (grl_media_get_title (media), title) == 0)
    grl_data_set_boolean (GRL_DATA (media), GRL_METADATA_KEY_TITLE_FROM_FILENAME, TRUE);
  g_free (title);

out:
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

static gchar *
get_sparql_type_filter (GrlOperationOptions *options,
                        gboolean prepend_union)
{
  GrlTypeFilter filter = grl_operation_options_get_type_filter (options);
  GString *sparql_filter = g_string_new ("");

  if (filter & GRL_TYPE_FILTER_AUDIO) {
    if (prepend_union) {
      sparql_filter = g_string_append (sparql_filter,
                                       "UNION { ?urn a nfo:Audio } ");
    } else {
      sparql_filter = g_string_append (sparql_filter,
                                       "{ ?urn a nfo:Audio } ");
      prepend_union = TRUE;
    }
  }
  if (filter & GRL_TYPE_FILTER_VIDEO) {
    if (prepend_union) {
      sparql_filter = g_string_append (sparql_filter,
                                       "UNION { ?urn a nmm:Video } ");
    } else {
      sparql_filter = g_string_append (sparql_filter,
                                       "{ ?urn a nmm:Video } ");
      prepend_union = TRUE;
    }
  }
  if (filter & GRL_TYPE_FILTER_IMAGE) {
    if (prepend_union) {
      sparql_filter = g_string_append (sparql_filter,
                                       "UNION { ?urn a nmm:Photo } ");
    } else {
      sparql_filter = g_string_append (sparql_filter,
                                       "{ ?urn a nmm:Photo } ");
    }
  }

  sparql_filter = g_string_append_c (sparql_filter, '.');

  return g_string_free (sparql_filter, FALSE);
}

/* I can haz templatze ?? */
#define TRACKER_QUERY_CB(spec_type,name,error)                          \
                                                                        \
  static void                                                           \
  tracker_##name##_result_cb (GObject      *source_object,              \
                              GAsyncResult *result,                     \
                              GrlTrackerOp *os)                         \
  {                                                                     \
    gint         col;                                                   \
    const gchar *sparql_type;                                           \
    GError      *tracker_error = NULL, *error = NULL;                   \
    GrlMedia    *media;                                                 \
    spec_type   *spec =                                                 \
      (spec_type *) os->data;                                           \
                                                                        \
    GRL_ODEBUG ("%s", __FUNCTION__);                                    \
                                                                        \
    if (g_cancellable_is_cancelled (os->cancel)) {                      \
      GRL_ODEBUG ("\tOperation %u cancelled", spec->operation_id);      \
      spec->callback (spec->source,                        \
                      spec->operation_id,                               \
                      NULL, 0,                                          \
                      spec->user_data, NULL);                           \
      grl_tracker_queue_done (grl_tracker_queue, os);                   \
                                                                        \
      return;                                                           \
    }                                                                   \
                                                                        \
    if (!tracker_sparql_cursor_next_finish (os->cursor,                 \
                                            result,                     \
                                            &tracker_error)) {          \
      if (tracker_error != NULL) {                                      \
        GRL_WARNING ("\terror in parsing query id=%u : %s",             \
                     spec->operation_id, tracker_error->message);       \
                                                                        \
        error = g_error_new (GRL_CORE_ERROR,                            \
                             GRL_CORE_ERROR_##error##_FAILED,           \
                             _("Failed to query: %s"),                  \
                             tracker_error->message);                   \
                                                                        \
        spec->callback (spec->source,                      \
                        spec->operation_id,                             \
                        NULL, 0,                                        \
                        spec->user_data, error);                        \
                                                                        \
        g_error_free (error);                                           \
        g_error_free (tracker_error);                                   \
      } else {                                                          \
        GRL_ODEBUG ("\tend of parsing id=%u :)", spec->operation_id);   \
                                                                        \
        /* Only emit this last one if more result than expected */      \
        if (os->count > 1)                                              \
          spec->callback (spec->source,                    \
                          spec->operation_id,                           \
                          NULL, 0,                                      \
                          spec->user_data, NULL);                       \
      }                                                                 \
                                                                        \
      grl_tracker_queue_done (grl_tracker_queue, os);                   \
      return;                                                           \
    }                                                                   \
                                                                        \
    sparql_type = tracker_sparql_cursor_get_string (os->cursor,         \
                                                    0,                  \
                                                    NULL);              \
                                                                        \
    GRL_ODEBUG ("\tParsing line %i of type %s",                         \
                os->current, sparql_type);                              \
                                                                        \
    media = grl_tracker_build_grilo_media (sparql_type, os->type_filter);\
                                                                        \
    if (media != NULL) {                                                \
      for (col = 1 ;                                                    \
           col < tracker_sparql_cursor_get_n_columns (os->cursor) ;     \
           col++) {                                                     \
        fill_grilo_media_from_sparql (GRL_TRACKER_SOURCE (spec->source), \
                                      media, os->cursor, col);          \
      }                                                                 \
      set_title_from_filename (media);                                  \
                                                                        \
      spec->callback (spec->source,                                     \
                      spec->operation_id,                               \
                      media,                                            \
                      --os->count,                                      \
                      spec->user_data,                                  \
                      NULL);                                            \
    }                                                                   \
                                                                        \
    /* Schedule the next line to parse */                               \
    os->current++;                                                      \
    if (os->count < 1)                                                  \
      grl_tracker_queue_done (grl_tracker_queue, os);                   \
    else                                                                \
      tracker_sparql_cursor_next_async (os->cursor, os->cancel,         \
                                        (GAsyncReadyCallback) tracker_##name##_result_cb, \
                                        (gpointer) os);                 \
  }                                                                     \
                                                                        \
  static void                                                           \
  tracker_##name##_cb (GObject      *source_object,                     \
                       GAsyncResult *result,                            \
                       GrlTrackerOp *os)                                \
  {                                                                     \
    GError *tracker_error = NULL, *error = NULL;                        \
    spec_type *spec = (spec_type *) os->data;                           \
    TrackerSparqlConnection *connection =                               \
      grl_tracker_source_get_tracker_connection (GRL_TRACKER_SOURCE (spec->source)); \
                                                                        \
    GRL_ODEBUG ("%s", __FUNCTION__);                                    \
                                                                        \
    os->cursor =                                                        \
      tracker_sparql_connection_query_finish (connection,               \
                                              result, &tracker_error);  \
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
      grl_tracker_queue_done (grl_tracker_queue, os);                   \
                                                                        \
      return;                                                           \
    }                                                                   \
                                                                        \
    /* Start parsing results */                                         \
    os->current = 0;                                                    \
    tracker_sparql_cursor_next_async (os->cursor, NULL,                 \
                                      (GAsyncReadyCallback) tracker_##name##_result_cb, \
                                      (gpointer) os);                   \
  }

TRACKER_QUERY_CB(GrlSourceQuerySpec, query, QUERY)
TRACKER_QUERY_CB(GrlSourceBrowseSpec, browse, BROWSE)
TRACKER_QUERY_CB(GrlSourceSearchSpec, search, SEARCH)

static void
tracker_resolve_cb (GObject      *source_object,
                    GAsyncResult *result,
                    GrlTrackerOp *os)
{
  GrlSourceResolveSpec *rs = (GrlSourceResolveSpec *) os->data;
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (rs->source);
  gint                  col;
  GError               *tracker_error = NULL, *error = NULL;
  TrackerSparqlCursor  *cursor;

  GRL_ODEBUG ("%s", __FUNCTION__);

  cursor = tracker_sparql_connection_query_finish (priv->tracker_connection,
                                                   result, &tracker_error);

  if (tracker_error) {
    GRL_WARNING ("Could not execute sparql resolve query : %s",
                 tracker_error->message);

    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_RESOLVE_FAILED,
                         _("Failed to resolve: %s"),
                         tracker_error->message);

    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);

    g_error_free (tracker_error);
    g_error_free (error);

    goto end_operation;
  }


  if (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
    /* Translate Sparql result into Grilo result */
    for (col = 0 ; col < tracker_sparql_cursor_get_n_columns (cursor) ; col++) {
      fill_grilo_media_from_sparql (GRL_TRACKER_SOURCE (rs->source),
                                    rs->media, cursor, col);
    }
    set_title_from_filename (rs->media);

    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
  } else {
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
  }

 end_operation:
  g_clear_object (&cursor);

  grl_tracker_queue_done (grl_tracker_queue, os);
}

static void
tracker_media_from_uri_cb (GObject      *source_object,
                           GAsyncResult *result,
                           GrlTrackerOp *os)
{
  GrlSourceMediaFromUriSpec *mfus = (GrlSourceMediaFromUriSpec *) os->data;
  GrlTrackerSourcePriv      *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (mfus->source);
  GError                    *tracker_error = NULL, *error = NULL;
  GrlMedia                  *media;
  TrackerSparqlCursor       *cursor;
  const gchar               *sparql_type;
  gint                       col;

  GRL_ODEBUG ("%s", __FUNCTION__);

  cursor = tracker_sparql_connection_query_finish (priv->tracker_connection,
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
    sparql_type = tracker_sparql_cursor_get_string (cursor, 0, NULL);
    media = grl_tracker_build_grilo_media (sparql_type, GRL_TYPE_FILTER_NONE);

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

  grl_tracker_queue_done (grl_tracker_queue, os);
}

static void
tracker_store_metadata_cb (GObject      *source_object,
                           GAsyncResult *result,
                           GrlTrackerOp *os)
{
  GrlSourceStoreMetadataSpec *sms =
    (GrlSourceStoreMetadataSpec *) os->data;
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (sms->source);
  GError *tracker_error = NULL, *error = NULL;

  tracker_sparql_connection_update_finish (priv->tracker_connection,
                                           result,
                                           &tracker_error);

  if (tracker_error) {
    GRL_WARNING ("Could not execute sparql update : %s",
                 tracker_error->message);

    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_STORE_METADATA_FAILED,
                         _("Failed to update metadata: %s"),
                         tracker_error->message);

    sms->callback (sms->source, sms->media, NULL, sms->user_data, error);

    g_error_free (tracker_error);
    g_error_free (error);
  } else {
    sms->callback (sms->source, sms->media, NULL, sms->user_data, error);
  }

  grl_tracker_queue_done (grl_tracker_queue, os);
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

    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_PLAY_COUNT,
                                      GRL_METADATA_KEY_LAST_PLAYED,
                                      GRL_METADATA_KEY_LAST_POSITION,
                                      GRL_METADATA_KEY_FAVOURITE,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_TRACK_NUMBER,
                                      GRL_METADATA_KEY_CREATION_DATE,
                                      grl_metadata_key_chromaprint,
                                      NULL);
  }
  return keys;
}

static void
grl_tracker_source_get_duration_min_max (GrlOperationOptions *options,
                                         int                 *min,
                                         int                 *max)
{
  GValue *min_val, *max_val;

  grl_operation_options_get_key_range_filter (options, GRL_METADATA_KEY_DURATION,
                                              &min_val, &max_val);
  if (min_val)
    *min = g_value_get_int (min_val);
  else
    *min = -1;
  if (max_val)
    *max = g_value_get_int (max_val);
  else
    *max = -1;
}

static char *
grl_tracker_source_create_constraint (int min, int max)
{
  if (min <= 0 && max <= 0)
    return g_strdup ("");
  if (max <= 0) {
    return g_strdup_printf ("?urn a nfo:FileDataObject . "
                            "OPTIONAL {"
                            "  ?urn nfo:duration ?duration "
                            "} . "
                            "FILTER(?duration > %d || !BOUND(?duration))",
                             min);
  }
  if (min <= 0) {
    return g_strdup_printf ("?urn a nfo:FileDataObject . "
                            "OPTIONAL {"
                            "  ?urn nfo:duration ?duration "
                            "} . "
                            "FILTER(?duration < %d || !BOUND(?duration))",
                             max);
  }
  return g_strdup_printf ("?urn a nfo:FileDataObject . "
                          "OPTIONAL {"
                          "  ?urn nfo:duration ?duration "
                          "} . "
                          "FILTER(?duration < %d || ?duration > %d || !BOUND(?duration))",
                           max, min);
}

/**
 * Query is a SPARQL query.
 *
 * Columns must be named with the Grilo key name that the column
 * represent. Unnamed or unknown columns will be ignored.
 *
 * First column must be the media type, and it does not need to be named.  It
 * must match with any value supported in rdf:type() property, or
 * grilo#Container. Types understood are:
 *
 * <itemizedlist>
 *   <listitem>
 *     <para>
 *       <literal>nmm#MusicPiece</literal>
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       <literal>nmm#Video</literal>
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       <literal>nmm#Photo</literal>
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       <literal>nmm#Artist</literal>
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       <literal>nmm#MusicAlbum</literal>
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       <literal>grilo#Container</literal>
 *     </para>
 *   </listitem>
 * </itemizedlist>
 *
 * An example for searching all songs:
 *
 * <informalexample>
 *   <programlisting>
 *     SELECT rdf:type(?song)
 *            ?song            AS ?id
 *            nie:title(?song) AS ?title
 *            nie:url(?song)   AS ?url
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
  GrlTrackerSourcePriv *priv  = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  gchar                *constraint;
  gchar                *sparql_final;
  gchar                *sparql_select;
  GrlTrackerOp         *os;
  gint count = grl_operation_options_get_count (qs->options);
  guint skip = grl_operation_options_get_skip (qs->options);

  GRL_IDEBUG ("%s: id=%u", __FUNCTION__, qs->operation_id);

  if (!qs->query || qs->query[0] == '\0') {
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_QUERY_FAILED,
                                 _("Empty query"));
    goto send_error;
  }

  /* Check if it is a full sparql query */
  if (g_ascii_strncasecmp (qs->query, "select ", 7) != 0) {
    constraint = grl_tracker_source_get_device_constraint (priv);
    sparql_select = grl_tracker_source_get_select_string (qs->keys);
    sparql_final = g_strdup_printf (TRACKER_QUERY_PARTIAL_REQUEST,
                                    sparql_select,
                                    qs->query,
                                    constraint,
                                    skip,
                                    count);
    g_free (constraint);
    g_free (qs->query);
    g_free (sparql_select);
    qs->query = sparql_final;
  } else {
    /* Append offset and limit */
    sparql_final = g_strdup_printf (TRACKER_QUERY_FULL_REQUEST,
                                    qs->query,
                                    skip,
                                    count);
    g_free (qs->query);
    qs->query = sparql_final;
  }

  os = grl_tracker_op_initiate_query (qs->operation_id,
                                      g_strdup (qs->query),
                                      (GAsyncReadyCallback) tracker_query_cb,
                                      qs);

  os->keys  = qs->keys;
  os->skip  = skip;
  os->count = count;
  os->type_filter = grl_operation_options_get_type_filter (qs->options);
  os->data  = qs;
  /* os->cb.sr     = qs->callback; */
  /* os->user_data = qs->user_data; */

  grl_tracker_queue_push (grl_tracker_queue, os);

  return;

 send_error:
  qs->callback (qs->source, qs->operation_id, NULL, 0, qs->user_data, error);
  g_error_free (error);
}

void
grl_tracker_source_resolve (GrlSource *source,
                            GrlSourceResolveSpec *rs)
{
  GrlTrackerSourcePriv *priv               = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  gchar                *constraint         = NULL, *sparql_select, *sparql_final;
  gchar                *sparql_type_filter = NULL;
  const gchar          *url                = grl_media_get_url (rs->media);
  GrlTrackerOp         *os;

  GRL_IDEBUG ("%s: id=%i", __FUNCTION__, rs->operation_id);

  /* Check if the media comes from this source or another */
  if (g_strcmp0 (priv->tracker_datasource, grl_source_get_id (rs->source)) == 0) {
    if (grl_media_get_id (rs->media) == NULL) {
      rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
      return;
    } else {
      sparql_select = grl_tracker_source_get_select_string (rs->keys);
      sparql_final = g_strdup_printf (TRACKER_RESOLVE_REQUEST, sparql_select,
                                      grl_media_get_id (rs->media));
    }
  } else {
    if (url) {
      sparql_select = grl_tracker_source_get_select_string (rs->keys);
      sparql_final = g_strdup_printf (TRACKER_RESOLVE_URL_REQUEST,
                                      sparql_select, url);
    } else {
      rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
      return;
    }
  }

  GRL_IDEBUG ("\request: '%s'", sparql_final);

  os = grl_tracker_op_initiate_metadata (sparql_final,
                                         (GAsyncReadyCallback) tracker_resolve_cb,
                                         rs);
  os->keys = rs->keys;

  grl_tracker_queue_push (grl_tracker_queue, os);

  g_clear_pointer (&sparql_type_filter, g_free);
  g_clear_pointer (&constraint, g_free);
  g_clear_pointer (&sparql_select, g_free);
}

gboolean
grl_tracker_source_may_resolve (GrlSource *source,
                                GrlMedia  *media,
                                GrlKeyID   key_id,
                                GList    **missing_keys)
{
  GRL_IDEBUG ("%s: key=%s", __FUNCTION__, GRL_METADATA_KEY_GET_NAME (key_id));

  if (media && grl_tracker_source_find_source (grl_media_get_source (media))) {
    return TRUE;
  }

  if (!grl_tracker_key_is_supported (key_id)) {
    return FALSE;
  }

  if (media) {
    if (grl_media_get_url (media)) {
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
  gchar *sparql_delete, *sparql_cdelete, *sparql_insert, *sparql_final;
  const gchar *urn = grl_data_get_string (GRL_DATA (sms->media),
                                          grl_metadata_key_tracker_urn);
  GrlTrackerOp *os;

  GRL_IDEBUG ("%s: urn=%s", G_STRFUNC, urn);

  sparql_delete = grl_tracker_get_delete_string (sms->keys);
  sparql_cdelete = grl_tracker_get_delete_conditional_string (urn, sms->keys);
  sparql_insert = grl_tracker_tracker_get_insert_string (sms->media, sms->keys);

  if (g_strcmp0 (sparql_insert, "") == 0) {
    sparql_final = g_strdup_printf (TRACKER_DELETE_REQUEST,
                                    urn, sparql_delete,
                                    urn, sparql_cdelete);
  } else {
    sparql_final = g_strdup_printf (TRACKER_SAVE_REQUEST,
                                    urn, sparql_delete,
                                    urn, sparql_cdelete,
                                    urn, sparql_insert);
  }

  os = grl_tracker_op_initiate_set_metadata (sparql_final,
                                             (GAsyncReadyCallback) tracker_store_metadata_cb,
                                             sms);
  os->keys = sms->keys;

  GRL_IDEBUG ("\trequest: '%s'", sparql_final);

  grl_tracker_queue_push (grl_tracker_queue, os);

  g_free (sparql_delete);
  g_free (sparql_cdelete);
  g_free (sparql_insert);
}

void
grl_tracker_source_search (GrlSource *source, GrlSourceSearchSpec *ss)
{
  GrlTrackerSourcePriv *priv  = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  gchar                *constraint;
  gchar                *sparql_select;
  gchar                *sparql_final;
  gchar                *sparql_type_filter;
  gchar                *escaped_text;
  GrlTrackerOp         *os;
  gint count = grl_operation_options_get_count (ss->options);
  guint skip = grl_operation_options_get_skip (ss->options);
  int min_dur, max_dur;
  char *duration_constraint;
  GRL_IDEBUG ("%s: id=%u", __FUNCTION__, ss->operation_id);

  constraint = grl_tracker_source_get_device_constraint (priv);
  sparql_select = grl_tracker_source_get_select_string (ss->keys);
  sparql_type_filter = get_sparql_type_filter (ss->options, FALSE);
  grl_tracker_source_get_duration_min_max (ss->options, &min_dur, &max_dur);
  duration_constraint = grl_tracker_source_create_constraint (min_dur, max_dur);
  if (!ss->text || ss->text[0] == '\0') {
    /* Search all */
    sparql_final = g_strdup_printf (TRACKER_SEARCH_ALL_REQUEST, sparql_select,
                                    constraint, duration_constraint, sparql_type_filter,
                                    skip, count);
  } else {
    escaped_text = tracker_sparql_escape_string (ss->text);
    sparql_final = g_strdup_printf (TRACKER_SEARCH_REQUEST, sparql_select,
                                    sparql_type_filter, escaped_text,
                                    constraint, duration_constraint, skip, count);
    g_free (escaped_text);
  }

  GRL_IDEBUG ("\tselect: '%s'", sparql_final);

  os = grl_tracker_op_initiate_query (ss->operation_id,
                                      sparql_final,
                                      (GAsyncReadyCallback) tracker_search_cb,
                                      ss);
  os->keys  = ss->keys;
  os->skip  = skip;
  os->count = count;
  os->type_filter = grl_operation_options_get_type_filter (ss->options);

  grl_tracker_queue_push (grl_tracker_queue, os);

  g_free (constraint);
  g_free (sparql_select);
  g_free (sparql_type_filter);
  g_free (duration_constraint);
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
  GrlTrackerSourcePriv *priv  = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  gchar                *constraint;
  gchar                *sparql_select;
  gchar                *sparql_final;
  GrlTrackerOp         *os;
  GrlMedia             *media;
  const gchar          *category;
  gint remaining;
  gint count = grl_operation_options_get_count (bs->options);
  guint skip = grl_operation_options_get_skip (bs->options);
  GrlTypeFilter filter = grl_operation_options_get_type_filter (bs->options);
  int min_dur, max_dur;
  char *duration_constraint;

  GRL_IDEBUG ("%s: id=%u", __FUNCTION__, bs->operation_id);

  /* If the category is missing, try to get it from the
   * container's ID */
  if (!is_root_box (bs->container) &&
      !grl_data_has_key (GRL_DATA (bs->container),
                         grl_metadata_key_tracker_category)) {
    const char *id;

    id = grl_media_get_id (bs->container);
    if (g_strcmp0 (id, "documents") == 0)
      category = "nfo:Document";
    else if (g_strcmp0 (id, "music") == 0)
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

  if (is_root_box (bs->container) ||
      !grl_data_has_key (GRL_DATA (bs->container),
                         grl_metadata_key_tracker_category)) {
    /* Hardcoded categories */
    if (filter == GRL_TYPE_FILTER_ALL) {
      remaining = 3;
      if (grl_tracker_show_documents)  {
        remaining++;
      }
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
    if (remaining == 1) {
      if (filter & GRL_TYPE_FILTER_AUDIO) {
        category = "nmm:MusicPiece";
      } else if (filter & GRL_TYPE_FILTER_IMAGE) {
        category = "nmm:Photo";
      } else {
        category = "nmm:Video";
      }
    } else {
      if (remaining == 4) {
        media = grl_media_container_new ();
        grl_media_set_title (media, "Documents");
        grl_media_set_id (media, "documents");
        grl_data_set_string (GRL_DATA (media),
                             grl_metadata_key_tracker_category,
                             "nfo:Document");
        bs->callback (bs->source, bs->operation_id, media, --remaining,
                      bs->user_data, NULL);
      }

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
  } else {
    category = grl_data_get_string (GRL_DATA (bs->container),
                                    grl_metadata_key_tracker_category);
  }

  grl_tracker_source_get_duration_min_max (bs->options, &min_dur, &max_dur);
  duration_constraint = grl_tracker_source_create_constraint (min_dur, max_dur);
  constraint = grl_tracker_source_get_device_constraint (priv);
  sparql_select = grl_tracker_source_get_select_string (bs->keys);
  sparql_final = g_strdup_printf (TRACKER_BROWSE_CATEGORY_REQUEST,
                                  sparql_select,
                                  category,
                                  constraint,
                                  duration_constraint,
                                  skip, count);

  GRL_IDEBUG ("\tselect: '%s'", sparql_final);

  os = grl_tracker_op_initiate_query (bs->operation_id,
                                      sparql_final,
                                      (GAsyncReadyCallback) tracker_browse_cb,
                                      bs);
  os->keys  = bs->keys;
  os->skip  = skip;
  os->count = count;
  os->type_filter = grl_operation_options_get_type_filter (bs->options);

  grl_tracker_queue_push (grl_tracker_queue, os);

  g_free (constraint);
  g_free (sparql_select);
  g_free (duration_constraint);
}

static void
grl_tracker_source_browse_filesystem (GrlSource *source,
                                      GrlSourceBrowseSpec *bs)
{
  GrlTrackerSourcePriv *priv  = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  gchar                *constraint;
  gchar                *sparql_select;
  gchar                *sparql_final;
  gchar                *sparql_type_filter;
  GrlTrackerOp         *os;
  gint count = grl_operation_options_get_count (bs->options);
  guint skip = grl_operation_options_get_skip (bs->options);
  int min_dur, max_dur;
  char *duration_constraint;

  GRL_IDEBUG ("%s: id=%u", __FUNCTION__, bs->operation_id);

  sparql_select = grl_tracker_source_get_select_string (bs->keys);
  constraint = grl_tracker_source_get_device_constraint (priv);
  sparql_type_filter = get_sparql_type_filter (bs->options, TRUE);
  grl_tracker_source_get_duration_min_max (bs->options, &min_dur, &max_dur);
  duration_constraint = grl_tracker_source_create_constraint (min_dur, max_dur);

  if (bs->container == NULL ||
      !grl_media_get_id (bs->container)) {
    sparql_final = g_strdup_printf (TRACKER_BROWSE_FILESYSTEM_ROOT_REQUEST,
                                    sparql_select,
                                    grl_tracker_show_documents? TRACKER_BROWSE_SHOW_DOCUMENTS: "",
                                    sparql_type_filter,
                                    constraint,
                                    skip, count);

  } else {
    sparql_final = g_strdup_printf (TRACKER_BROWSE_FILESYSTEM_REQUEST,
                                    sparql_select,
                                    grl_tracker_show_documents? TRACKER_BROWSE_SHOW_DOCUMENTS: "",
                                    sparql_type_filter,
                                    constraint,
                                    grl_media_get_id (bs->container),
                                    skip, count);
  }

  GRL_IDEBUG ("\tselect: '%s'", sparql_final);

  os = grl_tracker_op_initiate_query (bs->operation_id,
                                      sparql_final,
                                      (GAsyncReadyCallback) tracker_browse_cb,
                                      bs);
  os->keys  = bs->keys;
  os->skip  = skip;
  os->count = count;
  os->type_filter = grl_operation_options_get_type_filter (bs->options);

  grl_tracker_queue_push (grl_tracker_queue, os);

  g_free (sparql_type_filter);
  g_free (constraint);
  g_free (sparql_select);
  g_free (duration_constraint);
}

void
grl_tracker_source_browse (GrlSource *source,
                           GrlSourceBrowseSpec *bs)
{
  /* Ensure GRL_METADATA_KEY_ID is always requested */
  if (!g_list_find (bs->keys, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ID)))
    bs->keys = g_list_prepend (bs->keys, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ID));

  if (grl_tracker_browse_filesystem)
    grl_tracker_source_browse_filesystem (source, bs);
  else
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
    grl_tracker_queue_cancel (grl_tracker_queue, os);
}

gboolean
grl_tracker_source_change_start (GrlSource *source, GError **error)
{
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (source);

  priv->notify_changes = TRUE;

  return TRUE;
}

gboolean
grl_tracker_source_change_stop (GrlSource *source, GError **error)
{
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (source);

  priv->notify_changes = FALSE;

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
  GrlTrackerSourcePriv *priv  = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  GError               *error = NULL;
  TrackerSparqlCursor  *cursor;
  gboolean              empty;
  gchar                *constraint;
  gchar                *sparql_final;

  constraint = grl_tracker_source_get_device_constraint (priv);
  if (grl_tracker_show_documents) {
    sparql_final = g_strdup_printf (TRACKER_TEST_MEDIA_FROM_URI_REQUEST_WITH_DOCUMENTS,
                                    uri,
                                    constraint);
  } else {
    sparql_final = g_strdup_printf (TRACKER_TEST_MEDIA_FROM_URI_REQUEST,
                                    uri,
                                    constraint);
  }

  cursor = tracker_sparql_connection_query (grl_tracker_connection,
                                            sparql_final,
                                            NULL,
                                            &error);
  g_free (constraint);
  g_free (sparql_final);

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
  GrlTrackerSourcePriv *priv  = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  gchar                *constraint;
  gchar                *sparql_select;
  gchar                *sparql_final;
  GrlTrackerOp         *os;

  GRL_IDEBUG ("%s: id=%u", __FUNCTION__, mfus->operation_id);

  /* Ensure GRL_METADATA_KEY_ID is always requested */
  if (!g_list_find (mfus->keys, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ID)))
    mfus->keys = g_list_prepend (mfus->keys, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ID));

  constraint = grl_tracker_source_get_device_constraint (priv);
  sparql_select = grl_tracker_source_get_select_string (mfus->keys);
  sparql_final = g_strdup_printf (TRACKER_MEDIA_FROM_URI_REQUEST,
                                  sparql_select,
                                  mfus->uri,
                                  constraint);

  GRL_IDEBUG ("\tselect: '%s'", sparql_final);

  os = grl_tracker_op_initiate_metadata (sparql_final,
                                         (GAsyncReadyCallback) tracker_media_from_uri_cb,
                                         mfus);
  os->keys  = mfus->keys;

  grl_tracker_queue_push (grl_tracker_queue, os);

  g_free (constraint);
  g_free (sparql_select);
}
