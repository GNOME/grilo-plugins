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

#include <gio/gio.h>
#include <tracker-sparql.h>

#include "grl-tracker-api.h"
#include "grl-tracker-cache.h"
#include "grl-tracker-priv.h"
#include "grl-tracker-utils.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT tracker_request_log_domain
GRL_LOG_DOMAIN_STATIC(tracker_request_log_domain);

/* ------- Definitions ------- */

#define TRACKER_QUERY_REQUEST                  \
  "SELECT rdf:type(?urn) %s "                  \
  "WHERE { %s . %s } "                         \
  "ORDER BY DESC(nfo:fileLastModified(?urn)) " \
  "OFFSET %i "                                 \
  "LIMIT %i"

#define TRACKER_SEARCH_REQUEST                   \
  "SELECT rdf:type(?urn) %s "                    \
  "WHERE "                                       \
  "{ "                                           \
  "?urn a nfo:Media . "                          \
  "?urn tracker:available ?tr . "                \
  "?urn fts:match '*%s*' . "                     \
  "%s "                                          \
  "} "                                           \
  "ORDER BY DESC(nfo:fileLastModified(?urn)) "   \
  "OFFSET %i "                                   \
  "LIMIT %i"

#define TRACKER_SEARCH_ALL_REQUEST               \
  "SELECT rdf:type(?urn) %s "                    \
  "WHERE "                                       \
  "{ "                                           \
  "?urn a nfo:Media . "                          \
  "?urn tracker:available ?tr . "                \
  "%s "                                          \
  "} "                                           \
  "ORDER BY DESC(nfo:fileLastModified(?urn)) "   \
  "OFFSET %i "                                   \
  "LIMIT %i"

#define TRACKER_BROWSE_CATEGORY_REQUEST        \
  "SELECT rdf:type(?urn) %s "                  \
  "WHERE "                                     \
  "{ "                                         \
  "?urn a %s . "                               \
  "?urn tracker:available ?tr . "              \
  "%s "                                        \
  "} "                                         \
  "ORDER BY DESC(nfo:fileLastModified(?urn)) " \
  "OFFSET %i "                                 \
  "LIMIT %i"

#define TRACKER_BROWSE_FILESYSTEM_ROOT_REQUEST                  \
  "SELECT rdf:type(?urn) %s "                                   \
  "WHERE "                                                      \
  "{ "                                                          \
  "{ ?urn a nfo:Folder } UNION "                                \
  "{ ?urn a nfo:Audio } UNION "                                 \
  "{ ?urn a nmm:Photo } UNION "                                 \
  "{ ?urn a nmm:Video } . "                                     \
  "%s "                                                         \
  "FILTER (!bound(nfo:belongsToContainer(?urn))) "              \
  "} "                                                          \
  "ORDER BY DESC(nfo:fileLastModified(?urn)) "                  \
  "OFFSET %i "                                                  \
  "LIMIT %i"

#define TRACKER_BROWSE_FILESYSTEM_REQUEST                       \
  "SELECT rdf:type(?urn) %s "                                   \
  "WHERE "                                                      \
  "{ "                                                          \
  "{ ?urn a nfo:Folder } UNION "                                \
  "{ ?urn a nfo:Audio } UNION "                                 \
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

/**/

struct OperationSpec {
  GrlMediaSource         *source;
  GrlTrackerSourcePriv   *priv;
  guint                   operation_id;
  GCancellable           *cancel_op;
  const GList            *keys;
  guint                   skip;
  guint                   count;
  guint                   current;
  GrlMediaSourceResultCb  callback;
  gpointer                user_data;
  TrackerSparqlCursor    *cursor;
};

/**/

static GrlKeyID grl_metadata_key_tracker_category;

/**/

static struct OperationSpec *
tracker_operation_initiate (GrlMediaSource *source,
                            GrlTrackerSourcePriv *priv,
                            guint operation_id)
{
  struct OperationSpec *os = g_slice_new0 (struct OperationSpec);

  os->source       = source;
  os->priv         = priv;
  os->operation_id = operation_id;
  os->cancel_op    = g_cancellable_new ();

  g_hash_table_insert (priv->operations, GSIZE_TO_POINTER (operation_id), os);

  return os;
}

static void
tracker_operation_terminate (struct OperationSpec *os)
{
  if (os == NULL)
    return;

  g_hash_table_remove (os->priv->operations,
                       GSIZE_TO_POINTER (os->operation_id));

  g_object_unref (G_OBJECT (os->cursor));
  g_object_unref (G_OBJECT (os->cancel_op));
  g_slice_free (struct OperationSpec, os);
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
    gint int_val;
    gdouble double_val;
    const gchar *str_val;
  } val;

  if (assoc == NULL)
    return;

  GRL_DEBUG ("\tSetting media prop (col=%i/var=%s/prop=%s) %s",
             column,
             sparql_key,
             g_param_spec_get_name (G_PARAM_SPEC (assoc->grl_key)),
             tracker_sparql_cursor_get_string (cursor, column, NULL));

  if (tracker_sparql_cursor_is_bound (cursor, column) == FALSE) {
    GRL_DEBUG ("\t\tDropping, no data");
    return;
  }

  if (grl_data_has_key (GRL_DATA (media), assoc->grl_key)) {
    GRL_DEBUG ("\t\tDropping, already here");
    return;
  }

  switch (G_PARAM_SPEC (assoc->grl_key)->value_type) {
  case G_TYPE_STRING:
    /* Cache the source associated to this result. */
    if (assoc->grl_key == GRL_METADATA_KEY_ID) {
      grl_tracker_cache_add_item (grl_tracker_item_cache,
                                  tracker_sparql_cursor_get_integer (cursor, column),
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
    GRL_DEBUG ("\t\tUnexpected data type");
    break;
  }
}

static void
tracker_query_result_cb (GObject              *source_object,
                         GAsyncResult         *result,
                         struct OperationSpec *operation)
{
  gint         col;
  const gchar *sparql_type;
  GError      *tracker_error = NULL, *error = NULL;
  GrlMedia    *media;

  GRL_DEBUG ("%s", __FUNCTION__);

  if (g_cancellable_is_cancelled (operation->cancel_op)) {
    GRL_DEBUG ("\tOperation %u cancelled", operation->operation_id);
    operation->callback (operation->source,
                         operation->operation_id,
                         NULL, 0,
                         operation->user_data, NULL);
    tracker_operation_terminate (operation);

    return;
  }

  if (!tracker_sparql_cursor_next_finish (operation->cursor,
                                          result,
                                          &tracker_error)) {
    if (tracker_error != NULL) {
      GRL_DEBUG ("\terror in parsing : %s", tracker_error->message);

      error = g_error_new (GRL_CORE_ERROR,
                           GRL_CORE_ERROR_BROWSE_FAILED,
                           "Failed to start browse action : %s",
                           tracker_error->message);

      operation->callback (operation->source,
                           operation->operation_id,
                           NULL, 0,
                           operation->user_data, error);

      g_error_free (error);
      g_error_free (tracker_error);
    } else {
      GRL_DEBUG ("\tend of parsing :)");

      /* Only emit this last one if more result than expected */
      if (operation->count > 1)
        operation->callback (operation->source,
                             operation->operation_id,
                             NULL, 0,
                             operation->user_data, NULL);
    }

    tracker_operation_terminate (operation);
    return;
  }

  sparql_type = tracker_sparql_cursor_get_string (operation->cursor, 0, NULL);

  GRL_DEBUG ("\tParsing line %i of type %s", operation->current, sparql_type);

  media = grl_tracker_build_grilo_media (sparql_type);

  if (media != NULL) {
    for (col = 1 ;
         col < tracker_sparql_cursor_get_n_columns (operation->cursor) ;
         col++) {
      fill_grilo_media_from_sparql (GRL_TRACKER_SOURCE (operation->source),
                                    media, operation->cursor, col);
    }

    operation->callback (operation->source,
                         operation->operation_id,
                         media,
                         --operation->count,
                         operation->user_data,
                         NULL);
  }

  /* Schedule the next line to parse */
  operation->current++;
  if (operation->count < 1)
        tracker_operation_terminate (operation);
  else
    tracker_sparql_cursor_next_async (operation->cursor, operation->cancel_op,
                                      (GAsyncReadyCallback) tracker_query_result_cb,
                                      (gpointer) operation);
}

static void
tracker_query_cb (GObject              *source_object,
                  GAsyncResult         *result,
                  struct OperationSpec *operation)
{
  GError *tracker_error = NULL, *error = NULL;

  GRL_DEBUG ("%s", __FUNCTION__);

  operation->cursor =
    tracker_sparql_connection_query_finish (operation->priv->tracker_connection,
                                            result, &tracker_error);

  if (tracker_error) {
    GRL_WARNING ("Could not execute sparql query: %s", tracker_error->message);

    error = g_error_new (GRL_CORE_ERROR,
			 GRL_CORE_ERROR_BROWSE_FAILED,
			 "Failed to start browse action : %s",
                         tracker_error->message);

    operation->callback (operation->source, operation->operation_id, NULL, 0,
                         operation->user_data, error);

    g_error_free (tracker_error);
    g_error_free (error);
    g_slice_free (struct OperationSpec, operation);

    return;
  }

  /* Start parsing results */
  operation->current = 0;
  tracker_sparql_cursor_next_async (operation->cursor, NULL,
                                    (GAsyncReadyCallback) tracker_query_result_cb,
                                    (gpointer) operation);
}

static void
tracker_metadata_cb (GObject                    *source_object,
                     GAsyncResult               *result,
                     GrlMediaSourceMetadataSpec *ms)
{
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (ms->source);
  gint                  col;
  GError               *tracker_error = NULL, *error = NULL;
  TrackerSparqlCursor  *cursor;

  GRL_DEBUG ("%s", __FUNCTION__);

  cursor = tracker_sparql_connection_query_finish (priv->tracker_connection,
                                                   result, &tracker_error);

  if (tracker_error) {
    GRL_WARNING ("Could not execute sparql query: %s", tracker_error->message);

    error = g_error_new (GRL_CORE_ERROR,
			 GRL_CORE_ERROR_BROWSE_FAILED,
			 "Failed to start browse action : %s",
                         tracker_error->message);

    ms->callback (ms->source, NULL, ms->user_data, error);

    g_error_free (tracker_error);
    g_error_free (error);

    goto end_operation;
  }


  tracker_sparql_cursor_next (cursor, NULL, NULL);

  /* Translate Sparql result into Grilo result */
  for (col = 0 ; col < tracker_sparql_cursor_get_n_columns (cursor) ; col++) {
    fill_grilo_media_from_sparql (GRL_TRACKER_SOURCE (ms->source),
                                  ms->media, cursor, col);
  }

  ms->callback (ms->source, ms->media, ms->user_data, NULL);

 end_operation:
  if (cursor)
    g_object_unref (G_OBJECT (cursor));
}

/**/

gboolean
grl_tracker_source_may_resolve (GrlMetadataSource *source,
                                GrlMedia *media,
                                GrlKeyID key_id,
                                GList **missing_keys)
{
  return TRUE;
}

const GList *
grl_tracker_source_supported_keys (GrlMetadataSource *source)
{
  return
    grl_plugin_registry_get_metadata_keys (grl_plugin_registry_get_default ());
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
grl_tracker_source_query (GrlMediaSource *source,
                          GrlMediaSourceQuerySpec *qs)
{
  GError               *error = NULL;
  GrlTrackerSourcePriv *priv  = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  gchar                *constraint;
  gchar                *sparql_final;
  gchar                *sparql_select;
  struct OperationSpec *os;

  GRL_DEBUG ("%s: id=%u", __FUNCTION__, qs->query_id);

  if (!qs->query || qs->query[0] == '\0') {
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_QUERY_FAILED,
                                 "Empty query");
    goto send_error;
  }

  /* Check if it is a full sparql query */
  if (g_ascii_strncasecmp (qs->query, "select ", 7) != 0) {
    constraint = grl_tracker_source_get_device_constraint (priv);
    sparql_select = grl_tracker_source_get_select_string (source, qs->keys);
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
    grl_tracker_source_query (source, qs);
    return;
  }

  GRL_DEBUG ("\tselect : '%s'", qs->query);

  os = tracker_operation_initiate (source, priv, qs->query_id);
  os->keys         = qs->keys;
  os->skip         = qs->skip;
  os->count        = qs->count;
  os->callback     = qs->callback;
  os->user_data    = qs->user_data;

  tracker_sparql_connection_query_async (priv->tracker_connection,
                                         qs->query,
                                         os->cancel_op,
                                         (GAsyncReadyCallback) tracker_query_cb,
                                         os);

  return;

 send_error:
  qs->callback (qs->source, qs->query_id, NULL, 0, qs->user_data, error);
  g_error_free (error);
}

void
grl_tracker_source_metadata (GrlMediaSource *source,
                             GrlMediaSourceMetadataSpec *ms)
{
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  gchar                *sparql_select, *sparql_final;

  GRL_DEBUG ("%s: id=%i", __FUNCTION__, ms->metadata_id);

  if (grl_media_get_id (ms->media) == NULL) {
    ms->callback (ms->source, ms->media, ms->user_data, NULL);
    return;
  }

  sparql_select = grl_tracker_source_get_select_string (source, ms->keys);
  sparql_final = g_strdup_printf (TRACKER_METADATA_REQUEST, sparql_select,
                                  grl_media_get_id (ms->media));

  GRL_DEBUG ("\tselect: '%s'", sparql_final);

  tracker_sparql_connection_query_async (priv->tracker_connection,
                                         sparql_final,
                                         NULL,
                                         (GAsyncReadyCallback) tracker_metadata_cb,
                                         ms);

  if (sparql_select != NULL)
    g_free (sparql_select);
  if (sparql_final != NULL)
    g_free (sparql_final);
}

void
grl_tracker_source_search (GrlMediaSource *source, GrlMediaSourceSearchSpec *ss)
{
  GrlTrackerSourcePriv *priv  = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  gchar                *constraint;
  gchar                *sparql_select;
  gchar                *sparql_final;
  struct OperationSpec *os;

  GRL_DEBUG ("%s: id=%u", __FUNCTION__, ss->search_id);

  constraint = grl_tracker_source_get_device_constraint (priv);
  sparql_select = grl_tracker_source_get_select_string (source, ss->keys);
  if (!ss->text || ss->text[0] == '\0') {
    /* Search all */
    sparql_final = g_strdup_printf (TRACKER_SEARCH_ALL_REQUEST, sparql_select,
                                    constraint, ss->skip, ss->count);
  } else {
    sparql_final = g_strdup_printf (TRACKER_SEARCH_REQUEST, sparql_select,
                                    ss->text, constraint, ss->skip, ss->count);
  }

  GRL_DEBUG ("\tselect: '%s'", sparql_final);

  os = tracker_operation_initiate (source, priv, ss->search_id);
  os->keys         = ss->keys;
  os->skip         = ss->skip;
  os->count        = ss->count;
  os->callback     = ss->callback;
  os->user_data    = ss->user_data;

  tracker_sparql_connection_query_async (priv->tracker_connection,
                                         sparql_final,
                                         os->cancel_op,
                                         (GAsyncReadyCallback) tracker_query_cb,
                                         os);

  g_free (constraint);
  g_free (sparql_select);
  g_free (sparql_final);
}

static void
grl_tracker_source_browse_category (GrlMediaSource *source,
                                    GrlMediaSourceBrowseSpec *bs)
{
  GrlTrackerSourcePriv *priv  = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  gchar                *constraint;
  gchar                *sparql_select;
  gchar                *sparql_final;
  struct OperationSpec *os;
  GrlMedia             *media;
  const gchar          *category;

  GRL_DEBUG ("%s: id=%u", __FUNCTION__, bs->browse_id);

  if (bs->container == NULL ||
      !grl_data_has_key (GRL_DATA (bs->container),
                         grl_metadata_key_tracker_category)) {
    /* Hardcoded categories */
    media = grl_media_box_new ();
    grl_media_set_title (media, "Music");
    grl_data_set_string (GRL_DATA (media),
                         grl_metadata_key_tracker_category,
                         "nmm:MusicPiece");
    bs->callback (bs->source, bs->browse_id, media, 2, bs->user_data, NULL);

    media = grl_media_box_new ();
    grl_media_set_title (media, "Photo");
    grl_data_set_string (GRL_DATA (media),
                         grl_metadata_key_tracker_category,
                         "nmm:Photo");
    bs->callback (bs->source, bs->browse_id, media, 1, bs->user_data, NULL);

    media = grl_media_box_new ();
    grl_media_set_title (media, "Video");
    grl_data_set_string (GRL_DATA (media),
                         grl_metadata_key_tracker_category,
                         "nmm:Video");
    bs->callback (bs->source, bs->browse_id, media, 0, bs->user_data, NULL);
    return;
  }

  category = grl_data_get_string (GRL_DATA (bs->container),
                                  grl_metadata_key_tracker_category);

  constraint = grl_tracker_source_get_device_constraint (priv);
  sparql_select = grl_tracker_source_get_select_string (bs->source, bs->keys);
  sparql_final = g_strdup_printf (TRACKER_BROWSE_CATEGORY_REQUEST,
                                  sparql_select,
                                  category,
                                  constraint,
                                  bs->skip, bs->count);

  GRL_DEBUG ("\tselect: '%s'", sparql_final);

  os = tracker_operation_initiate (source, priv, bs->browse_id);
  os->keys         = bs->keys;
  os->skip         = bs->skip;
  os->count        = bs->count;
  os->callback     = bs->callback;
  os->user_data    = bs->user_data;

  tracker_sparql_connection_query_async (priv->tracker_connection,
                                         sparql_final,
                                         os->cancel_op,
                                         (GAsyncReadyCallback) tracker_query_cb,
                                         os);

  g_free (constraint);
  g_free (sparql_select);
  g_free (sparql_final);
}

static void
grl_tracker_source_browse_filesystem (GrlMediaSource *source,
                                      GrlMediaSourceBrowseSpec *bs)
{
  GrlTrackerSourcePriv *priv  = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  gchar                *constraint;
  gchar                *sparql_select;
  gchar                *sparql_final;
  struct OperationSpec *os;

  GRL_DEBUG ("%s: id=%u", __FUNCTION__, bs->browse_id);

  sparql_select = grl_tracker_source_get_select_string (bs->source, bs->keys);
  constraint = grl_tracker_source_get_device_constraint (priv);

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

  GRL_DEBUG ("\tselect: '%s'", sparql_final);

  os = tracker_operation_initiate (source, priv, bs->browse_id);
  os->keys         = bs->keys;
  os->skip         = bs->skip;
  os->count        = bs->count;
  os->callback     = bs->callback;
  os->user_data    = bs->user_data;

  tracker_sparql_connection_query_async (priv->tracker_connection,
                                         sparql_final,
                                         os->cancel_op,
                                         (GAsyncReadyCallback) tracker_query_cb,
                                         os);

  g_free (constraint);
  g_free (sparql_select);
  g_free (sparql_final);
}

void
grl_tracker_source_browse (GrlMediaSource *source,
                           GrlMediaSourceBrowseSpec *bs)
{
  if (grl_tracker_browse_filesystem)
    grl_tracker_source_browse_filesystem (source, bs);
  else
    grl_tracker_source_browse_category (source, bs);
}

void
grl_tracker_source_cancel (GrlMediaSource *source, guint operation_id)
{
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  struct OperationSpec *os;

  GRL_DEBUG ("%s: id=%u", __FUNCTION__, operation_id);

  os = g_hash_table_lookup (priv->operations, GSIZE_TO_POINTER (operation_id));

  if (os != NULL)
    g_cancellable_cancel (os->cancel_op);
}

gboolean
grl_tracker_source_change_start (GrlMediaSource *source, GError **error)
{
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (source);

  priv->notify_changes = TRUE;

  return TRUE;
}

gboolean
grl_tracker_source_change_stop (GrlMediaSource *source, GError **error)
{
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (source);

  priv->notify_changes = FALSE;

  return TRUE;
}

void
grl_tracker_init_requests (void)
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


  GRL_LOG_DOMAIN_INIT (tracker_request_log_domain, "tracker-request");
}
