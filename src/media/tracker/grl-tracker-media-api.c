/*
 * Copyright (C) 2011 Intel Corporation.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
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
#include "grl-tracker-media-api.h"
#include "grl-tracker-media-cache.h"
#include "grl-tracker-media-priv.h"
#include "grl-tracker-request-queue.h"
#include "grl-tracker-utils.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT tracker_media_request_log_domain

GRL_LOG_DOMAIN_STATIC(tracker_media_request_log_domain);
GRL_LOG_DOMAIN_STATIC(tracker_media_result_log_domain);

/* Inputs/requests */
#define GRL_IDEBUG(args...)                     \
  GRL_LOG (tracker_media_request_log_domain,    \
           GRL_LOG_LEVEL_DEBUG, args)

/* Outputs/results */
#define GRL_ODEBUG(args...)                     \
  GRL_LOG (tracker_media_result_log_domain,     \
           GRL_LOG_LEVEL_DEBUG, args)

/* ------- Definitions ------- */

#define TRACKER_QUERY_REQUEST                   \
  "SELECT rdf:type(?urn) %s "                   \
  "WHERE { %s . %s } "                          \
  "ORDER BY DESC(nfo:fileLastModified(?urn)) "  \
  "OFFSET %i "                                  \
  "LIMIT %i"

#define TRACKER_SEARCH_REQUEST                  \
  "SELECT rdf:type(?urn) %s "                   \
  "WHERE "                                      \
  "{ "                                          \
  "?urn a nfo:Media . "                         \
  "?urn tracker:available ?tr . "               \
  "?urn fts:match '*%s*' . "                    \
  "%s "                                         \
  "} "                                          \
  "ORDER BY DESC(nfo:fileLastModified(?urn)) "  \
  "OFFSET %i "                                  \
  "LIMIT %i"

#define TRACKER_SEARCH_ALL_REQUEST              \
  "SELECT rdf:type(?urn) %s "                   \
  "WHERE "                                      \
  "{ "                                          \
  "?urn a nfo:Media . "                         \
  "?urn tracker:available ?tr . "               \
  "%s "                                         \
  "} "                                          \
  "ORDER BY DESC(nfo:fileLastModified(?urn)) "  \
  "OFFSET %i "                                  \
  "LIMIT %i"

#define TRACKER_BROWSE_CATEGORY_REQUEST         \
  "SELECT rdf:type(?urn) %s "                   \
  "WHERE "                                      \
  "{ "                                          \
  "?urn a %s . "                                \
  "?urn tracker:available ?tr . "               \
  "%s "                                         \
  "} "                                          \
  "ORDER BY DESC(nfo:fileLastModified(?urn)) "  \
  "OFFSET %i "                                  \
  "LIMIT %i"

#define TRACKER_BROWSE_FILESYSTEM_ROOT_REQUEST          \
  "SELECT DISTINCT rdf:type(?urn) %s "                  \
  "WHERE "                                              \
  "{ "                                                  \
  "{ ?urn a nfo:Folder } UNION "                        \
  "{ ?urn a nfo:Audio } UNION "                         \
  "{ ?urn a nfo:Document } UNION "                      \
  "{ ?urn a nmm:Photo } UNION "                         \
  "{ ?urn a nmm:Video } . "                             \
  "%s "                                                 \
  "FILTER (!bound(nfo:belongsToContainer(?urn))) "      \
  "} "                                                  \
  "ORDER BY DESC(nfo:fileLastModified(?urn)) "          \
  "OFFSET %i "                                          \
  "LIMIT %i"

#define TRACKER_BROWSE_FILESYSTEM_REQUEST                       \
  "SELECT DISTINCT rdf:type(?urn) %s "                          \
  "WHERE "                                                      \
  "{ "                                                          \
  "{ ?urn a nfo:Folder } UNION "                                \
  "{ ?urn a nfo:Audio } UNION "                                 \
  "{ ?urn a nfo:Document } UNION "                              \
  "{ ?urn a nmm:Photo } UNION "                                 \
  "{ ?urn a nmm:Video } . "                                     \
  "%s "                                                         \
  "FILTER(tracker:id(nfo:belongsToContainer(?urn)) = %s) "      \
  "} "                                                          \
  "ORDER BY DESC(nfo:fileLastModified(?urn)) "                  \
  "OFFSET %i "                                                  \
  "LIMIT %i"

#define TRACKER_METADATA_REQUEST                \
  "SELECT %s "                                  \
  "WHERE "                                      \
  "{ "                                          \
  "?urn a nie:DataObject . "                    \
  "FILTER (tracker:id(?urn) = %s) "             \
  "}"

#define TRACKER_SAVE_REQUEST                            \
  "DELETE { <%s> %s } WHERE { <%s> a nfo:Media . %s } " \
  "INSERT { <%s> a nfo:Media ; %s . }"

/**/

/**/

static GrlKeyID    grl_metadata_key_tracker_category;
static GHashTable *grl_tracker_operations;

/**/


/**/

/**/

static void
fill_grilo_media_from_sparql (GrlTrackerMedia    *source,
                              GrlMedia            *media,
                              TrackerSparqlCursor *cursor,
                              gint                 column)
{
  const gchar *sparql_key = tracker_sparql_cursor_get_variable_name (cursor,
                                                                     column);
  tracker_grl_sparql_t *assoc =
    grl_tracker_get_mapping_from_sparql (sparql_key);
  union {
    gint int_val;
    gdouble double_val;
    const gchar *str_val;
  } val;

  if (assoc == NULL)
    return;

  GRL_ODEBUG ("\tSetting media prop (col=%i/var=%s/prop=%s) %s",
              column,
              sparql_key,
              g_param_spec_get_name (G_PARAM_SPEC (assoc->grl_key)),
              tracker_sparql_cursor_get_string (cursor, column, NULL));

  if (tracker_sparql_cursor_is_bound (cursor, column) == FALSE) {
    GRL_ODEBUG ("\t\tDropping, no data");
    return;
  }

  if (grl_data_has_key (GRL_DATA (media), assoc->grl_key)) {
    GRL_ODEBUG ("\t\tDropping, already here");
    return;
  }

  if (assoc->set_value) {
    assoc->set_value (cursor, column, media, assoc->grl_key);
  } else {
    switch (G_PARAM_SPEC (assoc->grl_key)->value_type) {
      case G_TYPE_STRING:
        /* Cache the source associated to this result. */
        if (assoc->grl_key == GRL_METADATA_KEY_ID) {
          grl_tracker_media_cache_add_item (grl_tracker_item_cache,
                                            tracker_sparql_cursor_get_integer (cursor,
                                                                               column),
                                            source);
        }
        val.str_val = tracker_sparql_cursor_get_string (cursor, column, NULL);
        if (val.str_val != NULL)
          grl_data_set_string (GRL_DATA (media), assoc->grl_key, val.str_val);
        break;

      case G_TYPE_INT:
        val.int_val = tracker_sparql_cursor_get_integer (cursor, column);
        grl_data_set_int (GRL_DATA (media), assoc->grl_key, val.int_val);
        break;

      case G_TYPE_FLOAT:
        val.double_val = tracker_sparql_cursor_get_double (cursor, column);
        grl_data_set_float (GRL_DATA (media), assoc->grl_key, (gfloat) val.double_val);
        break;

      default:
        GRL_ODEBUG ("\t\tUnexpected data type");
        break;
    }
  }
}

/* I can haz templatze ?? */
#define TRACKER_QUERY_CB(spec_type,name)                             \
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
      GRL_ODEBUG ("\tOperation %u cancelled", spec->name##_id);         \
      spec->callback (spec->source,                                     \
                      spec->name##_id,                                  \
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
                     spec->name##_id, tracker_error->message);          \
                                                                        \
        error = g_error_new (GRL_CORE_ERROR,                            \
                             GRL_CORE_ERROR_BROWSE_FAILED,              \
                             "Failed to start query action : %s",       \
                             tracker_error->message);                   \
                                                                        \
        spec->callback (spec->source,                                   \
                        spec->name##_id,                                \
                        NULL, 0,                                        \
                        spec->user_data, error);                        \
                                                                        \
        g_error_free (error);                                           \
        g_error_free (tracker_error);                                   \
      } else {                                                          \
        GRL_ODEBUG ("\tend of parsing id=%u :)", spec->name##_id);      \
                                                                        \
        /* Only emit this last one if more result than expected */      \
        if (os->count > 1)                                              \
          spec->callback (spec->source,                                 \
                          spec->name##_id,                              \
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
    media = grl_tracker_build_grilo_media (sparql_type);                \
                                                                        \
    if (media != NULL) {                                                \
      for (col = 1 ;                                                    \
           col < tracker_sparql_cursor_get_n_columns (os->cursor) ;     \
           col++) {                                                     \
        fill_grilo_media_from_sparql (GRL_TRACKER_MEDIA (spec->source), \
                                      media, os->cursor, col);          \
      }                                                                 \
                                                                        \
      spec->callback (spec->source,                                     \
                      spec->name##_id,                                  \
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
      grl_tracker_media_get_tracker_connection (GRL_TRACKER_MEDIA (spec->source)); \
                                                                        \
    GRL_ODEBUG ("%s", __FUNCTION__);                                    \
                                                                        \
    os->cursor =                                                        \
      tracker_sparql_connection_query_finish (connection,               \
                                              result, &tracker_error);  \
                                                                        \
    if (tracker_error) {                                                \
      GRL_WARNING ("Could not execute sparql query id=%u: %s",          \
                   spec->name##_id, tracker_error->message);            \
                                                                        \
      error = g_error_new (GRL_CORE_ERROR,                              \
                           GRL_CORE_ERROR_BROWSE_FAILED,                \
                           "Failed to start query action : %s",         \
                           tracker_error->message);                     \
                                                                        \
      spec->callback (spec->source, spec->name##_id, NULL, 0,           \
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

TRACKER_QUERY_CB(GrlMediaSourceQuerySpec, query)
TRACKER_QUERY_CB(GrlMediaSourceBrowseSpec, browse)
TRACKER_QUERY_CB(GrlMediaSourceSearchSpec, search)

static void
tracker_metadata_cb (GObject      *source_object,
                     GAsyncResult *result,
                     GrlTrackerOp *os)
{
  GrlMediaSourceMetadataSpec *ms            = (GrlMediaSourceMetadataSpec *) os->data;
  GrlTrackerMediaPriv        *priv          = GRL_TRACKER_MEDIA_GET_PRIVATE (ms->source);
  gint                        col;
  GError                     *tracker_error = NULL, *error = NULL;
  TrackerSparqlCursor        *cursor;

  GRL_ODEBUG ("%s", __FUNCTION__);

  cursor = tracker_sparql_connection_query_finish (priv->tracker_connection,
                                                   result, &tracker_error);

  if (tracker_error) {
    GRL_WARNING ("Could not execute sparql query id=%u : %s",
                 ms->metadata_id, tracker_error->message);

    error = g_error_new (GRL_CORE_ERROR,
			 GRL_CORE_ERROR_BROWSE_FAILED,
			 "Failed to start metadata action : %s",
                         tracker_error->message);

    ms->callback (ms->source, ms->metadata_id, ms->media, ms->user_data, error);

    g_error_free (tracker_error);
    g_error_free (error);

    goto end_operation;
  }


  if (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
    /* Translate Sparql result into Grilo result */
    for (col = 0 ; col < tracker_sparql_cursor_get_n_columns (cursor) ; col++) {
      fill_grilo_media_from_sparql (GRL_TRACKER_MEDIA (ms->source),
                                    ms->media, cursor, col);
    }

    ms->callback (ms->source, ms->metadata_id, ms->media, ms->user_data, NULL);
  }

 end_operation:
  if (cursor)
    g_object_unref (G_OBJECT (cursor));

  grl_tracker_queue_done (grl_tracker_queue, os);
}

static void
tracker_set_metadata_cb (GObject      *source_object,
                         GAsyncResult *result,
                         GrlTrackerOp *os)
{
  GrlMetadataSourceSetMetadataSpec *sms =
    (GrlMetadataSourceSetMetadataSpec *) os->data;
  GrlTrackerMediaPriv *priv = GRL_TRACKER_MEDIA_GET_PRIVATE (sms->source);
  GError *tracker_error = NULL, *error = NULL;

  tracker_sparql_connection_update_finish (priv->tracker_connection,
                                           result,
                                           &tracker_error);

  if (tracker_error) {
    GRL_WARNING ("Could not execute sparql update : %s",
                 tracker_error->message);

    error = g_error_new (GRL_CORE_ERROR,
			 GRL_CORE_ERROR_SET_METADATA_FAILED,
			 "Failed to set metadata : %s",
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
grl_tracker_media_writable_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_PLAY_COUNT,
                                      GRL_METADATA_KEY_LAST_PLAYED,
#ifdef TRACKER_0_10_5
                                      GRL_METADATA_KEY_LAST_POSITION,
#endif
                                      NULL);
  }
  return keys;
}

/**
 * Query is a SPARQL query.
 *
 * Columns must be named with the Grilo key name that the column
 * represent. Unnamed or unknown columns will be ignored.
 *
 * First column must be the media type, and it does not need to be named.  It
 * must match with any value supported in rdf:type() property, or
 * grilo#Box. Types understood are:
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
 *       <literal>grilo#Box</literal>
 *     </para>
 *   </listitem>
 * </itemizedlist>
 *
 * An example for searching all songs:
 *
 * <informalexample>
 *   <programlisting>
 *     SELECT rdf:type(?song)
 *            ?song            AS id
 *            nie:title(?song) AS title
 *            nie:url(?song)   AS url
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
grl_tracker_media_query (GrlMediaSource *source,
                         GrlMediaSourceQuerySpec *qs)
{
  GError               *error = NULL;
  GrlTrackerMediaPriv *priv  = GRL_TRACKER_MEDIA_GET_PRIVATE (source);
  gchar                *constraint;
  gchar                *sparql_final;
  gchar                *sparql_select;
  GrlTrackerOp         *os;

  GRL_IDEBUG ("%s: id=%u", __FUNCTION__, qs->query_id);

  if (!qs->query || qs->query[0] == '\0') {
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_QUERY_FAILED,
                                 "Empty query");
    goto send_error;
  }

  /* Check if it is a full sparql query */
  if (g_ascii_strncasecmp (qs->query, "select ", 7) != 0) {
    constraint = grl_tracker_media_get_device_constraint (priv);
    sparql_select = grl_tracker_media_get_select_string (qs->keys);
    sparql_final = g_strdup_printf (TRACKER_QUERY_REQUEST,
                                    sparql_select,
                                    qs->query,
                                    constraint,
                                    qs->skip,
                                    qs->count);
    g_free (constraint);
    g_free (qs->query);
    g_free (sparql_select);
    qs->query = sparql_final;
    grl_tracker_media_query (source, qs);
    return;
  }

  GRL_IDEBUG ("\tselect : '%s'", qs->query);

  os = grl_tracker_op_initiate_query (qs->query_id,
                                      g_strdup (qs->query),
                                      (GAsyncReadyCallback) tracker_query_cb,
                                      qs);

  os->keys  = qs->keys;
  os->skip  = qs->skip;
  os->count = qs->count;
  os->data  = qs;
  /* os->cb.sr     = qs->callback; */
  /* os->user_data = qs->user_data; */

  grl_tracker_queue_push (grl_tracker_queue, os);

  return;

 send_error:
  qs->callback (qs->source, qs->query_id, NULL, 0, qs->user_data, error);
  g_error_free (error);
}

void
grl_tracker_media_metadata (GrlMediaSource *source,
                            GrlMediaSourceMetadataSpec *ms)
{
  GrlTrackerMediaPriv *priv       = GRL_TRACKER_MEDIA_GET_PRIVATE (source);
  gchar               *constraint = NULL, *sparql_select, *sparql_final;
  GrlTrackerOp        *os;

  GRL_IDEBUG ("%s: id=%i", __FUNCTION__, ms->metadata_id);

  if (grl_media_get_id (ms->media) == NULL) {
    if (grl_tracker_per_device_source) {
      constraint = grl_tracker_media_get_device_constraint (priv);
      sparql_select = grl_tracker_media_get_select_string (ms->keys);
      sparql_final = g_strdup_printf (TRACKER_BROWSE_FILESYSTEM_ROOT_REQUEST,
                                      sparql_select, constraint, 0, 1);
    } else {
      ms->callback (ms->source, ms->metadata_id, ms->media, ms->user_data, NULL);
      return;
    }
  } else {
    sparql_select = grl_tracker_media_get_select_string (ms->keys);
    sparql_final = g_strdup_printf (TRACKER_METADATA_REQUEST, sparql_select,
                                    grl_media_get_id (ms->media));
  }

  GRL_IDEBUG ("\tselect: '%s'", sparql_final);

  os = grl_tracker_op_initiate_metadata (sparql_final,
                                         (GAsyncReadyCallback) tracker_metadata_cb,
                                         ms);
  os->keys = ms->keys;

  grl_tracker_queue_push (grl_tracker_queue, os);

  if (constraint != NULL)
    g_free (constraint);
  if (sparql_select != NULL)
    g_free (sparql_select);
}

void
grl_tracker_media_set_metadata (GrlMetadataSource *source,
                                GrlMetadataSourceSetMetadataSpec *sms)
{
  /* GrlTrackerMediaPriv *priv = GRL_TRACKER_MEDIA_GET_PRIVATE (source); */
  gchar *sparql_delete, *sparql_cdelete, *sparql_insert, *sparql_final;
  const gchar *urn = grl_data_get_string (GRL_DATA (sms->media),
                                          grl_metadata_key_tracker_urn);
  GrlTrackerOp *os;

  GRL_IDEBUG ("%s: urn=%s", G_STRFUNC, urn);

  sparql_delete = grl_tracker_get_delete_string (sms->keys);
  sparql_cdelete = grl_tracker_get_delete_conditional_string (urn, sms->keys);
  sparql_insert = grl_tracker_tracker_get_insert_string (sms->media, sms->keys);
  sparql_final = g_strdup_printf (TRACKER_SAVE_REQUEST,
                                  urn, sparql_delete,
                                  urn, sparql_cdelete,
                                  urn, sparql_insert);

  os = grl_tracker_op_initiate_set_metadata (sparql_final,
                                             (GAsyncReadyCallback) tracker_set_metadata_cb,
                                             sms);
  os->keys = sms->keys;

  GRL_IDEBUG ("\trequest: '%s'", sparql_final);

  grl_tracker_queue_push (grl_tracker_queue, os);

  g_free (sparql_delete);
  g_free (sparql_cdelete);
  g_free (sparql_insert);
}

void
grl_tracker_media_search (GrlMediaSource *source, GrlMediaSourceSearchSpec *ss)
{
  GrlTrackerMediaPriv *priv  = GRL_TRACKER_MEDIA_GET_PRIVATE (source);
  gchar                *constraint;
  gchar                *sparql_select;
  gchar                *sparql_final;
  GrlTrackerOp         *os;

  GRL_IDEBUG ("%s: id=%u", __FUNCTION__, ss->search_id);

  constraint = grl_tracker_media_get_device_constraint (priv);
  sparql_select = grl_tracker_media_get_select_string (ss->keys);
  if (!ss->text || ss->text[0] == '\0') {
    /* Search all */
    sparql_final = g_strdup_printf (TRACKER_SEARCH_ALL_REQUEST, sparql_select,
                                    constraint, ss->skip, ss->count);
  } else {
    sparql_final = g_strdup_printf (TRACKER_SEARCH_REQUEST, sparql_select,
                                    ss->text, constraint, ss->skip, ss->count);
  }

  GRL_IDEBUG ("\tselect: '%s'", sparql_final);

  os = grl_tracker_op_initiate_query (ss->search_id,
                                      sparql_final,
                                      (GAsyncReadyCallback) tracker_search_cb,
                                      ss);
  os->keys  = ss->keys;
  os->skip  = ss->skip;
  os->count = ss->count;

  grl_tracker_queue_push (grl_tracker_queue, os);

  g_free (constraint);
  g_free (sparql_select);
}

static void
grl_tracker_media_browse_category (GrlMediaSource *source,
                                   GrlMediaSourceBrowseSpec *bs)
{
  GrlTrackerMediaPriv *priv  = GRL_TRACKER_MEDIA_GET_PRIVATE (source);
  gchar                *constraint;
  gchar                *sparql_select;
  gchar                *sparql_final;
  GrlTrackerOp         *os;
  GrlMedia             *media;
  const gchar          *category;

  GRL_IDEBUG ("%s: id=%u", __FUNCTION__, bs->browse_id);

  if (bs->container == NULL ||
      !grl_data_has_key (GRL_DATA (bs->container),
                         grl_metadata_key_tracker_category)) {
    /* Hardcoded categories */
    media = grl_media_box_new ();
    grl_media_set_title (media, "Documents");
    grl_data_set_string (GRL_DATA (media),
                         grl_metadata_key_tracker_category,
                         "nfo:Document");
    bs->callback (bs->source, bs->browse_id, media, 3, bs->user_data, NULL);

    media = grl_media_box_new ();
    grl_media_set_title (media, "Music");
    grl_data_set_string (GRL_DATA (media),
                         grl_metadata_key_tracker_category,
                         "nmm:MusicPiece");
    bs->callback (bs->source, bs->browse_id, media, 2, bs->user_data, NULL);

    media = grl_media_box_new ();
    grl_media_set_title (media, "Photos");
    grl_data_set_string (GRL_DATA (media),
                         grl_metadata_key_tracker_category,
                         "nmm:Photo");
    bs->callback (bs->source, bs->browse_id, media, 1, bs->user_data, NULL);

    media = grl_media_box_new ();
    grl_media_set_title (media, "Videos");
    grl_data_set_string (GRL_DATA (media),
                         grl_metadata_key_tracker_category,
                         "nmm:Video");
    bs->callback (bs->source, bs->browse_id, media, 0, bs->user_data, NULL);
    return;
  }

  category = grl_data_get_string (GRL_DATA (bs->container),
                                  grl_metadata_key_tracker_category);

  constraint = grl_tracker_media_get_device_constraint (priv);
  sparql_select = grl_tracker_media_get_select_string (bs->keys);
  sparql_final = g_strdup_printf (TRACKER_BROWSE_CATEGORY_REQUEST,
                                  sparql_select,
                                  category,
                                  constraint,
                                  bs->skip, bs->count);

  GRL_IDEBUG ("\tselect: '%s'", sparql_final);

  os = grl_tracker_op_initiate_query (bs->browse_id,
                                      sparql_final,
                                      (GAsyncReadyCallback) tracker_browse_cb,
                                      bs);
  os->keys  = bs->keys;
  os->skip  = bs->skip;
  os->count = bs->count;

  grl_tracker_queue_push (grl_tracker_queue, os);

  g_free (constraint);
  g_free (sparql_select);
}

static void
grl_tracker_media_browse_filesystem (GrlMediaSource *source,
                                     GrlMediaSourceBrowseSpec *bs)
{
  GrlTrackerMediaPriv *priv  = GRL_TRACKER_MEDIA_GET_PRIVATE (source);
  gchar                *constraint;
  gchar                *sparql_select;
  gchar                *sparql_final;
  GrlTrackerOp         *os;

  GRL_IDEBUG ("%s: id=%u", __FUNCTION__, bs->browse_id);

  sparql_select = grl_tracker_media_get_select_string (bs->keys);
  constraint = grl_tracker_media_get_device_constraint (priv);

  if (bs->container == NULL ||
      !grl_media_get_id (bs->container)) {
    sparql_final = g_strdup_printf (TRACKER_BROWSE_FILESYSTEM_ROOT_REQUEST,
                                    sparql_select,
                                    constraint,
                                    bs->skip, bs->count);

  } else {
    sparql_final = g_strdup_printf (TRACKER_BROWSE_FILESYSTEM_REQUEST,
                                    sparql_select,
                                    constraint,
                                    grl_media_get_id (bs->container),
                                    bs->skip, bs->count);
  }

  GRL_IDEBUG ("\tselect: '%s'", sparql_final);

  os = grl_tracker_op_initiate_query (bs->browse_id,
                                      sparql_final,
                                      (GAsyncReadyCallback) tracker_browse_cb,
                                      bs);
  os->keys  = bs->keys;
  os->skip  = bs->skip;
  os->count = bs->count;

  grl_tracker_queue_push (grl_tracker_queue, os);

  g_free (constraint);
  g_free (sparql_select);
}

void
grl_tracker_media_browse (GrlMediaSource *source,
                          GrlMediaSourceBrowseSpec *bs)
{
  if (grl_tracker_browse_filesystem)
    grl_tracker_media_browse_filesystem (source, bs);
  else
    grl_tracker_media_browse_category (source, bs);
}

void
grl_tracker_media_cancel (GrlMetadataSource *source, guint operation_id)
{
  GrlTrackerOp *os;

  GRL_IDEBUG ("%s: id=%u", __FUNCTION__, operation_id);

  os = g_hash_table_lookup (grl_tracker_operations,
                            GSIZE_TO_POINTER (operation_id));

  if (os != NULL)
    grl_tracker_queue_cancel (grl_tracker_queue, os);
}

gboolean
grl_tracker_media_change_start (GrlMediaSource *source, GError **error)
{
  GrlTrackerMediaPriv *priv = GRL_TRACKER_MEDIA_GET_PRIVATE (source);

  priv->notify_changes = TRUE;

  return TRUE;
}

gboolean
grl_tracker_media_change_stop (GrlMediaSource *source, GError **error)
{
  GrlTrackerMediaPriv *priv = GRL_TRACKER_MEDIA_GET_PRIVATE (source);

  priv->notify_changes = FALSE;

  return TRUE;
}

void
grl_tracker_media_init_requests (void)
{
  grl_metadata_key_tracker_category =
    grl_plugin_registry_register_metadata_key (grl_plugin_registry_get_default (),
                                               g_param_spec_string ("tracker-category",
                                                                    "Tracker category",
                                                                    "Category a media belongs to",
                                                                    NULL,
                                                                    G_PARAM_STATIC_STRINGS |
                                                                    G_PARAM_READWRITE),
                                               NULL);

  grl_tracker_operations = g_hash_table_new (g_direct_hash, g_direct_equal);

  GRL_LOG_DOMAIN_INIT (tracker_media_request_log_domain,
                       "tracker-media-request");
  GRL_LOG_DOMAIN_INIT (tracker_media_result_log_domain,
                       "tracker-media-result");
}
