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

#include "grl-tracker-utils.h"
#include "grl-tracker-priv.h"

/**/

static GHashTable *grl_to_sparql_mapping = NULL;
static GHashTable *sparql_to_grl_mapping = NULL;

/**/

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

void
grl_tracker_setup_key_mappings (void)
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
                      "tracker:id(?urn)",
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

tracker_grl_sparql_t *
grl_tracker_get_mapping_from_sparql (const gchar *key)
{
  return (tracker_grl_sparql_t *) g_hash_table_lookup (sparql_to_grl_mapping,
                                                       key);
}

static GList *
get_mapping_from_grl (const GrlKeyID key)
{
  return (GList *) g_hash_table_lookup (grl_to_sparql_mapping, key);
}

/**/

gchar *
grl_tracker_source_get_device_constraint (GrlTrackerSourcePriv *priv)
{
  if (priv->tracker_datasource == NULL)
    return g_strdup ("");

  return g_strdup_printf ("?urn nie:dataSource <%s> .",
                          priv->tracker_datasource);
}

gchar *
grl_tracker_source_get_select_string (GrlMediaSource *source,
                                      const GList *keys)
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

/**/

/* Builds an appropriate GrlMedia based on ontology type returned by
   tracker, or NULL if unknown */
GrlMedia *
grl_tracker_build_grilo_media (const gchar *rdf_type)
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
    if (g_str_has_suffix (rdf_single_type[i], RDF_TYPE_MUSIC)) {
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

/**/

static gchar *
get_tracker_volume_name (const gchar *uri,
			 const gchar *datasource)
{
  gchar *source_name = NULL;
  GVolumeMonitor *volume_monitor;
  GList *mounts, *mount;
  GFile *file;

  if (uri != NULL) {
    volume_monitor = g_volume_monitor_get ();
    mounts = g_volume_monitor_get_mounts (volume_monitor);
    file = g_file_new_for_uri (uri);

    mount = mounts;
    while (mount != NULL) {
      GFile *m_file = g_mount_get_root (G_MOUNT (mount->data));

      if (g_file_equal (m_file, file)) {
        gchar *m_name = g_mount_get_name (G_MOUNT (mount->data));
        g_object_unref (G_OBJECT (m_file));
        source_name = g_strdup_printf ("Removable - %s", m_name);
        g_free (m_name);
        break;
      }
      g_object_unref (G_OBJECT (m_file));

      mount = mount->next;
    }
    g_list_foreach (mounts, (GFunc) g_object_unref, NULL);
    g_list_free (mounts);
    g_object_unref (G_OBJECT (file));
    g_object_unref (G_OBJECT (volume_monitor));
  } else {
    source_name = g_strdup ("Local");
  }

  return source_name;
}

static gchar *
get_tracker_upnp_name (const gchar *datasource_name)
{
  return g_strdup_printf ("UPnP - %s", datasource_name);
}

gchar *
grl_tracker_get_source_name (const gchar *rdf_type,
                             const gchar *uri,
                             const gchar *datasource,
                             const gchar *datasource_name)
{
  gchar *source_name = NULL;
  gchar **rdf_single_type;
  gint i;

  /* As rdf_type can be formed by several types, split them */
  rdf_single_type = g_strsplit (rdf_type, ",", -1);
  i = g_strv_length (rdf_single_type) - 1;

  while (i >= 0) {
    if (g_str_has_suffix (rdf_single_type[i], RDF_TYPE_VOLUME)) {
      source_name = get_tracker_volume_name (uri, datasource);
      break;
    } else if (g_str_has_suffix (rdf_single_type[i], RDF_TYPE_UPNP)) {
      source_name = get_tracker_upnp_name (datasource_name);
      break;
    }
    i--;
  }

  g_strfreev (rdf_single_type);

  if (!source_name)
    source_name = g_strdup_printf  ("%s %s",
                                    GRL_TRACKER_SOURCE_NAME,
                                    datasource);

  return source_name;
}
