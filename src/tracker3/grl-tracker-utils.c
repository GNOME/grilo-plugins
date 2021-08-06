/*
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
 * Copyright (C) 2020 Red Hat Inc.
 *
 * Contact: Carlos Garnacho <carlosg@gnome.org>
 *
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 *          Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
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
      || key == GRL_METADATA_KEY_LAST_PLAYED
      || key == GRL_METADATA_KEY_MODIFICATION_DATE
      || key == GRL_METADATA_KEY_PUBLICATION_DATE) {
    GDateTime *date = grl_date_time_from_iso8601 (str);
    if (date) {
      grl_data_set_boxed (GRL_DATA (media), key, date);
      g_date_time_unref (date);
    }
  }
}

static void
set_favourite (TrackerSparqlCursor *cursor,
               gint                 column,
               GrlMedia            *media,
               GrlKeyID             key)
{
  const gchar *str = tracker_sparql_cursor_get_string (cursor, column, NULL);
  gboolean is_favourite = FALSE;

  if (str != NULL && g_str_has_suffix (str, "predefined-tag-favorite"))
    is_favourite = TRUE;

  grl_data_set_boolean (GRL_DATA (media), key, is_favourite);
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

static void
set_string_metadata_keys (TrackerSparqlCursor *cursor,
                          gint                 column,
                          GrlMedia            *media,
                          GrlKeyID             key)
{
  const gchar *str = tracker_sparql_cursor_get_string (cursor, column, NULL);
  grl_data_set_string (GRL_DATA (media), key, str);
}

static void
set_int_metadata_keys (TrackerSparqlCursor *cursor,
                       gint                 column,
                       GrlMedia            *media,
                       GrlKeyID             key)
{
  const gint64 value = tracker_sparql_cursor_get_integer (cursor, column);
  grl_data_set_int (GRL_DATA (media), key, value);
}

static tracker_grl_sparql_t *
insert_key_mapping (GrlKeyID       grl_key,
                    const gchar   *sparql_var_name,
                    const gchar   *sparql_key_attr_call,
                    GrlTypeFilter  filter)
{
  tracker_grl_sparql_t *assoc;
  GList *assoc_list;
  gchar *canon_name;

  g_return_val_if_fail (grl_key != GRL_METADATA_KEY_INVALID, NULL);

  assoc = g_new0 (tracker_grl_sparql_t, 1);
  assoc_list = g_hash_table_lookup (grl_to_sparql_mapping,
                                    GRLKEYID_TO_POINTER (grl_key));
  canon_name = g_strdup (GRL_METADATA_KEY_GET_NAME (grl_key));

  assoc->grl_key               = grl_key;
  assoc->sparql_var_name       = sparql_var_name;
  assoc->sparql_key_attr_call  = sparql_key_attr_call;
  assoc->filter                = filter;

  assoc_list = g_list_append (assoc_list, assoc);

  g_hash_table_insert (grl_to_sparql_mapping,
                       GRLKEYID_TO_POINTER (grl_key),
                       assoc_list);
  g_hash_table_insert (sparql_to_grl_mapping,
                       (gpointer) assoc->sparql_var_name,
                       assoc);

  g_free (canon_name);

  return assoc;
}

static tracker_grl_sparql_t *
insert_key_mapping_with_setter (GrlKeyID                       grl_key,
                                const gchar                   *sparql_var_name,
                                const gchar                   *sparql_key_attr_call,
                                GrlTypeFilter                  filter,
                                tracker_grl_sparql_setter_cb_t setter)
{
  tracker_grl_sparql_t *assoc;

  assoc = insert_key_mapping (grl_key,
                              sparql_var_name,
                              sparql_key_attr_call,
                              filter);

  assoc->set_value = setter;

  return assoc;
}

void
grl_tracker_setup_key_mappings (void)
{
  GrlRegistry *registry = grl_registry_get_default ();
  GrlKeyID grl_metadata_key_chromaprint;

  grl_metadata_key_tracker_urn =
    grl_registry_lookup_metadata_key (registry, "tracker-urn");

  grl_metadata_key_gibest_hash =
    grl_registry_lookup_metadata_key (registry, "gibest-hash");

  grl_metadata_key_chromaprint =
    grl_registry_lookup_metadata_key (registry, "chromaprint");

  grl_to_sparql_mapping = g_hash_table_new (g_direct_hash, g_direct_equal);
  sparql_to_grl_mapping = g_hash_table_new (g_str_hash, g_str_equal);

  insert_key_mapping (grl_metadata_key_tracker_urn,
                      "urn",
                      "?urn",
                      GRL_TYPE_FILTER_ALL);

  insert_key_mapping (GRL_METADATA_KEY_ALBUM,
                      "album",
                      "nie:title(nmm:musicAlbum(?urn))",
                      GRL_TYPE_FILTER_AUDIO);

  insert_key_mapping (GRL_METADATA_KEY_ALBUM_DISC_NUMBER,
                      "albumDiscNumber",
                      "nmm:setNumber(nmm:musicAlbumDisc(?urn))",
                      GRL_TYPE_FILTER_AUDIO);

  insert_key_mapping (GRL_METADATA_KEY_ARTIST,
                      "artist",
                      "nmm:artistName(nmm:artist(?urn))",
                      GRL_TYPE_FILTER_AUDIO);

  insert_key_mapping (GRL_METADATA_KEY_ALBUM_ARTIST,
                      "albumArtist",
                      "nmm:artistName(nmm:albumArtist(nmm:musicAlbum(?urn)))",
                      GRL_TYPE_FILTER_AUDIO);

  insert_key_mapping (GRL_METADATA_KEY_AUTHOR,
                      "author",
                      "nmm:artistName(nmm:artist(?urn))",
                      GRL_TYPE_FILTER_AUDIO);

  insert_key_mapping (GRL_METADATA_KEY_BITRATE,
                      "bitrate",
                      "nfo:averageBitrate(?urn)",
                      GRL_TYPE_FILTER_AUDIO | GRL_TYPE_FILTER_VIDEO);

  insert_key_mapping (GRL_METADATA_KEY_CHILDCOUNT,
                      "childCount",
                      "nfo:entryCounter(?urn)",
                      GRL_TYPE_FILTER_ALL);

  insert_key_mapping (GRL_METADATA_KEY_COMPOSER,
                      "composer",
                      "nmm:artistName(nmm:composer(?urn))",
                      GRL_TYPE_FILTER_AUDIO);

  insert_key_mapping (GRL_METADATA_KEY_SIZE,
                      "size",
                      "nie:byteSize(?urn)",
                      GRL_TYPE_FILTER_ALL);

  insert_key_mapping (grl_metadata_key_gibest_hash,
                      "gibestHash",
                      "(select nfo:hashValue(?h) { ?urn nie:isStoredAs/nfo:hasHash ?h . ?h nfo:hashAlgorithm \"gibest\" })",
                      GRL_TYPE_FILTER_VIDEO);

  insert_key_mapping_with_setter (GRL_METADATA_KEY_MODIFICATION_DATE,
                                  "lastModified",
                                  "COALESCE(nie:contentLastModified(?urn), (select ?lm { ?urn nie:isStoredAs/nfo:fileLastModified ?lm }))",
                                  GRL_TYPE_FILTER_ALL,
                                  set_date);

  insert_key_mapping (GRL_METADATA_KEY_DURATION,
                      "duration",
                      "nfo:duration(?urn)",
                      GRL_TYPE_FILTER_AUDIO | GRL_TYPE_FILTER_VIDEO);

  insert_key_mapping (GRL_METADATA_KEY_MB_TRACK_ID,
                      "mbTrack",
                      "(SELECT tracker:referenceIdentifier(?t) AS ?t_id { ?urn tracker:hasExternalReference ?t . ?t tracker:referenceSource \"https://musicbrainz.org/doc/Track\" })",
                      GRL_TYPE_FILTER_AUDIO);

  insert_key_mapping (GRL_METADATA_KEY_MB_ARTIST_ID,
                      "mbArtist",
		      "(SELECT tracker:referenceIdentifier(?a) AS ?a_id { ?urn nmm:artist ?artist . ?artist tracker:hasExternalReference ?a . ?a tracker:referenceSource \"https://musicbrainz.org/doc/Artist\" })",
                      GRL_TYPE_FILTER_AUDIO);

  insert_key_mapping (GRL_METADATA_KEY_MB_RECORDING_ID,
                      "mbRecording",
		      "(SELECT tracker:referenceIdentifier(?r) AS ?r_id { ?urn tracker:hasExternalReference ?r . ?r tracker:referenceSource \"https://musicbrainz.org/doc/Recording\" })",
                      GRL_TYPE_FILTER_AUDIO);

  insert_key_mapping (GRL_METADATA_KEY_MB_RELEASE_ID,
                      "mbRelease",
		      "(SELECT tracker:referenceIdentifier(?re) AS ?re_id { ?urn nmm:musicAlbum ?album . ?album tracker:hasExternalReference ?re . ?re tracker:referenceSource \"https://musicbrainz.org/doc/Release\" })",
                      GRL_TYPE_FILTER_AUDIO);

  insert_key_mapping (GRL_METADATA_KEY_MB_RELEASE_GROUP_ID,
                      "mbReleaseGroup",
		      "(SELECT tracker:referenceIdentifier(?rg) AS ?rg_id { ?urn nmm:musicAlbum ?album . ?album tracker:hasExternalReference ?rg . ?rg tracker:referenceSource \"https://musicbrainz.org/doc/Release_Group\" })",
                      GRL_TYPE_FILTER_AUDIO);

  if (grl_metadata_key_chromaprint != 0) {
    insert_key_mapping_with_setter (grl_metadata_key_chromaprint,
                                    "chromaprint",
                                    "(select nfo:hashValue(?h) { ?urn nie:isStoredAs/nfo:hasHash ?h . ?h nfo:hashAlgorithm \"chromaprint\" })",
                                    GRL_TYPE_FILTER_AUDIO,
                                    set_string_metadata_keys);
  };

  insert_key_mapping (GRL_METADATA_KEY_FRAMERATE,
                      "frameRate",
                      "nfo:frameRate(?urn)",
                      GRL_TYPE_FILTER_VIDEO);

  insert_key_mapping (GRL_METADATA_KEY_HEIGHT,
                      "height",
                      "nfo:height(?urn)",
                      GRL_TYPE_FILTER_VIDEO | GRL_TYPE_FILTER_IMAGE);

  insert_key_mapping (GRL_METADATA_KEY_ID,
                      "id",
                      "?urn",
                      GRL_TYPE_FILTER_ALL);

  insert_key_mapping (GRL_METADATA_KEY_MIME,
                      "mimeType",
                      "nie:mimeType(?urn)",
                      GRL_TYPE_FILTER_ALL);

  insert_key_mapping (GRL_METADATA_KEY_SITE,
                      "siteUrl",
                      "nie:isStoredAs(?urn)",
                      GRL_TYPE_FILTER_ALL);

  insert_key_mapping_with_setter (GRL_METADATA_KEY_TITLE,
                                  "title",
                                  "nie:title(?urn)",
                                  GRL_TYPE_FILTER_ALL,
                                  set_title);

  insert_key_mapping (GRL_METADATA_KEY_URL,
                      "url",
                      "nie:isStoredAs(?urn)",
                      GRL_TYPE_FILTER_ALL);

  insert_key_mapping (GRL_METADATA_KEY_WIDTH,
                      "width",
                      "nfo:width(?urn)",
                      GRL_TYPE_FILTER_VIDEO | GRL_TYPE_FILTER_IMAGE);

  insert_key_mapping (GRL_METADATA_KEY_SEASON,
                      "season",
                      "nmm:seasonNumber(nmm:isPartOfSeason(?urn))",
                      GRL_TYPE_FILTER_VIDEO);

  insert_key_mapping (GRL_METADATA_KEY_EPISODE,
                      "episode",
                      "nmm:episodeNumber(?urn)",
                      GRL_TYPE_FILTER_VIDEO);

  insert_key_mapping_with_setter (GRL_METADATA_KEY_CREATION_DATE,
                                  "creationDate",
                                  "nie:contentCreated(?urn)",
                                  GRL_TYPE_FILTER_ALL,
                                  set_date);

  insert_key_mapping_with_setter (GRL_METADATA_KEY_PUBLICATION_DATE,
                                  "publicationDate",
                                  "nie:contentCreated(?urn)",
                                  GRL_TYPE_FILTER_ALL,
                                  set_date);

  insert_key_mapping (GRL_METADATA_KEY_CAMERA_MODEL,
                      "cameraModel",
                      "nfo:model(nfo:equipment(?urn))",
                      GRL_TYPE_FILTER_IMAGE);

  insert_key_mapping (GRL_METADATA_KEY_FLASH_USED,
                      "flashUsed",
                      "nmm:flash(?urn)",
                      GRL_TYPE_FILTER_IMAGE);

  insert_key_mapping (GRL_METADATA_KEY_EXPOSURE_TIME,
                      "exposureTime",
                      "nmm:exposureTime(?urn)",
                      GRL_TYPE_FILTER_IMAGE);

  insert_key_mapping (GRL_METADATA_KEY_ISO_SPEED,
                      "isoSpeed",
                      "nmm:isoSpeed(?urn)",
                      GRL_TYPE_FILTER_IMAGE);

  insert_key_mapping_with_setter (GRL_METADATA_KEY_ORIENTATION,
                                  "orientation",
                                  "nfo:orientation(?urn)",
                                  GRL_TYPE_FILTER_IMAGE,
                                  set_orientation);

  insert_key_mapping (GRL_METADATA_KEY_PLAY_COUNT,
                      "playCount",
                      "nie:usageCounter(?urn)",
                      GRL_TYPE_FILTER_AUDIO | GRL_TYPE_FILTER_VIDEO);

  insert_key_mapping_with_setter (GRL_METADATA_KEY_LAST_PLAYED,
                                  "lastPlayed",
                                  "nie:contentAccessed(?urn)",
                                  GRL_TYPE_FILTER_ALL,
                                  set_date);

  insert_key_mapping (GRL_METADATA_KEY_LAST_POSITION,
                      "lastPlayPosition",
                      "nfo:lastPlayedPosition(?urn)",
                      GRL_TYPE_FILTER_AUDIO | GRL_TYPE_FILTER_VIDEO);

  insert_key_mapping (GRL_METADATA_KEY_START_TIME,
                      "startTime",
                      "nfo:audioOffset(?urn)",
                      GRL_TYPE_FILTER_AUDIO);

  insert_key_mapping_with_setter (GRL_METADATA_KEY_TRACK_NUMBER,
                                  "trackNumber",
                                  "nmm:trackNumber(?urn)",
                                  GRL_TYPE_FILTER_AUDIO,
                                  set_int_metadata_keys);

  insert_key_mapping_with_setter (GRL_METADATA_KEY_FAVOURITE,
                                  "favorite",
                                  "nao:hasTag(?urn)",
                                  GRL_TYPE_FILTER_ALL,
                                  set_favourite);
}

tracker_grl_sparql_t *
grl_tracker_get_mapping_from_sparql (const gchar *key)
{
  return (tracker_grl_sparql_t *) g_hash_table_lookup (sparql_to_grl_mapping,
                                                       key);
}

gboolean
grl_tracker_key_is_supported (const GrlKeyID key)
{
  return g_hash_table_lookup (grl_to_sparql_mapping,
                              GRLKEYID_TO_POINTER (key)) != NULL;
}

/**/

/* Builds an appropriate GrlMedia based on tracker query results */
GrlMedia *
grl_tracker_build_grilo_media (GrlMediaType type)
{
  GrlMedia *media = NULL;

  if (type == GRL_MEDIA_TYPE_AUDIO) {
    media = grl_media_audio_new ();
  } else if (type == GRL_MEDIA_TYPE_VIDEO) {
    media = grl_media_video_new ();
  } else if (type == GRL_MEDIA_TYPE_IMAGE) {
    media = grl_media_image_new ();
  } else if (type == GRL_MEDIA_TYPE_CONTAINER) {
    media = grl_media_container_new ();
  }

  if (!media)
    media = grl_media_new ();

  return media;
}

/**/

const GList *
grl_tracker_supported_keys (GrlSource *source)
{
  static GList *supported_keys = NULL;

  if (!supported_keys) {
    supported_keys =  g_hash_table_get_keys (grl_to_sparql_mapping);
  }

  return supported_keys;
}

const gchar *
grl_tracker_key_get_variable_name (const GrlKeyID key)
{
  tracker_grl_sparql_t *assoc;
  GList *assoc_list;

  assoc_list = g_hash_table_lookup (grl_to_sparql_mapping,
                                    GRLKEYID_TO_POINTER (key));
  if (!assoc_list)
    return NULL;
  assoc = assoc_list->data;

  return assoc->sparql_var_name;
}

const gchar *
grl_tracker_key_get_sparql_statement (const GrlKeyID key,
                                      GrlTypeFilter  filter)
{
  tracker_grl_sparql_t *assoc;
  GList *assoc_list;

  assoc_list = g_hash_table_lookup (grl_to_sparql_mapping,
                                    GRLKEYID_TO_POINTER (key));
  if (!assoc_list)
    return NULL;

  assoc = assoc_list->data;
  if ((assoc->filter & filter) == 0)
    return NULL;

  return assoc->sparql_key_attr_call;
}

static TrackerResource *
ensure_resource_for_property (TrackerResource *resource,
                              const gchar     *prop,
                              gboolean         multivalued)
{
  TrackerResource *child = NULL;

  if (!multivalued)
    child = tracker_resource_get_first_relation (resource, prop);

  if (!child) {
    child = tracker_resource_new (NULL);
    tracker_resource_add_take_relation (resource, prop, child);
  }

  return child;
}

static TrackerResource *
ensure_resource_for_musicbrainz_tag (TrackerResource *resource,
                                     const gchar     *source,
                                     const gchar     *identifier)
{
  TrackerResource *reference;

  reference = ensure_resource_for_property (resource,
                                            "tracker:hasExternalReference",
                                            TRUE);
  tracker_resource_set_uri (reference,
                            "tracker:referenceSource",
                            source);
  tracker_resource_set_string (reference,
                               "tracker:referenceIdentifier",
                               identifier);
  return reference;
}

TrackerResource *
grl_tracker_build_resource_from_media (GrlMedia *media, GList *keys)
{
  TrackerResource *resource;
  GrlRegistry *registry;
  GrlKeyID grl_metadata_key_chromaprint;
  GrlMediaType type;
  GList *l;

  registry = grl_registry_get_default ();
  grl_metadata_key_chromaprint = grl_registry_lookup_metadata_key (registry, "chromaprint");

  resource = tracker_resource_new (NULL);
  tracker_resource_set_uri (resource, "nie:isStoredAs",
                            grl_media_get_url (media));

  type = grl_media_get_media_type (media);
  if (type & GRL_MEDIA_TYPE_IMAGE)
    tracker_resource_add_uri (resource, "rdf:type", "nfo:Image");
  if (type & GRL_MEDIA_TYPE_AUDIO)
    tracker_resource_add_uri (resource, "rdf:type", "nfo:Audio");
  if (type & GRL_MEDIA_TYPE_VIDEO)
    tracker_resource_add_uri (resource, "rdf:type", "nfo:Video");

  for (l = keys; l; l = l->next) {
    if (l->data == GRLKEYID_TO_POINTER (GRL_METADATA_KEY_TITLE)) {
      tracker_resource_set_string (resource, "nie:title",
                                   grl_media_get_title (media));
    } else if (l->data == GRLKEYID_TO_POINTER (GRL_METADATA_KEY_TRACK_NUMBER)) {
      tracker_resource_set_int (resource, "nmm:trackNumber",
                                grl_media_get_track_number (media));
    } else if (l->data == GRLKEYID_TO_POINTER (GRL_METADATA_KEY_EPISODE)) {
      tracker_resource_set_int (resource, "nmm:episodeNumber",
                                grl_media_get_episode (media));
    } else if (l->data == GRLKEYID_TO_POINTER (GRL_METADATA_KEY_CREATION_DATE)) {
      GDateTime *creation;
      gchar *date;

      creation = grl_media_get_creation_date (media);
      date = g_date_time_format_iso8601 (creation);
      tracker_resource_set_string (resource, "nie:contentCreated", date);
      g_free (date);
    } else if (l->data == GRLKEYID_TO_POINTER (GRL_METADATA_KEY_PUBLICATION_DATE)) {
      GDateTime *publication;
      gchar *date;

      publication = grl_media_get_publication_date (media);
      date = g_date_time_format_iso8601 (publication);
      tracker_resource_set_string (resource, "nie:contentCreated", date);
      g_free (date);
    } else if (l->data == GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ALBUM)) {
      TrackerResource *album;
      album = ensure_resource_for_property (resource, "nmm:musicAlbum", FALSE);
      tracker_resource_set_string (album, "nie:title",
                                   grl_media_get_album (media));

      /* Handle MB release/release group inline */
      if (g_list_find (keys, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_MB_RELEASE_ID))) {
        const gchar *mb_release_id;

        mb_release_id = grl_media_get_mb_release_id (media);
        if (mb_release_id) {
          ensure_resource_for_musicbrainz_tag (album,
                                               "https://musicbrainz.org/doc/Release",
                                               mb_release_id);
        }
      }

      if (g_list_find (keys, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_MB_RELEASE_GROUP_ID))) {
        const gchar *mb_release_group_id;

        mb_release_group_id = grl_media_get_mb_release_group_id (media);
        if (mb_release_group_id) {
          ensure_resource_for_musicbrainz_tag (album,
                                               "https://musicbrainz.org/doc/Release_Group",
                                               mb_release_group_id);
        }
      }
    } else if (l->data == GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ALBUM_DISC_NUMBER)) {
      TrackerResource *disc;
      disc = ensure_resource_for_property (resource, "nmm:musicAlbumDisc", FALSE);
      tracker_resource_set_int (disc, "nmm:setNumber",
                                grl_media_get_album_disc_number (media));
    } else if (l->data == GRLKEYID_TO_POINTER (GRL_METADATA_KEY_SEASON)) {
      TrackerResource *season;
      season = ensure_resource_for_property (resource, "nmm:isPartOfSeason", FALSE);
      tracker_resource_set_int (season, "nmm:seasonNumber",
                                grl_media_get_season (media));
    } else if (l->data == GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ALBUM_ARTIST)) {
      TrackerResource *album, *album_artist;
      album = ensure_resource_for_property (resource, "nmm:musicAlbum", FALSE);
      album_artist = ensure_resource_for_property (album, "nmm:albumArtist", FALSE);
      tracker_resource_set_string (album_artist, "nmm:artistName",
                                   grl_media_get_album_artist (media));
    } else if (l->data == GRLKEYID_TO_POINTER (GRL_METADATA_KEY_MB_RECORDING_ID)) {
      const gchar *mb_recording_id;
      mb_recording_id = grl_media_get_mb_recording_id (media);
      if (mb_recording_id) {
        ensure_resource_for_musicbrainz_tag (resource,
                                             "https://musicbrainz.org/doc/Recording",
                                             mb_recording_id);
      }
    } else if (l->data == GRLKEYID_TO_POINTER (GRL_METADATA_KEY_MB_TRACK_ID)) {
      const gchar *mb_track_id;
      mb_track_id = grl_media_get_mb_track_id (media);
      if (mb_track_id) {
        ensure_resource_for_musicbrainz_tag (resource,
                                             "https://musicbrainz.org/doc/Track",
                                             mb_track_id);
      }
    } else if (l->data == GRLKEYID_TO_POINTER (grl_metadata_key_chromaprint)) {
      TrackerResource *hash;
      hash = ensure_resource_for_property (resource, "nfo:hasHash", FALSE);
      tracker_resource_set_string (hash, "nfo:hashAlgorithm", "chromaprint");
      tracker_resource_set_string (hash, "nfo:hashValue",
                                   grl_data_get_string (GRL_DATA (media), grl_metadata_key_chromaprint));
    } else if (l->data == GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ARTIST)) {
      TrackerResource *artist = NULL;
      const gchar *artist_name;
      gint i = 0;

      while (TRUE) {
        artist_name = grl_media_get_artist_nth (media, i);
        if (!artist_name)
          break;

        artist = ensure_resource_for_property (resource, "nmm:artist", TRUE);
        tracker_resource_set_string (artist, "nmm:artistName", artist_name);

        /* Handle MB artist inline */
        if (g_list_find (keys, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_MB_ARTIST_ID))) {
          const gchar *mb_artist_id;

          mb_artist_id = grl_media_get_mb_artist_id_nth (media, i);
          if (mb_artist_id) {
            ensure_resource_for_musicbrainz_tag (artist,
                                                 "https://musicbrainz.org/doc/Artist",
                                                 mb_artist_id);
          }
        }
        i++;
      }
    } else if (l->data == GRLKEYID_TO_POINTER (GRL_METADATA_KEY_AUTHOR)) {
      TrackerResource *artist;
      const gchar *artist_name;
      gint i = 0;

      while (TRUE) {
        artist_name = grl_media_get_artist_nth (media, i);
        if (!artist_name)
          break;

        artist = ensure_resource_for_property (resource, "nmm:artist", TRUE);
        tracker_resource_set_string (artist, "nmm:artistName", artist_name);
        i++;
      }
    } else if (l->data == GRLKEYID_TO_POINTER (GRL_METADATA_KEY_COMPOSER)) {
      TrackerResource *composer;
      const gchar *composer_name;
      gint i = 0;

      while (TRUE) {
        composer_name = grl_media_get_composer_nth (media, i);
        if (!composer_name)
          break;

        composer = ensure_resource_for_property (resource, "nmm:composer", TRUE);
        tracker_resource_set_string (composer, "nmm:artistName", composer_name);
        i++;
      }
    }
  }

  return resource;
}
