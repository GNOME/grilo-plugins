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

/* Execute a sparql query an sends the elements as Grilo medias.
   Returns FALSE if the query failed.
*/
static gboolean
do_tracker_query (GrlMediaSource *source,
                  const gchar *sparql_query,
                  GrlMediaSourceResultCb callback,
                  guint query_id,
                  gpointer user_data,
                  GError **error)
{
  GError *tracker_error = NULL;
  GrlKeyID key;
  GrlMedia *media;
  TrackerSparqlCursor *cursor;
  const gchar *key_name = NULL;
  int i;
  int n_columns;
  int type_column;

  cursor =
    tracker_sparql_connection_query (GRL_TRACKER_SOURCE (source)->priv->tracker_connection,
                                     sparql_query,
                                     NULL,
                                     &tracker_error);
  if (!cursor) {
    if (error) {
      *error = g_error_new (GRL_CORE_ERROR,
                            GRL_CORE_ERROR_QUERY_FAILED,
                            "Query failed: %s",
                            tracker_error->message);
    }
    return FALSE;
  }

  while (tracker_sparql_cursor_next (cursor, NULL, NULL)) {
      n_columns = tracker_sparql_cursor_get_n_columns (cursor);
      /* Search column with type */
      for (type_column = 0; type_column < n_columns; type_column++) {
        if (strcmp (tracker_sparql_cursor_get_variable_name (cursor,
                                                             type_column),
                    MEDIA_TYPE) == 0) {
          break;
        }
      }

      /* type not found */
      if (type_column >= n_columns) {
        continue;
      }

      media = build_grilo_media (tracker_sparql_cursor_get_string (cursor,
                                                                   type_column,
                                                                   NULL));
      /* Unknown media */
      if (!media) {
        continue;
      }

      /* Fill data */
      for (i = 0; i < n_columns; i++) {
        /* Skip type */
        if (i == type_column) {
          continue;
        }

        /* Column has no value */
        if (!tracker_sparql_cursor_is_bound (cursor, i)) {
          continue;
        }

        key_name = tracker_sparql_cursor_get_variable_name (cursor, i);

        /* Unnamed column */
        if (!key_name) {
          continue;
        }

        key = grl_plugin_registry_lookup_metadata_key (grl_plugin_registry_get_default (),
                                                       key_name);
        /* Unknown key */
        if (!key) {
          continue;
        }

        grl_data_set_string (GRL_DATA (media),
                             key,
                             tracker_sparql_cursor_get_string (cursor,
                                                               i,
                                                               NULL));
      }

      /* Send data */
      callback (source, query_id, media, -1, user_data, NULL);
  }

  /* Notify there is no more elements */
  callback (source, query_id, NULL, 0, user_data, NULL);

  g_object_unref (cursor);

  return TRUE;
}

/* ================== API Implementation ================ */

static GrlSupportedOps
grl_tracker_source_supported_operations (GrlMetadataSource *metadata_source)
{
  GrlSupportedOps   caps;
  GrlTrackerSource *source;

  source = GRL_TRACKER_SOURCE (metadata_source);
  caps = GRL_OP_QUERY;

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
  GError *error = NULL;

  GRL_DEBUG ("%s", __FUNCTION__);

  if (!qs->query || qs->query[0] == '\0') {
    error = g_error_new_literal (GRL_CORE_ERROR,
                                 GRL_CORE_ERROR_QUERY_FAILED,
                                 "Empty query");
    goto send_error;
  }

  GRL_DEBUG ("select : %s", qs->query);

  do_tracker_query (source,
                    qs->query,
                    qs->callback,
                    qs->query_id,
                    qs->user_data,
                    &error);

  if (error) {
    goto send_error;
  }

  return;

 send_error:
  qs->callback (qs->source, qs->query_id, NULL, 0, qs->user_data, error);
  g_error_free (error);
}
