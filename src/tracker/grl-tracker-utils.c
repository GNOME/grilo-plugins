/*
 * Copyright (C) 2011 Igalia S.L.
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

#include "grl-tracker-utils.h"
#include <glib/gi18n-lib.h>

/**/

static GHashTable *grl_to_sparql_mapping = NULL;
static GHashTable *sparql_to_grl_mapping = NULL;

GrlKeyID grl_metadata_key_tracker_urn;
GrlKeyID grl_metadata_key_gibest_hash;


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
set_orientation (TrackerSparqlCursor *cursor,
                 gint                 column,
                 GrlMedia            *media,
                 GrlKeyID             key)
{
  const gchar *str = tracker_sparql_cursor_get_string (cursor, column, NULL);

  if (g_str_has_suffix (str, "nfo#orientation-top"))
    grl_data_set_int (GRL_DATA (media), key, 0);
  else if (g_str_has_suffix (str, "nfo#orientation-right"))
    grl_data_set_int (GRL_DATA (media), key, 90);
  else if (g_str_has_suffix (str, "nfo#orientation-bottom"))
    grl_data_set_int (GRL_DATA (media), key, 180);
  else if (g_str_has_suffix (str, "nfo#orientation-left"))
    grl_data_set_int (GRL_DATA (media), key, 270);
}

static void
set_date (TrackerSparqlCursor *cursor,
          gint                 column,
          GrlMedia            *media,
          GrlKeyID             key)
{
  const gchar *str = tracker_sparql_cursor_get_string (cursor, column, NULL);
  if (key == GRL_METADATA_KEY_CREATION_DATE
      || key == GRL_METADATA_KEY_MODIFICATION_DATE) {
    GDateTime *date = grl_date_time_from_iso8601 (str);
    if (date) {
      grl_data_set_boxed (GRL_DATA (media), key, date);
      g_date_time_unref (date);
    }
  }
}

static void
set_title_from_filename (TrackerSparqlCursor *cursor,
                         gint                 column,
                         GrlMedia            *media,
                         GrlKeyID             key)
{
  const gchar *str = tracker_sparql_cursor_get_string (cursor, column, NULL);
  if (key == GRL_METADATA_KEY_TITLE) {
    grl_data_set_boolean (GRL_DATA (media), GRL_METADATA_KEY_TITLE_FROM_FILENAME, TRUE);
    grl_media_set_title (media, str);
  }
}

static void
set_title (TrackerSparqlCursor *cursor,
           gint                 column,
           GrlMedia            *media,
           GrlKeyID             key)
{
  const gchar *str = tracker_sparql_cursor_get_string (cursor, column, NULL);
  grl_data_set_boolean (GRL_DATA (media), GRL_METADATA_KEY_TITLE_FROM_FILENAME, FALSE);
  grl_media_set_title (media, str);
}

static tracker_grl_sparql_t *
insert_key_mapping (GrlKeyID     grl_key,
                    const gchar *sparql_key_attr,
                    const gchar *sparql_key_attr_call,
                    const gchar *sparql_key_flavor)
{
  tracker_grl_sparql_t *assoc = g_new0 (tracker_grl_sparql_t, 1);
  GList *assoc_list = g_hash_table_lookup (grl_to_sparql_mapping,
                                           GRLKEYID_TO_POINTER (grl_key));
  gchar *canon_name = g_strdup (GRL_METADATA_KEY_GET_NAME (grl_key));

  assoc->grl_key              = grl_key;
  assoc->sparql_key_name      = build_flavored_key (canon_name,
                                                    sparql_key_flavor);
  assoc->sparql_key_attr      = sparql_key_attr;
  assoc->sparql_key_attr_call = sparql_key_attr_call;
  assoc->sparql_key_flavor    = sparql_key_flavor;

  assoc_list = g_list_append (assoc_list, assoc);

  g_hash_table_insert (grl_to_sparql_mapping,
                       GRLKEYID_TO_POINTER (grl_key),
                       assoc_list);
  g_hash_table_insert (sparql_to_grl_mapping,
                       (gpointer) assoc->sparql_key_name,
                       assoc);
  g_hash_table_insert (sparql_to_grl_mapping,
                       (gpointer) GRL_METADATA_KEY_GET_NAME (grl_key),
                       assoc);

  g_free (canon_name);

  return assoc;
}

static tracker_grl_sparql_t *
insert_key_mapping_with_setter (GrlKeyID                       grl_key,
                                const gchar                   *sparql_key_attr,
                                const gchar                   *sparql_key_attr_call,
                                const gchar                   *sparql_key_flavor,
                                tracker_grl_sparql_setter_cb_t setter)
{
  tracker_grl_sparql_t *assoc;

  assoc = insert_key_mapping (grl_key,
                              sparql_key_attr,
                              sparql_key_attr_call,
                              sparql_key_flavor);

  assoc->set_value = setter;

  return assoc;
}

void
grl_tracker_setup_key_mappings (void)
{
  GrlRegistry *registry = grl_registry_get_default ();

  /* Check if "tracker-urn" is registered; if not, then register it */
  grl_metadata_key_tracker_urn =
    grl_registry_lookup_metadata_key (registry, "tracker-urn");

  if (grl_metadata_key_tracker_urn == GRL_METADATA_KEY_INVALID) {
    grl_metadata_key_tracker_urn =
      grl_registry_register_metadata_key (grl_registry_get_default (),
                                          g_param_spec_string ("tracker-urn",
                                                               "Tracker URN",
                                                               "Universal resource number in Tracker's store",
                                                               NULL,
                                                               G_PARAM_STATIC_STRINGS |
                                                               G_PARAM_READWRITE),
                                          NULL);
  }

  /* Check if "gibest-hash" is registered; if not, then register it */
  grl_metadata_key_gibest_hash =
    grl_registry_lookup_metadata_key (registry, "gibest-hash");

  if (grl_metadata_key_gibest_hash == GRL_METADATA_KEY_INVALID) {
    grl_metadata_key_gibest_hash =
      grl_registry_register_metadata_key (grl_registry_get_default (),
                                          g_param_spec_string ("gibest-hash",
                                                               "Gibest hash",
                                                               "Gibest hash of the video file",
                                                               NULL,
                                                               G_PARAM_STATIC_STRINGS |
                                                               G_PARAM_READWRITE),
                                          NULL);
  }

  grl_to_sparql_mapping = g_hash_table_new (g_direct_hash, g_direct_equal);
  sparql_to_grl_mapping = g_hash_table_new (g_str_hash, g_str_equal);

  insert_key_mapping (grl_metadata_key_tracker_urn,
                      NULL,
                      "?urn",
                      "file");

  insert_key_mapping (GRL_METADATA_KEY_ALBUM,
                      NULL,
                      "nmm:albumTitle(nmm:musicAlbum(?urn))",
                      "audio");

  insert_key_mapping (GRL_METADATA_KEY_ARTIST,
                      NULL,
                      "nmm:artistName(nmm:performer(?urn))",
                      "audio");

  insert_key_mapping (GRL_METADATA_KEY_AUTHOR,
                      NULL,
                      "nmm:artistName(nmm:performer(?urn))",
                      "audio");

  insert_key_mapping (GRL_METADATA_KEY_BITRATE,
                      "nfo:averageBitrate",
                      "nfo:averageBitrate(?urn)",
                      "audio");

  insert_key_mapping (GRL_METADATA_KEY_CHILDCOUNT,
                      "nfo:entryCounter",
                      "nfo:entryCounter(?urn)",
                      "directory");

  insert_key_mapping (GRL_METADATA_KEY_SIZE,
                      NULL,
                      "nfo:fileSize(?urn)",
                      "file");

  insert_key_mapping (grl_metadata_key_gibest_hash,
                      NULL,
                      "(select nfo:hashValue(?h) { ?urn nfo:hasHash ?h . ?h nfo:hashAlgorithm \"gibest\" })",
                      "video");

  insert_key_mapping_with_setter (GRL_METADATA_KEY_MODIFICATION_DATE,
                                  "nfo:fileLastModified",
                                  "nfo:fileLastModified(?urn)",
                                  "file",
                                  set_date);

  insert_key_mapping (GRL_METADATA_KEY_DURATION,
                      "nfo:duration",
                      "nfo:duration(?urn)",
                      "audio");

  insert_key_mapping (GRL_METADATA_KEY_FRAMERATE,
                      "nfo:frameRate",
                      "nfo:frameRate(?urn)",
                      "video");

  insert_key_mapping (GRL_METADATA_KEY_HEIGHT,
                      "nfo:height",
                      "nfo:height(?urn)",
                      "video");

  insert_key_mapping (GRL_METADATA_KEY_ID,
                      "tracker:id",
                      "tracker:id(?urn)",
                      "file");

  /* insert_key_mapping (GRL_METADATA_KEY_LAST_PLAYED, */
  /*                     "nfo:fileLastAccessed(?urn)", */
  /*                     "file"); */

  insert_key_mapping (GRL_METADATA_KEY_MIME,
                      "nie:mimeType",
                      "nie:mimeType(?urn)",
                      "file");

  insert_key_mapping (GRL_METADATA_KEY_SITE,
                      "nie:url",
                      "nie:url(?urn)",
                      "file");

  insert_key_mapping_with_setter (GRL_METADATA_KEY_TITLE,
                                  "nie:title",
                                  "nie:title(?urn)",
                                  "audio",
                                  set_title);

  insert_key_mapping_with_setter (GRL_METADATA_KEY_TITLE,
                                  "nfo:fileName",
                                  "nfo:fileName(?urn)",
                                  "file",
                                  set_title_from_filename);

  insert_key_mapping (GRL_METADATA_KEY_URL,
                      "nie:url",
                      "nie:url(?urn)",
                      "file");

  insert_key_mapping (GRL_METADATA_KEY_WIDTH,
                      "nfo:width",
                      "nfo:width(?urn)",
                      "video");

  insert_key_mapping (GRL_METADATA_KEY_SEASON,
                      "nmm:season",
                      "nmm:season(?urn)",
                      "video");

  insert_key_mapping (GRL_METADATA_KEY_EPISODE,
                      "nmm:episodeNumber",
                      "nmm:episodeNumber(?urn)",
                      "video");

  insert_key_mapping_with_setter (GRL_METADATA_KEY_CREATION_DATE,
                                  "nie:contentCreated",
                                  "nie:contentCreated(?urn)",
                                  "image",
                                  set_date);

  insert_key_mapping (GRL_METADATA_KEY_CAMERA_MODEL,
                      NULL,
                      "nfo:model(nfo:equipment(?urn))",
                      "image");

  insert_key_mapping (GRL_METADATA_KEY_FLASH_USED,
                      "nmm:flash",
                      "nmm:flash(?urn)",
                      "image");

  insert_key_mapping (GRL_METADATA_KEY_EXPOSURE_TIME,
                      "nmm:exposureTime",
                      "nmm:exposureTime(?urn)",
                      "image");

  insert_key_mapping (GRL_METADATA_KEY_ISO_SPEED,
                      "nmm:isoSpeed",
                      "nmm:isoSpeed(?urn)",
                      "image");

  insert_key_mapping_with_setter (GRL_METADATA_KEY_ORIENTATION,
                                  "nfo:orientation",
                                  "nfo:orientation(?urn)",
                                  "image",
                                  set_orientation);

  insert_key_mapping (GRL_METADATA_KEY_PLAY_COUNT,
                      "nie:usageCounter",
                      "nie:usageCounter(?urn)",
                      "media");

  insert_key_mapping (GRL_METADATA_KEY_LAST_PLAYED,
                      "nie:contentAccessed",
                      "nie:contentAccessed(?urn)",
                      "media");

  insert_key_mapping (GRL_METADATA_KEY_LAST_POSITION,
                      "nfo:lastPlayedPosition",
                      "nfo:lastPlayedPosition(?urn)",
                      "media");

  insert_key_mapping (GRL_METADATA_KEY_START_TIME,
                      "nfo:audioOffset",
                      "nfo:audioOffset(?urn)",
                      "media");

  if (grl_tracker_upnp_present) {
    insert_key_mapping (GRL_METADATA_KEY_THUMBNAIL,
                        "upnp:thumbnail",
                        "upnp:thumbnail(?urn)",
                        "media");
  }
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
  return (GList *) g_hash_table_lookup (grl_to_sparql_mapping,
                                        GRLKEYID_TO_POINTER (key));
}

gboolean
grl_tracker_key_is_supported (const GrlKeyID key)
{
  return g_hash_table_lookup (grl_to_sparql_mapping,
                              GRLKEYID_TO_POINTER (key)) != NULL;
}

/**/

gchar *
grl_tracker_source_get_device_constraint (GrlTrackerSourcePriv *priv)
{
  if (priv->tracker_datasource == NULL ||
      priv->tracker_datasource[0] == '\0')
    return g_strdup ("");

  return g_strdup_printf ("?urn nie:dataSource <%s> .",
                          priv->tracker_datasource);
}

gchar *
grl_tracker_source_get_select_string (const GList *keys)
{
  const GList *key = keys;
  GString *gstr = g_string_new ("");
  GList *assoc_list;
  tracker_grl_sparql_t *assoc;

  assoc_list = get_mapping_from_grl (grl_metadata_key_tracker_urn);
  assoc = (tracker_grl_sparql_t *) assoc_list->data;
  g_string_append_printf (gstr, "%s AS %s ",
                          assoc->sparql_key_attr_call,
                          assoc->sparql_key_name);

  while (key != NULL) {
    assoc_list = get_mapping_from_grl (GRLPOINTER_TO_KEYID (key->data));
    while (assoc_list != NULL) {
      assoc = (tracker_grl_sparql_t *) assoc_list->data;
      if (assoc != NULL) {
        g_string_append_printf (gstr, "%s AS %s ",
                                assoc->sparql_key_attr_call,
                                assoc->sparql_key_name);
      }
      assoc_list = assoc_list->next;
    }
    key = key->next;
  }

  return g_string_free (gstr, FALSE);
}

static void
gen_prop_insert_string (GString *gstr,
                        tracker_grl_sparql_t *assoc,
                        GrlData *data)
{
  gchar *tmp;

  switch (GRL_METADATA_KEY_GET_TYPE (assoc->grl_key)) {
  case G_TYPE_STRING:
    tmp = g_strescape (grl_data_get_string (data, assoc->grl_key), NULL);
    g_string_append_printf (gstr, "%s \"%s\"",
                            assoc->sparql_key_attr, tmp);
    g_free (tmp);
    break;

  case G_TYPE_INT:
    g_string_append_printf (gstr, "%s %i",
                            assoc->sparql_key_attr,
                            grl_data_get_int (data, assoc->grl_key));
    break;

  case G_TYPE_FLOAT:
    g_string_append_printf (gstr, "%s %f",
                            assoc->sparql_key_attr,
                            grl_data_get_float (data, assoc->grl_key));
    break;

  default:
    break;
  }
}

gchar *
grl_tracker_tracker_get_insert_string (GrlMedia *media, const GList *keys)
{
  gboolean first = TRUE;
  const GList *key = keys, *assoc_list;
  tracker_grl_sparql_t *assoc;
  GString *gstr = g_string_new ("");
  gchar *ret;

  while (key != NULL) {
    assoc_list = get_mapping_from_grl (GRLPOINTER_TO_KEYID (key->data));
    while (assoc_list != NULL) {
      assoc = (tracker_grl_sparql_t *) assoc_list->data;
      if (assoc != NULL) {
        if (grl_data_has_key (GRL_DATA (media),
                              GRLPOINTER_TO_KEYID (key->data))) {
          if (first) {
            gen_prop_insert_string (gstr, assoc, GRL_DATA (media));
            first = FALSE;
          } else {
            g_string_append (gstr, " ; ");
            gen_prop_insert_string (gstr, assoc, GRL_DATA (media));
          }
        }
      }
      assoc_list = assoc_list->next;
    }
    key = key->next;
  }

  ret = gstr->str;
  g_string_free (gstr, FALSE);

  return ret;
}

gchar *
grl_tracker_get_delete_string (const GList *keys)
{
  gboolean first = TRUE;
  const GList *key = keys, *assoc_list;
  tracker_grl_sparql_t *assoc;
  GString *gstr = g_string_new ("");
  gchar *ret;
  gint var_n = 0;

  while (key != NULL) {
    assoc_list = get_mapping_from_grl (GRLPOINTER_TO_KEYID (key->data));
    while (assoc_list != NULL) {
      assoc = (tracker_grl_sparql_t *) assoc_list->data;
      if (assoc != NULL) {
        if (first) {
          g_string_append_printf (gstr, "%s ?v%i",
                                  assoc->sparql_key_attr, var_n);
          first = FALSE;
        } else {
          g_string_append_printf (gstr, " ; %s ?v%i",
                                  assoc->sparql_key_attr, var_n);
        }
        var_n++;
      }
      assoc_list = assoc_list->next;
    }
    key = key->next;
  }

  ret = gstr->str;
  g_string_free (gstr, FALSE);

  return ret;
}

gchar *
grl_tracker_get_delete_conditional_string (const gchar *urn,
                                           const GList *keys)
{
  gboolean first = TRUE;
  const GList *key = keys, *assoc_list;
  tracker_grl_sparql_t *assoc;
  GString *gstr = g_string_new ("");
  gchar *ret;
  gint var_n = 0;

  while (key != NULL) {
    assoc_list = get_mapping_from_grl (GRLPOINTER_TO_KEYID (key->data));
    while (assoc_list != NULL) {
      assoc = (tracker_grl_sparql_t *) assoc_list->data;
      if (assoc != NULL) {
        if (first) {
          g_string_append_printf (gstr, "OPTIONAL { <%s>  %s ?v%i }",
                                  urn,  assoc->sparql_key_attr, var_n);
          first = FALSE;
        } else {
          g_string_append_printf (gstr, " . OPTIONAL { <%s> %s ?v%i }",
                                  urn, assoc->sparql_key_attr, var_n);
        }
        var_n++;
      }
      assoc_list = assoc_list->next;
    }
    key = key->next;
  }

  ret = gstr->str;
  g_string_free (gstr, FALSE);

  return ret;
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
    } else if (g_str_has_suffix (rdf_single_type[i], RDF_TYPE_FOLDER)) {
      media = grl_media_box_new ();
    }
    i--;
  }

  g_strfreev (rdf_single_type);

  if (!media)
    media = grl_media_new ();

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

  if (uri != NULL && *uri != '\0') {
    volume_monitor = g_volume_monitor_get ();
    mounts = g_volume_monitor_get_mounts (volume_monitor);
    file = g_file_new_for_uri (uri);

    mount = mounts;
    while (mount != NULL) {
      GFile *m_file = g_mount_get_root (G_MOUNT (mount->data));

      if (g_file_equal (m_file, file)) {
        gchar *m_name = g_mount_get_name (G_MOUNT (mount->data));
        g_object_unref (G_OBJECT (m_file));
        source_name = g_strdup_printf (_("Removable - %s"), m_name);
        g_free (m_name);
        break;
      }
      g_object_unref (G_OBJECT (m_file));

      mount = mount->next;
    }
    g_list_free_full (mounts, g_object_unref);
    g_object_unref (G_OBJECT (file));
    g_object_unref (G_OBJECT (volume_monitor));
  } else {
    source_name = g_strdup (_("Local files"));
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

  return source_name;
}

const GList *
grl_tracker_supported_keys (GrlSource *source)
{
  static GList *supported_keys = NULL;

  if (!supported_keys) {
    supported_keys =  g_hash_table_get_keys (grl_to_sparql_mapping);
  }

  return supported_keys;
}
