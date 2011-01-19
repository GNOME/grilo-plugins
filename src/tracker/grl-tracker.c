/*
 * Copyright (C) 2011 Igalia S.L.
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
#include <tracker-sparql.h>

#include "grl-tracker.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT tracker_log_domain
GRL_LOG_DOMAIN_STATIC(tracker_log_domain);

/* ------- Definitions ------- */

#define MEDIA_TYPE "grilo-media-type"

#define RDF_TYPE_ALBUM  "nmm#MusicAlbum"
#define RDF_TYPE_ARTIST "nmm#Artist"
#define RDF_TYPE_AUDIO  "nmm#MusicPiece"
#define RDF_TYPE_IMAGE  "nmm#Photo"
#define RDF_TYPE_VIDEO  "nmm#Video"
#define RDF_TYPE_BOX    "grilo#Box"

/* ---- Plugin information --- */

#define PLUGIN_ID   TRACKER_PLUGIN_ID

#define SOURCE_ID   "grl-tracker"
#define SOURCE_NAME "Tracker"
#define SOURCE_DESC "A plugin for searching multimedia content using Tracker"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

enum {
  METADATA,
  BROWSE,
  QUERY,
  SEARCH
};

/* --- Other --- */

#define TRACKER_METADATA_REQUEST "                                  \
  SELECT %s                                                         \
  WHERE {                                                           \
    ?urn nie:isStoredAs <%s>                                        \
  }"

typedef struct {
  GrlKeyID     grl_key;
  const gchar *sparql_key_name;
  const gchar *sparql_key_attr;
  const gchar *sparql_key_flavor;
} tracker_grl_sparql_t;

struct OperationSpec {
  GrlMediaSource         *source;
  GrlTrackerSourcePriv   *priv;
  guint                   operation_id;
  const GList            *keys;
  guint                   skip;
  guint                   count;
  guint                   current;
  GrlMediaSourceResultCb  callback;
  gpointer                user_data;
  TrackerSparqlCursor    *cursor;
};

enum {
  PROP_0,
  PROP_TRACKER_CONNECTION,
};

struct _GrlTrackerSourcePriv {
  TrackerSparqlConnection *tracker_connection;
};

#define GRL_TRACKER_SOURCE_GET_PRIVATE(object)		\
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                \
                               GRL_TRACKER_SOURCE_TYPE,	\
                               GrlTrackerSourcePriv))

static GrlTrackerSource *grl_tracker_source_new (TrackerSparqlConnection *connection);

static void grl_tracker_source_set_property (GObject      *object,
                                             guint         propid,
                                             const GValue *value,
                                             GParamSpec   *pspec);

static void grl_tracker_source_finalize (GObject *object);

gboolean grl_tracker_plugin_init (GrlPluginRegistry *registry,
                                  const GrlPluginInfo *plugin,
                                  GList *configs);

static GrlSupportedOps grl_tracker_source_supported_operations (GrlMetadataSource *metadata_source);

static const GList *grl_tracker_source_supported_keys (GrlMetadataSource *source);

static void grl_tracker_source_query (GrlMediaSource *source,
                                      GrlMediaSourceQuerySpec *qs);

static void grl_tracker_source_metadata (GrlMediaSource *source,
                                         GrlMediaSourceMetadataSpec *ms);

static void setup_key_mappings (void);

/* ===================== Globals  ================= */

static GHashTable *grl_to_sparql_mapping = NULL;
static GHashTable *sparql_to_grl_mapping = NULL;

/* =================== Tracker Plugin  =============== */

static void
tracker_get_connection_cb (GObject             *object,
                           GAsyncResult        *res,
                           const GrlPluginInfo *plugin)
{
  TrackerSparqlConnection *connection;
  GrlTrackerSource *source;

  connection = tracker_sparql_connection_get_finish (res, NULL);

  if (connection != NULL) {
    source = grl_tracker_source_new (connection);
    grl_plugin_registry_register_source (grl_plugin_registry_get_default (),
                                         plugin,
                                         GRL_MEDIA_PLUGIN (source),
                                         NULL);
    g_object_unref (G_OBJECT (connection));
  }
}

gboolean
grl_tracker_plugin_init (GrlPluginRegistry *registry,
                         const GrlPluginInfo *plugin,
                         GList *configs)
{
  GRL_LOG_DOMAIN_INIT (tracker_log_domain, "tracker");

  GRL_DEBUG ("%s", __FUNCTION__);

  tracker_sparql_connection_get_async (NULL,
                                       (GAsyncReadyCallback) tracker_get_connection_cb,
                                       (gpointer) plugin);
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_tracker_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== Tracker GObject ================ */

static GrlTrackerSource *
grl_tracker_source_new (TrackerSparqlConnection *connection)
{
  GRL_DEBUG ("%s", __FUNCTION__);

  return g_object_new (GRL_TRACKER_SOURCE_TYPE,
                       "source-id", SOURCE_ID,
                       "source-name", SOURCE_NAME,
                       "source-desc", SOURCE_DESC,
                       "tracker-connection", connection,
                       NULL);
}

G_DEFINE_TYPE (GrlTrackerSource, grl_tracker_source, GRL_TYPE_MEDIA_SOURCE);

static void
grl_tracker_source_class_init (GrlTrackerSourceClass * klass)
{
  GrlMediaSourceClass    *source_class   = GRL_MEDIA_SOURCE_CLASS (klass);
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);
  GObjectClass           *g_class        = G_OBJECT_CLASS (klass);

  source_class->query = grl_tracker_source_query;
  source_class->metadata = grl_tracker_source_metadata;

  metadata_class->supported_keys = grl_tracker_source_supported_keys;
  metadata_class->supported_operations = grl_tracker_source_supported_operations;

  g_class->finalize = grl_tracker_source_finalize;
  g_class->set_property = grl_tracker_source_set_property;

  g_object_class_install_property (g_class,
                                   PROP_TRACKER_CONNECTION,
                                   g_param_spec_object ("tracker-connection",
                                                        "tracker-connection",
                                                        "A Tracker connection",
                                                        TRACKER_SPARQL_TYPE_CONNECTION,
                                                        G_PARAM_WRITABLE
                                                        | G_PARAM_CONSTRUCT_ONLY
                                                        | G_PARAM_STATIC_NAME));

  g_type_class_add_private (klass, sizeof (GrlTrackerSourcePriv));

  setup_key_mappings ();

}

static void
grl_tracker_source_init (GrlTrackerSource *source)
{
  source->priv = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
}

static void
grl_tracker_source_finalize (GObject *object)
{
  GrlTrackerSource *self;

  self = GRL_TRACKER_SOURCE (object);
  if (self->priv->tracker_connection)
    g_object_unref (self->priv->tracker_connection);

  G_OBJECT_CLASS (grl_tracker_source_parent_class)->finalize (object);
}

static void
grl_tracker_source_set_property (GObject      *object,
                                 guint         propid,
                                 const GValue *value,
                                 GParamSpec   *pspec)

{
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (object);

  switch (propid) {
  case PROP_TRACKER_CONNECTION:
    g_object_unref (G_OBJECT (priv->tracker_connection));
    priv->tracker_connection = g_object_ref (g_value_get_object (value));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, propid, pspec);
  }
}

/* ======================= Utilities ==================== */

static gchar *
build_flavored_key (gchar *key, const gchar *flavor)
{
  gint i = 0;

  while (key[i] != '\0') {
    if (!g_ascii_isalnum (key[i])) {
      key[i] = '_';
    }
    i++;
  }

  return g_strdup_printf ("%s_%s", key, flavor);
}

static void
insert_key_mapping (GrlKeyID     grl_key,
                    const gchar *sparql_key_attr,
                    const gchar *sparql_key_flavor)
{
  tracker_grl_sparql_t *assoc = g_slice_new0 (tracker_grl_sparql_t);
  GList *assoc_list = g_hash_table_lookup (grl_to_sparql_mapping, grl_key);
  gchar *canon_name = g_strdup (g_param_spec_get_name (grl_key));

  assoc->grl_key           = grl_key;
  assoc->sparql_key_name   = build_flavored_key (canon_name, sparql_key_flavor);
  assoc->sparql_key_attr   = sparql_key_attr;
  assoc->sparql_key_flavor = sparql_key_flavor;

  assoc_list = g_list_append (assoc_list, assoc);

  g_hash_table_insert (grl_to_sparql_mapping, grl_key, assoc_list);
  g_hash_table_insert (sparql_to_grl_mapping,
                       (gpointer) assoc->sparql_key_name,
                       assoc);
  g_hash_table_insert (sparql_to_grl_mapping,
                       (gpointer) g_param_spec_get_name (G_PARAM_SPEC (grl_key)),
                       assoc);

  g_free (canon_name);
}

static void
setup_key_mappings (void)
{
  grl_to_sparql_mapping = g_hash_table_new (g_direct_hash, g_direct_equal);
  sparql_to_grl_mapping = g_hash_table_new (g_str_hash, g_str_equal);

  insert_key_mapping (GRL_METADATA_KEY_ALBUM,
                      "nmm:albumTitle(nmm:musicAlbum(?urn))",
                      "audio");

  insert_key_mapping (GRL_METADATA_KEY_ARTIST,
                      "nmm:artistName(nmm:performer(?urn))",
                      "audio");

  insert_key_mapping (GRL_METADATA_KEY_AUTHOR,
                      "nmm:artistName(nmm:performer(?urn))",
                      "audio");

  insert_key_mapping (GRL_METADATA_KEY_BITRATE,
                      "nfo:averageBitrate(?urn)",
                      "audio");

  insert_key_mapping (GRL_METADATA_KEY_CHILDCOUNT,
                      "nfo:entryCounter(?urn)",
                      "directory");

  insert_key_mapping (GRL_METADATA_KEY_DATE,
                      "nfo:fileLastModified(?urn)",
                      "file");

  insert_key_mapping (GRL_METADATA_KEY_DURATION,
                      "nfo:duration(?urn)",
                      "audio");

  insert_key_mapping (GRL_METADATA_KEY_FRAMERATE,
                      "nfo:frameRate(?urn)",
                      "video");

  insert_key_mapping (GRL_METADATA_KEY_HEIGHT,
                      "nfo:height(?urn)",
                      "video");

  insert_key_mapping (GRL_METADATA_KEY_ID,
                      "nie:isStoredAs(?urn)",
                      "file");

  insert_key_mapping (GRL_METADATA_KEY_LAST_PLAYED,
                      "nfo:fileLastAccessed(?urn)",
                      "file");

  insert_key_mapping (GRL_METADATA_KEY_MIME,
                      "nie:mimeType(?urn)",
                      "file");

  insert_key_mapping (GRL_METADATA_KEY_SITE,
                      "nie:url(?urn)",
                      "file");

  insert_key_mapping (GRL_METADATA_KEY_TITLE,
                      "nie:title(?urn)",
                      "audio");

  insert_key_mapping (GRL_METADATA_KEY_TITLE,
                      "nfo:fileName(?urn)",
                      "file");

  insert_key_mapping (GRL_METADATA_KEY_URL,
                      "nie:url(?urn)",
                      "file");

  insert_key_mapping (GRL_METADATA_KEY_WIDTH,
                      "nfo:width(?urn)",
                      "video");
}

static tracker_grl_sparql_t *
get_mapping_from_sparql (const gchar *key)
{
  return (tracker_grl_sparql_t *) g_hash_table_lookup (sparql_to_grl_mapping,
                                                       key);
}

static GList *
get_mapping_from_grl (const GrlKeyID key)
{
  return (GList *) g_hash_table_lookup (grl_to_sparql_mapping, key);
}

static void
tracker_operation_terminate (struct OperationSpec *operation)
{
  if (operation == NULL)
    return;

  g_object_unref (G_OBJECT (operation->cursor));
  g_slice_free (struct OperationSpec, operation);
}

static gchar *
get_select_string (GrlMediaSource *source, const GList *keys)
{
  const GList *key = keys;
  GString *gstr = g_string_new ("");
  GList *assoc_list;
  tracker_grl_sparql_t *assoc;

  while (key != NULL) {
    assoc_list = get_mapping_from_grl ((GrlKeyID) key->data);
    while (assoc_list != NULL) {
      assoc = (tracker_grl_sparql_t *) assoc_list->data;
      if (assoc != NULL) {
        g_string_append_printf (gstr, "%s AS %s",
                                assoc->sparql_key_attr,
                                assoc->sparql_key_name);
        g_string_append (gstr, " ");
      }
      assoc_list = assoc_list->next;
    }
    key = key->next;
  }

  return g_string_free (gstr, FALSE);
}

/* Builds an appropriate GrlMedia based on ontology type returned by tracker, or
   NULL if unknown */
static GrlMedia *
build_grilo_media (const gchar *rdf_type)
{
  GrlMedia *media = NULL;
  gchar **rdf_single_type;
  int i;

  if (!rdf_type) {
    return NULL;
  }

  /* As rdf_type can be formed by several types, split them */
  rdf_single_type = g_strsplit (rdf_type, ",", -1);
  i = g_strv_length (rdf_single_type) - 1;

  while (!media && i >= 0) {
    if (g_str_has_suffix (rdf_single_type[i], RDF_TYPE_AUDIO)) {
      media = grl_media_audio_new ();
    } else if (g_str_has_suffix (rdf_single_type[i], RDF_TYPE_VIDEO)) {
      media = grl_media_video_new ();
    } else if (g_str_has_suffix (rdf_single_type[i], RDF_TYPE_IMAGE)) {
      media = grl_media_image_new ();
    } else if (g_str_has_suffix (rdf_single_type[i], RDF_TYPE_ARTIST)) {
      media = grl_media_box_new ();
    } else if (g_str_has_suffix (rdf_single_type[i], RDF_TYPE_ALBUM)) {
      media = grl_media_box_new ();
    } else if (g_str_has_suffix (rdf_single_type[i], RDF_TYPE_BOX)) {
      media = grl_media_box_new ();
    }
    i--;
  }

  g_strfreev (rdf_single_type);

  return media;
}

static void
fill_grilo_media_from_sparql (GrlMedia            *media,
                              TrackerSparqlCursor *cursor,
                              gint                 column)
{
  const gchar *sparql_key = tracker_sparql_cursor_get_variable_name (cursor, column);
  tracker_grl_sparql_t *assoc = get_mapping_from_sparql (sparql_key);;
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

  GRL_DEBUG ("Parsing line %i of type %s", operation->current, sparql_type);

  media = build_grilo_media (sparql_type);

  if (media != NULL) {
    for (col = 1 ;
         col < tracker_sparql_cursor_get_n_columns (operation->cursor) ;
         col++) {
      fill_grilo_media_from_sparql (media, operation->cursor, col);
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
    tracker_sparql_cursor_next_async (operation->cursor, NULL,
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
    fill_grilo_media_from_sparql (ms->media, cursor, col);
  }

  ms->callback (ms->source, ms->media, ms->user_data, NULL);

 end_operation:
  if (cursor)
    g_object_unref (G_OBJECT (cursor));
}

/* ================== API Implementation ================ */

static GrlSupportedOps
grl_tracker_source_supported_operations (GrlMetadataSource *metadata_source)
{
  GrlSupportedOps   caps;
  GrlTrackerSource *source;

  source = GRL_TRACKER_SOURCE (metadata_source);
  caps = GRL_OP_METADATA | GRL_OP_QUERY;

  return caps;
}

static const GList *
grl_tracker_source_supported_keys (GrlMetadataSource *source)
{
  return grl_plugin_registry_get_metadata_keys (grl_plugin_registry_get_default ());
}

/**
 * Query is a SPARQL query.
 *
 * Columns must be named with the Grilo key name that the column
 * represent. Unnamed or unknown columns will be ignored.
 *
 * For the case of media type, name to be used is "grilo-media-type". This is a
 * mandatory column. It must match with rdf:type() property. Types understood
 * are:
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
 *     SELECT rdf:type(?song)  AS grilo-media-type
 *            ?song            AS id
 *            nie:title(?song) AS title
 *            nie:url(?song)   AS url
 *     WHERE { ?song a nmm:MusicPiece }
 *   </programlisting>
 * </informalexample>
 */
static void
grl_tracker_source_query (GrlMediaSource *source,
                          GrlMediaSourceQuerySpec *qs)
{
  GrlTrackerSourcePriv *priv  = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  GError               *error = NULL;
  struct OperationSpec *os;

  GRL_DEBUG ("%s", __FUNCTION__);

  if (!qs->query || qs->query[0] == '\0') {
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_QUERY_FAILED,
                                 "Empty query");
    goto send_error;
  }

  GRL_DEBUG ("select : %s", qs->query);

  os = g_slice_new0 (struct OperationSpec);
  os->source       = qs->source;
  os->priv         = priv;
  os->operation_id = qs->query_id;
  os->keys         = qs->keys;
  os->skip         = qs->skip;
  os->count        = qs->count;
  os->callback     = qs->callback;
  os->user_data    = qs->user_data;

  tracker_sparql_connection_query_async (priv->tracker_connection,
                                         qs->query,
                                         NULL,
                                         (GAsyncReadyCallback) tracker_query_cb,
                                         os);

  return;

 send_error:
  qs->callback (qs->source, qs->query_id, NULL, 0, qs->user_data, error);
  g_error_free (error);
}

static void
grl_tracker_source_metadata (GrlMediaSource *source,
                             GrlMediaSourceMetadataSpec *ms)
{
  GrlTrackerSourcePriv *priv = GRL_TRACKER_SOURCE_GET_PRIVATE (source);
  gchar                *sparql_select, *sparql_final;

  GRL_DEBUG ("%s", __FUNCTION__);

  sparql_select = get_select_string (source, ms->keys);
  sparql_final = g_strdup_printf (TRACKER_METADATA_REQUEST, sparql_select,
                                  grl_media_get_id (ms->media));

  GRL_DEBUG ("select: '%s'", sparql_final);

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
