/*
 * Copyright (C) 2010, 2011 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
 * Copyright (C) 2013 Intel Corporation.
 *
 * This component is based on the grl-upnp source code.
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

#include "config.h"

#include "grl-dleyna-source.h"
#include "grl-dleyna-utils.h"

#include <grilo.h>
#include <glib/gi18n-lib.h>

#define SOURCE_ID_TEMPLATE    "grl-dleyna-%s"
#define SOURCE_DESC_TEMPLATE  _("A source for browsing the DLNA server “%s”")

#define MEDIA_ID_PREFIX "dleyna:"
#define MEDIA_ID_PREFIX_LENGTH 7

#define DLEYNA_SEARCH_SPEC                \
  "(DisplayName contains \"%s\" or "      \
  "Album contains \"%s\" or "             \
  "Artist contains \"%s\")"

#define DLEYNA_BROWSE_SPEC                \
  "Parent = \"%s\""

#define DLEYNA_TYPE_FILTER_AUDIO          \
  "Type derivedfrom \"audio\" or Type derivedfrom \"music\""

#define DLEYNA_TYPE_FILTER_VIDEO          \
  "Type derivedfrom \"video\""

#define DLEYNA_TYPE_FILTER_IMAGE          \
  "Type derivedfrom \"image\""

#define DLEYNA_TYPE_FILTER_CONTAINER      \
  "Type derivedfrom \"container\""

#define GRL_LOG_DOMAIN_DEFAULT dleyna_log_domain
GRL_LOG_DOMAIN_EXTERN(dleyna_log_domain);

struct _GrlDleynaSourcePrivate {
  GrlDleynaServer *server;
  GHashTable *uploads;
  gboolean search_enabled;
  gboolean browse_filtered_enabled;
};

enum {
  PROP_0,
  PROP_SERVER
};

typedef enum {
  CONTAINER_TYPE_ALBUM,
  CONTAINER_TYPE_ARTIST,
  CONTAINER_TYPE_GENRE,
  CONTAINER_TYPE_UNKNOWN,
  CONTAINER_TYPE_NOT_CONTAINER
} ContainerType;

typedef enum {
  DLEYNA_CHANGE_TYPE_ADD = 1,
  DLEYNA_CHANGE_TYPE_MOD = 2,
  DLEYNA_CHANGE_TYPE_DEL = 3,
  DLEYNA_CHANGE_TYPE_DONE = 4,
  DLEYNA_CHANGE_TYPE_CONTAINER = 5
} DleynaChangeType;

/* ================== Prototypes ================== */

static void            grl_dleyna_source_dispose              (GObject *object);
static void            grl_dleyna_source_set_property         (GObject *object,
                                                               guint prop_id,
                                                               const GValue *value,
                                                               GParamSpec *pspec);
static void            grl_dleyna_source_get_property         (GObject *object,
                                                               guint prop_id,
                                                               GValue *value,
                                                               GParamSpec *pspec);
static GrlCaps *       grl_dleyna_source_get_caps             (GrlSource *source,
                                                               GrlSupportedOps operation);
static const GList *   grl_dleyna_source_supported_keys       (GrlSource *source);
static const GList *   grl_dleyna_source_writable_keys        (GrlSource *source);
static GrlSupportedOps grl_dleyna_source_supported_operations (GrlSource *source);
static void            grl_dleyna_source_resolve              (GrlSource *source,
                                                               GrlSourceResolveSpec *rs);
static void            grl_dleyna_source_browse               (GrlSource *source,
                                                               GrlSourceBrowseSpec *bs);
static void            grl_dleyna_source_search               (GrlSource *source,
                                                               GrlSourceSearchSpec *ss);
static void            grl_dleyna_source_query                (GrlSource *source,
                                                               GrlSourceQuerySpec *qs);
static void            grl_dleyna_source_store                (GrlSource *source,
                                                               GrlSourceStoreSpec *ss);
static void            grl_dleyna_source_store_metadata       (GrlSource *source,
                                                               GrlSourceStoreMetadataSpec *sms);
static void            grl_dleyna_source_remove               (GrlSource *source,
                                                               GrlSourceRemoveSpec *rs);
static void            grl_dleyna_source_cancel               (GrlSource *source,
                                                               guint operation_id);
static gboolean        grl_dleyna_source_notify_change_start  (GrlSource *source,
                                                               GError **error);
static gboolean        grl_dleyna_source_notify_change_stop   (GrlSource *source,
                                                               GError **error);
static void            grl_dleyna_source_upload_destroy       (GrlSourceStoreSpec *ss);
static void            grl_dleyna_source_set_server           (GrlDleynaSource *source,
                                                               GrlDleynaServer *server);

/* ================== GObject API implementation ================== */

G_DEFINE_TYPE_WITH_PRIVATE (GrlDleynaSource, grl_dleyna_source, GRL_TYPE_SOURCE)

static void
grl_dleyna_source_class_init (GrlDleynaSourceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

  gobject_class->dispose = grl_dleyna_source_dispose;
  gobject_class->get_property = grl_dleyna_source_get_property;
  gobject_class->set_property = grl_dleyna_source_set_property;

  source_class->get_caps = grl_dleyna_source_get_caps;
  source_class->supported_keys = grl_dleyna_source_supported_keys;
  source_class->writable_keys = grl_dleyna_source_writable_keys;
  source_class->supported_operations = grl_dleyna_source_supported_operations;
  source_class->resolve = grl_dleyna_source_resolve;
  source_class->browse = grl_dleyna_source_browse;
  source_class->search = grl_dleyna_source_search;
  source_class->query = grl_dleyna_source_query;
  source_class->store = grl_dleyna_source_store;
  source_class->store_metadata = grl_dleyna_source_store_metadata;
  source_class->remove = grl_dleyna_source_remove;
  source_class->cancel = grl_dleyna_source_cancel;
  source_class->notify_change_start = grl_dleyna_source_notify_change_start;
  source_class->notify_change_stop = grl_dleyna_source_notify_change_stop;

  g_object_class_install_property (gobject_class,
                                   PROP_SERVER,
                                   g_param_spec_object ("server",
                                                        "Server",
                                                        "The DLNA Media Server (DMS) this source refer to.",
                                                        GRL_TYPE_DLEYNA_SERVER,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));
}

static void
grl_dleyna_source_init (GrlDleynaSource *source)
{
  source->priv = grl_dleyna_source_get_instance_private (source);
  source->priv->uploads = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
                                                 (GDestroyNotify)grl_dleyna_source_upload_destroy);
}

static void
grl_dleyna_source_dispose (GObject *object)
{
  GrlDleynaSource *source = GRL_DLEYNA_SOURCE (object);

  g_clear_object (&source->priv->server);

  G_OBJECT_CLASS (grl_dleyna_source_parent_class)->dispose (object);
}

static void
grl_dleyna_source_set_property (GObject *object,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
  GrlDleynaSource *source = GRL_DLEYNA_SOURCE (object);

  switch (prop_id) {
    case PROP_SERVER:
      grl_dleyna_source_set_server (source, GRL_DLEYNA_SERVER (g_value_get_object (value)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
grl_dleyna_source_get_property (GObject *object,
                                guint prop_id,
                                GValue *value,
                                GParamSpec *pspec)
{
  GrlDleynaSource *source = GRL_DLEYNA_SOURCE (object);

  switch (prop_id) {
    case PROP_SERVER:
      g_value_set_object (value, source->priv->server);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* ================== Private functions ================== */

static GError *
grl_dleyna_source_convert_error (GError *src,
                                 gint code)
{
  GError *dst;
  dst = g_error_new_literal (GRL_CORE_ERROR, code, src->message);
  g_error_free (src);
  return dst;
}

static const gchar *
grl_dleyna_source_media_get_object_path_from_id (const gchar *id)
{
  g_return_val_if_fail (g_str_has_prefix(id, "dleyna:"), NULL);
  return id + MEDIA_ID_PREFIX_LENGTH;
}

static const gchar *
grl_dleyna_source_media_get_object_path (GrlMedia *media)
{
  const gchar *id;

  if (media == NULL) {
    return NULL;
  }

  id = grl_media_get_id (media);
  if (id == NULL) {
    return NULL;
  }

  return grl_dleyna_source_media_get_object_path_from_id (id);
}

static void
grl_dleyna_source_media_set_id_from_object_path (GrlMedia    *media,
                                                 const gchar *object_path)
{
  gchar *id;

  id = g_strdup_printf (MEDIA_ID_PREFIX "%s", object_path);
  grl_media_set_id (media, id);
  g_free (id);
}

static void
grl_dleyna_source_upload_destroy (GrlSourceStoreSpec *ss)
{
  GError *error;

  error = g_error_new_literal (GRL_CORE_ERROR, GRL_CORE_ERROR_STORE_FAILED,
                               _("Upload failed, target source destroyed"));
  ss->callback (ss->source, ss->media, NULL, ss->user_data, error);
  g_error_free (error);
}

static void
grl_dleyna_source_update_caps_cb (GObject    *gobject,
                                  GParamSpec *pspec,
                                  gpointer    user_data)
{
  GrlDleynaSource *source = GRL_DLEYNA_SOURCE (gobject);
  GrlDleynaMediaDevice *device = GRL_DLEYNA_MEDIA_DEVICE (user_data);
  const gchar * const *search_caps;

  search_caps = grl_dleyna_media_device_get_search_caps (device);
  if (search_caps == NULL) {
    GRL_DEBUG ("%s caps:NULL", G_STRFUNC);
    source->priv->search_enabled = FALSE;
    source->priv->browse_filtered_enabled = FALSE;
  }
  else if (g_strv_length ((gchar **) search_caps) == 1 && g_strcmp0 ("*", search_caps[0]) == 0) {
    GRL_DEBUG ("%s caps:*", G_STRFUNC);
    source->priv->search_enabled = TRUE;
    source->priv->browse_filtered_enabled = TRUE;
  }
  else {
    gchar **cap;
    gboolean type, type_ex, displayname, album, artist, parent;
    type = type_ex = displayname = album = artist = parent = FALSE;

    GRL_DEBUG ("%s caps:", G_STRFUNC);
    for (cap = (gchar **) search_caps; *cap != NULL; cap++) {
      GRL_DEBUG ("  %s", *cap);
      type = type || (g_strcmp0(*cap, "Type") == 0);
      type_ex = type_ex || (g_strcmp0 (*cap, "TypeEx") == 0);
      displayname = displayname || (g_strcmp0(*cap, "DisplayName") == 0);
      album = album || (g_strcmp0(*cap, "Album") == 0);
      artist = artist || (g_strcmp0(*cap, "Artist") == 0);
      parent = parent || (g_strcmp0(*cap, "Parent") == 0);
    }

    source->priv->search_enabled = type && type_ex && (displayname || album || artist);
    source->priv->browse_filtered_enabled = type && type_ex && parent;
  }

  GRL_DEBUG ("%s %s search:%d filtered:%d", G_STRFUNC, grl_source_get_id (GRL_SOURCE (source)),
             source->priv->search_enabled, source->priv->browse_filtered_enabled);
}

static void
grl_dleyna_source_store_upload_completed (GrlSourceStoreSpec *ss,
                                          const gchar        *object_path,
                                          GError             *error)
{
  GList *failed_keys;

  GRL_DEBUG ("%s", G_STRFUNC);

  if (error != NULL) {
    GRL_WARNING ("%s error:%s", G_STRFUNC, error->message);
    error = grl_dleyna_source_convert_error (error, GRL_CORE_ERROR_STORE_FAILED);
    ss->callback (ss->source, ss->media, NULL, ss->user_data, error);
    g_error_free (error);
    return;
  }

  if (object_path != NULL) {
    grl_dleyna_source_media_set_id_from_object_path (ss->media, object_path);
  }

  failed_keys = grl_data_get_keys (GRL_DATA (ss->media));
  failed_keys = g_list_remove (failed_keys, GINT_TO_POINTER (GRL_METADATA_KEY_URL));
  failed_keys = g_list_remove (failed_keys, GINT_TO_POINTER (GRL_METADATA_KEY_ID));
  failed_keys = g_list_remove (failed_keys, GINT_TO_POINTER (GRL_METADATA_KEY_TITLE));

  ss->callback (ss->source, ss->media, failed_keys, ss->user_data, error);

  g_list_free (failed_keys);
}

static void
grl_dleyna_source_store_upload_update_cb (GrlDleynaSource      *self,
                                          guint                 upload_id,
                                          const gchar          *upload_status,
                                          guint64               length,
                                          guint64               total,
                                          GrlDleynaMediaDevice *mediadevice)
{
  GrlSourceStoreSpec *ss;
  GError *error = NULL;

  ss = g_hash_table_lookup (self->priv->uploads, GUINT_TO_POINTER (upload_id));

  /* Upload status notification for an upload started by someone else, ignore */
  if (ss == NULL) {
    return;
  }

  /* Prevent grl_dleyna_source_upload_destroy() from being called */
  g_hash_table_steal (self->priv->uploads, GUINT_TO_POINTER (upload_id));

  if (!g_str_equal (upload_status, "COMPLETED")) {
    error = g_error_new (GRL_CORE_ERROR, GRL_CORE_ERROR_STORE_FAILED,
                          _("Upload failed, “%s”, transferred %lu of %lu bytes"),
                          upload_status,
                          (long unsigned int) length,
                          (long unsigned int) total);
    GRL_WARNING ("%s error:%s", G_STRFUNC, error->message);
  }

  grl_dleyna_source_store_upload_completed (ss, NULL, error);
}

static void
grl_dleyna_source_set_server (GrlDleynaSource *source,
                              GrlDleynaServer *server)
{
  GrlDleynaMediaDevice *device;

  GRL_DEBUG (G_STRFUNC);

  g_return_if_fail (source->priv->server == NULL);

  source->priv->server = g_object_ref (server);

  device = grl_dleyna_server_get_media_device (server);

  g_signal_connect_object (device, "notify::search-caps", G_CALLBACK (grl_dleyna_source_update_caps_cb),
                           source, G_CONNECT_SWAPPED);
  grl_dleyna_source_update_caps_cb (G_OBJECT (source), NULL, device);

  g_signal_connect_object (device, "upload-update", G_CALLBACK(grl_dleyna_source_store_upload_update_cb),
                           source, G_CONNECT_SWAPPED);
}

static void
properties_add_for_key (GPtrArray *properties,
                        const GrlKeyID key_id)
{
  switch (key_id)
    {
    case GRL_METADATA_KEY_ID:
      g_ptr_array_add (properties, "UDN");
      break;
    case GRL_METADATA_KEY_TITLE:
      g_ptr_array_add (properties, "DisplayName");
      break;
    case GRL_METADATA_KEY_URL:
      g_ptr_array_add (properties, "URLs");
      break;
    case GRL_METADATA_KEY_AUTHOR:
      g_ptr_array_add (properties, "Creator");
      break;
    case GRL_METADATA_KEY_ARTIST:
      g_ptr_array_add (properties, "Artist");
      break;
    case GRL_METADATA_KEY_ALBUM:
      g_ptr_array_add (properties, "Album");
      break;
    case GRL_METADATA_KEY_GENRE:
      g_ptr_array_add (properties, "Genre");
      break;
    case GRL_METADATA_KEY_DURATION:
      g_ptr_array_add (properties, "Duration");
      break;
    case GRL_METADATA_KEY_TRACK_NUMBER:
      g_ptr_array_add (properties, "TrackNumber");
      break;
    case GRL_METADATA_KEY_MIME:
      g_ptr_array_add (properties, "MIMEType");
      break;
    case GRL_METADATA_KEY_WIDTH:
      g_ptr_array_add (properties, "Width");
      break;
    case GRL_METADATA_KEY_HEIGHT:
      g_ptr_array_add (properties, "Height");
      break;
    case GRL_METADATA_KEY_BITRATE:
      g_ptr_array_add (properties, "Bitrate");
      break;
    case GRL_METADATA_KEY_CHILDCOUNT:
      g_ptr_array_add (properties, "ChildCount");
      break;
    case GRL_METADATA_KEY_THUMBNAIL:
      g_ptr_array_add (properties, "AlbumArtURL");
      break;
    case GRL_METADATA_KEY_PUBLICATION_DATE:
      g_ptr_array_add (properties, "Date");
      break;
    default:
      GRL_DEBUG ("%s ignored non-supported key %s", G_STRFUNC, GRL_METADATA_KEY_GET_NAME (key_id));
    }
}

static ContainerType
get_container_type (const gchar *type_ex)
{
  ContainerType con_type = CONTAINER_TYPE_UNKNOWN;

  if (g_strcmp0 (type_ex, "container.person.musicArtist") == 0) {
    con_type = CONTAINER_TYPE_ARTIST;
  }
  if (g_strcmp0 (type_ex, "container.album.musicAlbum") == 0) {
    con_type = CONTAINER_TYPE_ALBUM;
  }
  if (g_strcmp0 (type_ex, "container.genre.musicGenre") == 0) {
    con_type = CONTAINER_TYPE_GENRE;
  }
  if (g_str_has_prefix (type_ex, "item")) {
    con_type = CONTAINER_TYPE_NOT_CONTAINER;
  }

  return con_type;
}

static void
media_set_property (GrlMedia *media,
                    const gchar *key,
                    GVariant *value,
                    ContainerType container_type)
{
  const gchar *s;
  gint i;

  if (g_strcmp0 (key, "Path") == 0) {
    s = g_variant_get_string (value, NULL);
    grl_dleyna_source_media_set_id_from_object_path (media, s);
  }
  else if (g_strcmp0 (key, "DisplayName") == 0) {
    s = g_variant_get_string (value, NULL);
    grl_media_set_title (media, s);
    switch (container_type) {
      case CONTAINER_TYPE_ALBUM:
        grl_media_set_album (media, s);
        break;
      case CONTAINER_TYPE_ARTIST:
        grl_media_set_artist (media, s);
        break;
      case CONTAINER_TYPE_GENRE:
        grl_media_set_genre (media, s);
        break;
      case CONTAINER_TYPE_NOT_CONTAINER:
        grl_media_set_title (media, s);
        break;
      default:
        grl_media_set_title (media, s);
        break;
    }
  }
  else if (g_strcmp0 (key, "URLs") == 0 && g_variant_n_children (value) > 0) {
    g_variant_get_child (value, 0, "&s", &s);
    grl_media_set_url (media, s);
  }
  else if (g_strcmp0 (key, "MIMEType") == 0) {
    s = g_variant_get_string (value, NULL);
    grl_media_set_mime (media, s);
  }
  else if (g_strcmp0 (key, "Duration") == 0) {
    i = g_variant_get_int32 (value);
    grl_media_set_duration (media, i);
  }
  else if (g_strcmp0 (key, "Author") == 0) {
    s = g_variant_get_string (value, NULL);
    grl_media_set_author (media, s);
  }
  else if (g_strcmp0 (key, "Artist") == 0) {
    s = g_variant_get_string (value, NULL);
    if (grl_media_is_audio (media) || grl_media_is_container (media)) {
      grl_media_set_artist (media, s);
    }
  }
  else if (g_strcmp0 (key, "Album") == 0) {
    s = g_variant_get_string (value, NULL);
    if (grl_media_is_audio (media) || grl_media_is_container (media)) {
      grl_media_set_album (media, s);
    }
  }
  else if (g_strcmp0 (key, "Genre") == 0) {
    s = g_variant_get_string (value, NULL);
    if (grl_media_is_audio (media) || grl_media_is_container (media)) {
      grl_media_set_genre (media, s);
    }
  }
  else if (g_strcmp0 (key, "TrackNumber") == 0) {
    i = g_variant_get_int32 (value);
    if (grl_media_is_audio (media)) {
      grl_media_set_track_number (media, i);
    }
  }
  else if (g_strcmp0 (key, "ChildCount") == 0) {
    guint32 count = g_variant_get_uint32 (value);
    if (grl_media_is_container (media)) {
      grl_media_set_childcount (media, count);
    }
  }
  else if (g_strcmp0 (key, "Width") == 0) {
    i = g_variant_get_int32 (value);
    if (grl_media_is_video (media)) {
      grl_media_set_width (media, i);
    }
    if (grl_media_is_image (media)) {
      grl_media_set_width (media, i);
    }
  }
  else if (g_strcmp0 (key, "Height") == 0) {
    i = g_variant_get_int32 (value);
    if (grl_media_is_video (media)) {
      grl_media_set_height (media, i);
    }
    if (grl_media_is_image (media)) {
      grl_media_set_height (media, i);
    }
  }
  else if (g_strcmp0 (key, "Bitrate") == 0) {
    i = g_variant_get_int32 (value);
    if (grl_media_is_audio (media)) {
      /* DLNA res@bitrate is in bits/second
       * (see guidelines at https://www.dlna.org/guidelines).
       * Convert to kbits/second.
       */
      grl_media_set_bitrate (media, i / 1000);
    }
  }
  else if (g_strcmp0 (key, "AlbumArtURL") == 0) {
    s = g_variant_get_string (value, NULL);
    grl_media_add_thumbnail (media, s);
  }
  else if (g_strcmp0 (key, "Date") == 0) {
    GDate date;
    s = g_variant_get_string (value, NULL);
    g_date_set_parse (&date, s);
    if (g_date_valid (&date)) {
      GDateTime *datetime;
      datetime = g_date_time_new_utc (date.year, date.month, date.day, 0, 0, 0);
      grl_media_set_publication_date (media, datetime);
      g_date_time_unref (datetime);
    }
  }
}

static GrlMedia *
populate_media_from_variant (GrlMedia *media,
                             GVariant *variant,
                             ContainerType container_type)
{
  GVariantIter iter;
  const gchar *key;
  GVariant *value;

  g_variant_iter_init (&iter, variant);
  while (g_variant_iter_next (&iter, "{&sv}", &key, &value)) {
    media_set_property (media, key, value, container_type);
    g_variant_unref (value);
  }

  return media;
}

static GrlMedia *
build_media_from_variant (GVariant *variant)
{
  GrlMedia *media;
  const gchar *type = NULL;
  const gchar *type_ex = NULL;

  g_variant_lookup (variant, "Type", "&s", &type);
  g_variant_lookup (variant, "TypeEx", "&s", &type_ex);

  ContainerType container_type = get_container_type (type_ex);

  if (type == NULL) {
    media = grl_media_new ();
  }
  else if (g_str_has_prefix (type, "container")) {
    media = grl_media_container_new ();
  }
  /* Workaround https://github.com/01org/dleyna-server/issues/101 */
  else if (g_str_has_prefix (type, "album") ||
           g_str_has_prefix (type, "person") ||
           g_str_has_prefix (type, "genre")) {
    media = grl_media_container_new ();
  }
  else if (g_str_has_prefix (type, "audio") ||
           g_str_has_prefix (type, "music")) {
    media = grl_media_audio_new ();
  }
  else if (g_str_has_prefix (type, "video")) {
    media = grl_media_video_new ();
  }
  else if (g_str_has_prefix (type, "image")) {
    media = grl_media_image_new ();
  }
  else {
    media = grl_media_new ();
  }

  populate_media_from_variant (media, variant, container_type);

  return media;
}

static gboolean
variant_set_property (GVariantBuilder *builder,
                      GrlMedia *media,
                      GrlKeyID key_id)
{
  gchar *s;

  switch (key_id) {
    case GRL_METADATA_KEY_TITLE:
      g_variant_builder_add_parsed (builder, "{'DisplayName', <%s>}",
                                    grl_media_get_title (media));
      return TRUE;
    case GRL_METADATA_KEY_ARTIST:
      if (grl_media_is_audio (media))
        g_variant_builder_add_parsed (builder, "{'Artist', <%s>}",
                                      grl_media_get_artist (media));
      return TRUE;
    case GRL_METADATA_KEY_ALBUM:
      if (grl_media_is_audio (media))
        g_variant_builder_add_parsed (builder, "{'Album', <%s>}",
                                      grl_media_get_album (media));
      return TRUE;
    case GRL_METADATA_KEY_GENRE:
      if (grl_media_is_audio (media))
        g_variant_builder_add_parsed (builder, "{'Genre', <%s>}",
                                      grl_media_get_genre (media));
      return TRUE;
    case GRL_METADATA_KEY_TRACK_NUMBER:
      if (grl_media_is_audio (media))
        g_variant_builder_add_parsed (builder, "{'TrackNumber', <%i>}",
                                      grl_media_get_track_number (media));
      return TRUE;
    case GRL_METADATA_KEY_AUTHOR:
      g_variant_builder_add_parsed (builder, "{'Creator', <%s>}",
                                    grl_media_get_author (media));
      return TRUE;
    case GRL_METADATA_KEY_PUBLICATION_DATE:
      s = g_date_time_format (grl_media_get_publication_date (media), "%F");
      g_variant_builder_add_parsed (builder, "{'Date', <%s>}", s);
      g_free (s);
      return TRUE;
    default:
      GRL_WARNING ("%s ignored non-writable key %s", G_STRFUNC, GRL_METADATA_KEY_GET_NAME (key_id));
      return FALSE;
    }
}

static GVariant *
build_variant_from_media (GrlMedia  *media,
                          GList     *keys,
                          GPtrArray *delete_keys)
{
  GVariantBuilder *builder;
  GList *iter;

  builder = g_variant_builder_new (G_VARIANT_TYPE_VARDICT);
  for (iter = keys; iter != NULL; iter = g_list_next (iter)) {
    GrlKeyID key = GRLPOINTER_TO_KEYID (iter->data);
    if (grl_data_has_key (GRL_DATA (media), key)) {
      variant_set_property (builder, media, key);
    }
    else {
      properties_add_for_key (delete_keys, key);
    }
  }

  return g_variant_builder_end (builder);
}

static void
grl_dleyna_source_results (GrlSource *source,
                           GError *error,
                           gint code,
                           GVariant *results,
                           guint operation_id,
                           GrlSourceResultCb callback,
                           gpointer user_data)
{
  GVariantIter iter;
  GVariant *item;
  gsize remaining;

  GRL_DEBUG (G_STRFUNC);

  if (error != NULL) {
    GRL_WARNING ("%s error:%s", G_STRFUNC, error->message);
    error = grl_dleyna_source_convert_error (error, code);
    callback (source, operation_id, NULL, 0, user_data, error);
    g_error_free (error);
    return;
  }

  remaining = g_variant_n_children (results);
  if (remaining == 0) {
    GRL_DEBUG ("%s no results", G_STRFUNC);
    callback (source, operation_id, NULL, 0, user_data, NULL);
    return;
  }

  g_variant_iter_init (&iter, results);
  while ((item = g_variant_iter_next_value (&iter))) {
    GrlMedia *media;
    media = build_media_from_variant (item);
    GRL_DEBUG ("%s %s", G_STRFUNC, grl_media_get_id (media));
    callback (source, operation_id, media, --remaining, user_data, NULL);
    g_variant_unref (item);
  }
}

static gchar const **
build_properties_filter (const GList *keys)
{
  GPtrArray *filter;

  filter = g_ptr_array_new ();
  g_ptr_array_add (filter, "Path"); /* always retrieve the items' DBus path */
  g_ptr_array_add (filter, "Type"); /* and their object type */
  g_ptr_array_add (filter, "TypeEx"); /* and their object extended type */
  while (keys != NULL) {
    properties_add_for_key (filter, GRLPOINTER_TO_KEYID (keys->data));
    keys = g_list_next (keys);
  }
  g_ptr_array_add (filter, NULL); /* nul-terminate the strvector */

  return (gchar const **) g_ptr_array_free (filter, FALSE);
}

static gchar *
build_type_filter_query (GrlTypeFilter type_filter)
{
  GString *filter;
  gboolean append_or = FALSE;

  if (type_filter == GRL_TYPE_FILTER_ALL) {
    return NULL;
  }

  filter = g_string_new ("( ");

  if (type_filter & GRL_TYPE_FILTER_AUDIO) {
    filter = g_string_append (filter, DLEYNA_TYPE_FILTER_AUDIO);
    append_or = TRUE;
  }

  if (type_filter & GRL_TYPE_FILTER_VIDEO) {
    if (append_or) {
      filter = g_string_append (filter, " or ");
    }
    filter = g_string_append (filter, DLEYNA_TYPE_FILTER_VIDEO);
    append_or = TRUE;
  }

  if (type_filter & GRL_TYPE_FILTER_IMAGE) {
    if (append_or) {
      filter = g_string_append (filter, " or ");
    }
    filter = g_string_append (filter, DLEYNA_TYPE_FILTER_IMAGE);
  }

  filter = g_string_append (filter, " )");

  return g_string_free (filter, FALSE);
}

static gchar *
build_search_query (GrlTypeFilter types,
                    const gchar *text)
{
  gchar *type_filter, *props_filter, *full_filter;

  type_filter = build_type_filter_query (types);

  if (text != NULL) {
    props_filter = g_strdup_printf (DLEYNA_SEARCH_SPEC, text, text, text);
  }
  else {
    props_filter = NULL;
  }

  if (text != NULL && type_filter != NULL) {
    full_filter = g_strdup_printf ("%s and %s", type_filter, props_filter);
  }
  else if (type_filter == NULL) {
    full_filter = g_strdup (props_filter);
  }
  else {
    full_filter = g_strdup ("*");
  }

  g_free (type_filter);
  g_free (props_filter);

  return full_filter;
}

static gchar *
build_browse_query (GrlTypeFilter types,
                    const gchar *container_id)
{
  gchar *type_filter, *container_filter, *full_filter;

  g_return_val_if_fail (container_id != NULL, NULL);

  type_filter = build_type_filter_query (types);
  container_filter = g_strdup_printf (DLEYNA_BROWSE_SPEC, container_id);

  if (type_filter != NULL) {
    full_filter = g_strdup_printf ("(%s or %s) and %s", DLEYNA_TYPE_FILTER_CONTAINER, type_filter, container_filter);
  }
  else {
    full_filter = g_strdup (container_filter);
  }

  g_free (type_filter);
  g_free (container_filter);

  return full_filter;
}

static void
grl_dleyna_source_resolve_browse_objects_cb (GObject      *object,
                                             GAsyncResult *res,
                                             gpointer      user_data)
{
  ContainerType container_type = CONTAINER_TYPE_UNKNOWN;
  GrlDleynaMediaDevice *device = GRL_DLEYNA_MEDIA_DEVICE (object);
  GrlSourceResolveSpec *rs = user_data;
  GVariant *results, *dict, *item_error;
  GError *error = NULL;

  GRL_DEBUG (G_STRFUNC);
  grl_dleyna_media_device_call_browse_objects_finish (device, &results, res, &error);

  if (error != NULL) {
    GRL_WARNING ("%s error:%s", G_STRFUNC, error->message);
    error = grl_dleyna_source_convert_error (error, GRL_CORE_ERROR_RESOLVE_FAILED);
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);
    g_error_free (error);
    return;
  }

  dict = g_variant_get_child_value (results, 0);

  /* Handle per-object errors */
  item_error = g_variant_lookup_value (dict, "Error", G_VARIANT_TYPE_VARDICT);
  if (item_error != NULL) {
    gint32 id = 0;
    gchar *message = NULL;

    g_variant_lookup (item_error, "ID", "i", &id);
    g_variant_lookup (item_error, "Message", "&s", &message);
    GRL_WARNING ("%s item error id:%d \"%s\"", G_STRFUNC, id, message);
    error = g_error_new (GRL_CORE_ERROR, GRL_CORE_ERROR_RESOLVE_FAILED,
                        _("Failed to retrieve item properties (BrowseObjects error %d: %s)"), id, message);
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);
    g_error_free (error);
    return;
  }

  populate_media_from_variant (rs->media, dict, container_type);
  rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
}

static void
grl_dleyna_source_search_search_objects_cb (GObject      *object,
                                            GAsyncResult *res,
                                            gpointer      user_data)
{
  GrlDleynaMediaContainer2 *container = GRL_DLEYNA_MEDIA_CONTAINER2 (object);
  GrlSourceSearchSpec *ss = user_data;
  GVariant *results;
  GError *error = NULL;

  GRL_DEBUG (G_STRFUNC);
  grl_dleyna_media_container2_call_search_objects_finish (container, &results, res, &error);
  grl_dleyna_source_results (ss->source, error, GRL_CORE_ERROR_SEARCH_FAILED, results, ss->operation_id, ss->callback, ss->user_data);
}

static void
grl_dleyna_source_query_search_objects_cb (GObject      *object,
                                           GAsyncResult *res,
                                           gpointer      user_data)
{
  GrlDleynaMediaContainer2 *container = GRL_DLEYNA_MEDIA_CONTAINER2 (object);
  GrlSourceQuerySpec *qs = user_data;
  GVariant *results;
  GError *error = NULL;

  GRL_DEBUG (G_STRFUNC);
  grl_dleyna_media_container2_call_search_objects_finish (container, &results, res, &error);
  grl_dleyna_source_results (qs->source, error, GRL_CORE_ERROR_QUERY_FAILED, results, qs->operation_id, qs->callback, qs->user_data);
}

static void
grl_dleyna_source_browse_list_children_cb (GObject      *object,
                                           GAsyncResult *res,
                                           gpointer      user_data)
{
  GrlDleynaMediaContainer2 *container = GRL_DLEYNA_MEDIA_CONTAINER2 (object);
  GrlSourceBrowseSpec *bs = user_data;
  GVariant *children;
  GError *error = NULL;

  GRL_DEBUG (G_STRFUNC);
  grl_dleyna_media_container2_call_list_children_finish (container, &children, res, &error);
  grl_dleyna_source_results (bs->source, error, GRL_CORE_ERROR_BROWSE_FAILED, children, bs->operation_id, bs->callback, bs->user_data);
}

static void
grl_dleyna_source_browse_search_objects_cb (GObject      *object,
                                            GAsyncResult *res,
                                            gpointer      user_data)
{
  GrlDleynaMediaContainer2 *container = GRL_DLEYNA_MEDIA_CONTAINER2 (object);
  GrlSourceBrowseSpec *bs = user_data;
  GVariant *children;
  GError *error = NULL;

  GRL_DEBUG (G_STRFUNC);
  grl_dleyna_media_container2_call_search_objects_finish (container, &children, res, &error);
  grl_dleyna_source_results (bs->source, error, GRL_CORE_ERROR_BROWSE_FAILED, children, bs->operation_id, bs->callback, bs->user_data);
}

static void
grl_dleyna_source_store_upload_wait_for_completion (GrlSourceStoreSpec *ss,
                                                    guint               upload_id,
                                                    gchar              *object_path,
                                                    GError             *error)
{
  GrlDleynaSource *self = GRL_DLEYNA_SOURCE (ss->source);

  GRL_DEBUG (G_STRFUNC);

  if (error != NULL) {
    GRL_WARNING ("%s error:%s", G_STRFUNC, error->message);
    error = grl_dleyna_source_convert_error (error, GRL_CORE_ERROR_STORE_FAILED);
    ss->callback (ss->source, ss->media, NULL, ss->user_data, error);
    g_error_free (error);
    return;
  }

  g_hash_table_insert (self->priv->uploads, GUINT_TO_POINTER (upload_id), ss);

  grl_dleyna_source_media_set_id_from_object_path (ss->media, object_path);
  g_free (object_path);
}

static void
grl_dleyna_source_store_create_container_in_any_container_cb (GObject      *object,
                                                              GAsyncResult *res,
                                                              gpointer      user_data)
{
  GrlDleynaMediaDevice *device = GRL_DLEYNA_MEDIA_DEVICE (object);
  GrlSourceStoreSpec *ss = user_data;
  gchar *object_path = NULL;
  GError *error = NULL;

  GRL_DEBUG (G_STRFUNC);

  grl_dleyna_media_device_call_create_container_in_any_container_finish (device, &object_path, res, &error);
  grl_dleyna_source_store_upload_completed (ss, object_path, error);
  g_free (object_path);
}

static void
grl_dleyna_source_store_upload_to_any_container_cb (GObject      *object,
                                                    GAsyncResult *res,
                                                    gpointer      user_data)
{
  GrlDleynaMediaDevice *device = GRL_DLEYNA_MEDIA_DEVICE (object);
  GrlSourceStoreSpec *ss = user_data;
  gchar *object_path = NULL;
  guint upload_id;
  GError *error = NULL;

  GRL_DEBUG (G_STRFUNC);

  grl_dleyna_media_device_call_upload_to_any_container_finish (device, &upload_id, &object_path, res, &error);
  grl_dleyna_source_store_upload_wait_for_completion (ss, upload_id, object_path, error);
}

static void
grl_dleyna_source_store_create_container_cb (GObject      *object,
                                             GAsyncResult *res,
                                             gpointer      user_data)
{
  GrlDleynaMediaContainer2 *container = GRL_DLEYNA_MEDIA_CONTAINER2 (object);
  GrlSourceStoreSpec *ss = user_data;
  gchar *object_path = NULL;
  GError *error = NULL;

  GRL_DEBUG (G_STRFUNC);

  grl_dleyna_media_container2_call_create_container_finish (container, &object_path, res, &error);
  grl_dleyna_source_store_upload_completed (ss, object_path, error);
  g_free (object_path);
}

static void
grl_dleyna_source_store_upload_cb (GObject      *object,
                                   GAsyncResult *res,
                                   gpointer      user_data)
{
  GrlDleynaMediaContainer2 *container = GRL_DLEYNA_MEDIA_CONTAINER2 (object);
  GrlSourceStoreSpec *ss = user_data;
  gchar *object_path = NULL;
  guint upload_id;
  GError *error = NULL;

  GRL_DEBUG (G_STRFUNC);

  grl_dleyna_media_container2_call_upload_finish (container, &upload_id, &object_path, res, &error);
  grl_dleyna_source_store_upload_wait_for_completion (ss, upload_id, object_path, error);
}

static void
grl_dleyna_source_store_metadata_update_cb (GObject      *obj,
                                            GAsyncResult *res,
                                            gpointer      user_data)
{
  GrlDleynaMediaObject2 *object = GRL_DLEYNA_MEDIA_OBJECT2 (obj);
  GrlSourceStoreMetadataSpec *sms = user_data;
  GList *failed_keys;
  const GList *w;
  GError *error = NULL;

  GRL_DEBUG ("%s", G_STRFUNC);

  grl_dleyna_media_object2_call_update_finish (object, res, &error);

  if (error != NULL) {
    GRL_WARNING ("%s error:%s", G_STRFUNC, error->message);
    error = grl_dleyna_source_convert_error (error, GRL_CORE_ERROR_STORE_FAILED);
    sms->callback (sms->source, sms->media, NULL, sms->user_data, error);
    g_error_free (error);
    return;
  }

  /* Drop from the set of keys to be stored the writable ones */
  failed_keys = g_list_copy (sms->keys);
  for (w = grl_dleyna_source_writable_keys (sms->source); w != NULL; w = g_list_next (w))
    failed_keys = g_list_remove (failed_keys, w->data);

  sms->callback (sms->source, sms->media, failed_keys, sms->user_data, NULL);
  g_list_free (failed_keys);
}

static void
grl_dleyna_source_remove_delete_cb (GObject      *obj,
                                    GAsyncResult *res,
                                    gpointer      user_data)
{
  GrlDleynaMediaObject2 *object = GRL_DLEYNA_MEDIA_OBJECT2 (obj);
  GrlSourceRemoveSpec *rs = user_data;
  GError *error = NULL;

  GRL_DEBUG ("%s", G_STRFUNC);

  grl_dleyna_media_object2_call_delete_finish (object, res, &error);
  if (error != NULL) {
    GRL_WARNING ("%s error:%s", G_STRFUNC, error->message);
    error = grl_dleyna_source_convert_error (error, GRL_CORE_ERROR_REMOVE_FAILED);
  }

  rs->callback (rs->source, rs->media, rs->user_data, error);
  g_clear_error (&error);
}

static void
grl_dleyna_source_changed_cb (GrlDleynaSource *self,
                              GVariant *changes,
                              gpointer user_data)
{
  GPtrArray *changed_medias = NULL;
  GVariantIter iter;
  GVariant *change, *next;
  guint32 change_type, next_change_type;
  GrlSourceChangeType grl_change_type;
  gboolean location_unknown;

  GRL_DEBUG (G_STRFUNC);

  g_variant_iter_init (&iter, changes);
  next = g_variant_iter_next_value (&iter);
  while (next != NULL) {
    GrlMedia *media;

    change = next;
    next = g_variant_iter_next_value (&iter);

    if (!g_variant_lookup (change, "ChangeType", "u", &change_type)) {
      GRL_WARNING ("Missing mandatory ChangeType property in the Changed(aa{sv}) DBus signal");
      continue;
    }

    /* make sure to flush the notification array if the next element
      * has no ChangeType property (which would be bug, as it is mandatory) */
    next_change_type = G_MAXUINT;
    if (next != NULL) {
      g_variant_lookup (next, "ChangeType", "u", &next_change_type);
    }

    switch ((DleynaChangeType)change_type) {
      case DLEYNA_CHANGE_TYPE_ADD:
        grl_change_type = GRL_CONTENT_ADDED;
        location_unknown = FALSE;
        break;
      case DLEYNA_CHANGE_TYPE_MOD:
        grl_change_type = GRL_CONTENT_CHANGED;
        location_unknown = FALSE;
        break;
      case DLEYNA_CHANGE_TYPE_DEL:
        grl_change_type = GRL_CONTENT_REMOVED;
        location_unknown = FALSE;
        break;
      case DLEYNA_CHANGE_TYPE_CONTAINER:
        grl_change_type = GRL_CONTENT_CHANGED;
        location_unknown = TRUE;
        break;
      case DLEYNA_CHANGE_TYPE_DONE:
        continue;
      default:
        GRL_WARNING ("%s ignore change type %d", G_STRFUNC, change_type);
        continue;
    }

    if (changed_medias == NULL) {
      changed_medias = g_ptr_array_new ();
    }

    media = build_media_from_variant (change);
    g_ptr_array_add (changed_medias, media);

    /* Flush the notifications when reaching the last element or when the
      * next one has a different change type */
    if (next == NULL || next_change_type != change_type) {
      grl_source_notify_change_list (GRL_SOURCE (self), changed_medias, grl_change_type, location_unknown);
      changed_medias = NULL;
    }
  }
}

/* ================== Internal functions ================== */

gchar *
grl_dleyna_source_build_id (const gchar *udn)
{
  return g_strdup_printf (SOURCE_ID_TEMPLATE, udn);
}

GrlDleynaSource *
grl_dleyna_source_new (GrlDleynaServer *server)
{
  GrlDleynaSource *source;
  GrlDleynaMediaDevice *device;
  const gchar *friendly_name, *udn, *icon_url, *location;
  gboolean localhost, localuser;
  gchar *id, *desc;
  gchar *tags[3];
  gint i;
  GIcon *icon = NULL;

  GRL_DEBUG (G_STRFUNC);

  device = grl_dleyna_server_get_media_device (server);

  friendly_name = grl_dleyna_media_device_get_friendly_name (device);
  udn = grl_dleyna_media_device_get_udn (device);
  id = grl_dleyna_source_build_id (udn);
  desc = g_strdup_printf (SOURCE_DESC_TEMPLATE, friendly_name);

  icon_url = grl_dleyna_media_device_get_icon_url (device);
  if (icon_url != NULL) {
    GFile *file;
    file = g_file_new_for_uri (icon_url);
    icon = g_file_icon_new (file);
    g_object_unref (file);
  }

  location = grl_dleyna_media_device_get_location (device);
  grl_dleyna_util_uri_is_localhost (location, &localuser, &localhost);
  i = 0;
  if (localhost) {
    tags[i++] = "localhost";
  }
  if (localuser) {
    tags[i++] = "localuser";
  }
  tags[i++] = NULL;

  source = g_object_new (GRL_DLEYNA_SOURCE_TYPE,
                         "server", server,
                         "source-id", id,
                         "source-name", friendly_name,
                         "source-desc", desc,
                         "source-icon", icon,
                         "source-tags", tags[0]? tags: NULL,
                         NULL);
  g_free (id);
  g_free (desc);

  return source;
}

/* ================== Grilo API implementation ================== */

static GrlCaps *
grl_dleyna_source_get_caps (GrlSource *source,
                            GrlSupportedOps operation)
{
  static GrlCaps *caps = NULL;
  static GrlCaps *caps_browse = NULL;

  if (caps == NULL) {
    caps = grl_caps_new ();
    if (GRL_DLEYNA_SOURCE (source)->priv->search_enabled) {
      grl_caps_set_type_filter (caps, GRL_TYPE_FILTER_ALL);
    }
  }

  if (caps_browse == NULL) {
    caps_browse = grl_caps_new ();
    if (GRL_DLEYNA_SOURCE (source)->priv->browse_filtered_enabled) {
      grl_caps_set_type_filter (caps_browse, GRL_TYPE_FILTER_ALL);
    }
  }

  return (operation == GRL_OP_BROWSE) ? caps_browse: caps;
}

static const GList *
grl_dleyna_source_supported_keys (GrlSource *source)
{
  static GList *keys = NULL;

  if (keys == NULL) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_URL,
                                      GRL_METADATA_KEY_MIME,
                                      GRL_METADATA_KEY_DURATION,
                                      GRL_METADATA_KEY_ARTIST,
                                      GRL_METADATA_KEY_ALBUM,
                                      GRL_METADATA_KEY_GENRE,
                                      GRL_METADATA_KEY_CHILDCOUNT,
                                      GRL_METADATA_KEY_THUMBNAIL,
                                      GRL_METADATA_KEY_TRACK_NUMBER,
                                      GRL_METADATA_KEY_AUTHOR,
                                      GRL_METADATA_KEY_WIDTH,
                                      GRL_METADATA_KEY_HEIGHT,
                                      GRL_METADATA_KEY_BITRATE,
                                      GRL_METADATA_KEY_PUBLICATION_DATE,
                                      NULL);
  }

  return keys;
}

static const GList *
grl_dleyna_source_writable_keys (GrlSource *source)
{
  static GList *keys = NULL;

  if (keys == NULL) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_ARTIST,
                                      GRL_METADATA_KEY_ALBUM,
                                      GRL_METADATA_KEY_GENRE,
                                      GRL_METADATA_KEY_TRACK_NUMBER,
                                      GRL_METADATA_KEY_AUTHOR,
                                      GRL_METADATA_KEY_PUBLICATION_DATE,
                                      NULL);
  }

  return keys;
}

static GrlSupportedOps
grl_dleyna_source_supported_operations (GrlSource *source)
{
  GrlDleynaSource *self = GRL_DLEYNA_SOURCE (source);
  GrlSupportedOps caps;

  caps = GRL_OP_BROWSE | GRL_OP_RESOLVE | GRL_OP_STORE | GRL_OP_STORE_PARENT |
         GRL_OP_STORE_METADATA | GRL_OP_REMOVE | GRL_OP_NOTIFY_CHANGE;
  if (self->priv->search_enabled) {
    caps = caps | GRL_OP_SEARCH | GRL_OP_QUERY;
  }

  return caps;
}

static void
grl_dleyna_source_resolve (GrlSource *source,
                           GrlSourceResolveSpec *rs)
{
  GrlDleynaSource *self = GRL_DLEYNA_SOURCE (source);
  GrlDleynaMediaDevice *device;
  GCancellable *cancellable;
  GPtrArray *filter;
  GList *iter;
  gchar const *media_id;
  gchar const *object_path;
  gchar const *object_paths[] = { NULL, NULL };

  GRL_DEBUG (G_STRFUNC);

  media_id = grl_media_get_id (rs->media);

  /* assume root container if no id has been specified */
  if (media_id == NULL) {
    GrlDleynaMediaContainer2 *root;
    root = grl_dleyna_server_get_media_container (self->priv->server);
    object_path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (root));
    grl_dleyna_source_media_set_id_from_object_path (rs->media, object_path);
  }

  device = grl_dleyna_server_get_media_device (self->priv->server);
  object_path = grl_dleyna_source_media_get_object_path (rs->media);
  object_paths[0] = object_path;

  /* discard media from different servers */
  if (!g_str_has_prefix (object_path, grl_dleyna_server_get_object_path (self->priv->server))) {
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
    return;
  }

  cancellable = g_cancellable_new ();
  grl_operation_set_data_full (rs->operation_id, cancellable, g_object_unref);

  filter = g_ptr_array_new ();
  for (iter = rs->keys; iter != NULL; iter = g_list_next (iter)) {
    properties_add_for_key (filter, GRLPOINTER_TO_KEYID (iter->data));
  }
  g_ptr_array_add (filter, NULL); /* nul-terminate the strvector */

  grl_dleyna_media_device_call_browse_objects (device, object_paths,
                                               (const gchar * const*)filter->pdata, cancellable,
                                               grl_dleyna_source_resolve_browse_objects_cb, rs);
  g_ptr_array_unref (filter);
}

static void
grl_dleyna_source_browse (GrlSource *source,
                          GrlSourceBrowseSpec *bs)
{
  GrlDleynaSource *self = GRL_DLEYNA_SOURCE (source);
  GrlDleynaMediaContainer2 *container, *root;
  GDBusConnection *connection;
  GCancellable *cancellable;
  GrlTypeFilter type_filter;
  gchar const *object_path;
  gchar const **filter;
  guint offset, limit;
  GError *error = NULL;

  GRL_DEBUG (G_STRFUNC);

  cancellable = g_cancellable_new ();
  grl_operation_set_data_full (bs->operation_id, cancellable, g_object_unref);

  root = grl_dleyna_server_get_media_container (self->priv->server);
  connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (root));
  filter = build_properties_filter (bs->keys);
  offset = grl_operation_options_get_skip (bs->options);
  /* Grilo uses -1 to say "no limit" while dLeyna expect 0 for the same purpose */
  limit = MAX (0, grl_operation_options_get_count (bs->options));

  object_path = grl_dleyna_source_media_get_object_path (bs->container);
  if (object_path == NULL) {
    object_path = g_dbus_proxy_get_object_path (G_DBUS_PROXY (root));
  }

  /* This does not block as we don't load properties nor connect to signals*/
  container = grl_dleyna_media_container2_proxy_new_sync (connection,
                                                          G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                                          G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                                          DLEYNA_DBUS_NAME, object_path, cancellable, &error);
  if (error != NULL) {
    grl_dleyna_source_results (bs->source, error, GRL_CORE_ERROR_BROWSE_FAILED, NULL, bs->operation_id, bs->callback, bs->user_data);
    goto out;
  }

  /* invoke SearchObjects instead of ListChildren if we need to filter by type */
  type_filter = grl_operation_options_get_type_filter (bs->options);
  if (type_filter != GRL_TYPE_FILTER_ALL) {
    gchar *query;

    query = build_browse_query (type_filter, object_path);
    GRL_DEBUG ("%s browse:%s", G_STRFUNC, query);
    grl_dleyna_media_container2_call_search_objects (container, query, offset, limit, filter, cancellable,
                                                     grl_dleyna_source_browse_search_objects_cb, bs);
    g_free (query);
  }
  else {
    grl_dleyna_media_container2_call_list_children (container, offset, limit, filter, cancellable,
                                                    grl_dleyna_source_browse_list_children_cb, bs);
  }

out:
  g_object_unref (container);
  g_free (filter);
}

static void
grl_dleyna_source_search (GrlSource *source,
                          GrlSourceSearchSpec *ss)
{
  GrlDleynaSource *self = GRL_DLEYNA_SOURCE (source);
  GrlDleynaMediaContainer2 *root;
  GCancellable *cancellable;
  gchar const **filter;
  gchar *query;
  guint skip, count;

  GRL_DEBUG (G_STRFUNC);

  cancellable = g_cancellable_new ();
  grl_operation_set_data_full (ss->operation_id, cancellable, g_object_unref);

  skip = grl_operation_options_get_skip (ss->options);
  /* Grilo uses -1 to say "no limit" while dLeyna expect 0 for the same purpose */
  count = MAX (0, grl_operation_options_get_count (ss->options));
  filter = build_properties_filter (ss->keys);
  query = build_search_query (grl_operation_options_get_type_filter (ss->options), ss->text);

  GRL_DEBUG ("%s query:'%s'", G_STRFUNC, query);
  root = grl_dleyna_server_get_media_container (self->priv->server);
  grl_dleyna_media_container2_call_search_objects (root, query, skip, count, filter,
                                                   cancellable, grl_dleyna_source_search_search_objects_cb, ss);
  g_free (filter);
  g_free (query);
}

/*
 * Query format is the org.gnome.UPnP.MediaContainer2.SearchObjects search
 * criteria format, e.g.
 * 'Artist contains "Rick Astley" and
 *  (Type derivedfrom "audio") or (Type derivedfrom "music")'
 *
 * Note that we don't guarantee or check that the server actually
 * supports the given criteria. Offering the searchcaps as
 * additional metadata to clients that _really_ are interested might
 * be useful.
 */
static void
grl_dleyna_source_query (GrlSource *source,
                         GrlSourceQuerySpec *qs)
{
  GrlDleynaSource *self = GRL_DLEYNA_SOURCE (source);
  GrlDleynaMediaContainer2 *root;
  GCancellable *cancellable;
  gchar const **filter;
  guint skip, count;

  GRL_DEBUG (G_STRFUNC);

  cancellable = g_cancellable_new ();
  grl_operation_set_data_full (qs->operation_id, cancellable, g_object_unref);

  skip = grl_operation_options_get_skip (qs->options);
  /* Grilo uses -1 to say "no limit" while dLeyna expect 0 for the same purpose */
  count = MAX (0, grl_operation_options_get_count (qs->options));
  filter = build_properties_filter (qs->keys);
  root = grl_dleyna_server_get_media_container (self->priv->server);
  grl_dleyna_media_container2_call_search_objects (root, qs->query, skip, count, filter,
                                                   cancellable, grl_dleyna_source_query_search_objects_cb, qs);
  g_free (filter);
}

static void
grl_dleyna_source_store (GrlSource *source,
                         GrlSourceStoreSpec *ss)
{
  GrlDleynaSource *self = GRL_DLEYNA_SOURCE (source);
  GrlDleynaMediaContainer2 *container;
  GrlDleynaMediaDevice *device;
  const gchar *container_object_path;
  const gchar *url;
  const gchar * const child_types[] = { "*", NULL };
  gchar *title = NULL;
  gchar *filename = NULL;
  GError *error = NULL;

  GRL_DEBUG (G_STRFUNC);

  title = g_strdup (grl_media_get_title (ss->media));

  if (!grl_media_is_container (ss->media)) {
    url = grl_media_get_url (ss->media);
    if (url == NULL) {
      error = g_error_new (GRL_CORE_ERROR, GRL_CORE_ERROR_STORE_FAILED,
                          _("Upload failed, URL missing on the media object to be transferred"));
      GRL_WARNING ("%s error:%s", G_STRFUNC, error->message);
      ss->callback (ss->source, ss->media, NULL, ss->user_data, error);
      goto out;
    }

    filename = g_filename_from_uri (url, NULL, &error);
    if (error != NULL) {
      GRL_WARNING ("%s error:%s", G_STRFUNC, error->message);
      error = grl_dleyna_source_convert_error (error, GRL_CORE_ERROR_STORE_FAILED);
      ss->callback (ss->source, ss->media, NULL, ss->user_data, error);
      goto out;
    }

    if (title == NULL) {
      title = g_path_get_basename (filename);
    }
  }

  device = grl_dleyna_server_get_media_device (self->priv->server);
  container_object_path = grl_dleyna_source_media_get_object_path (GRL_MEDIA (ss->parent));
  if (container_object_path == NULL) {
    /* If no container has been explicitly requested, let the DMS choose the
      * appropriate storage location automatically */
    if (grl_media_is_container (ss->media)) {
      grl_dleyna_media_device_call_create_container_in_any_container (device, title, "container", child_types, NULL,
                                                                      grl_dleyna_source_store_create_container_in_any_container_cb, ss);
    }
    else {
      grl_dleyna_media_device_call_upload_to_any_container (device, title, filename, NULL,
                                                            grl_dleyna_source_store_upload_to_any_container_cb, ss);
    }
  }
  else {
    GDBusConnection *connection;

    connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (device));
    /* This does not block as we don't load properties nor connect to signals*/
    container = grl_dleyna_media_container2_proxy_new_sync (connection,
                                                            G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                                            G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                                            DLEYNA_DBUS_NAME, container_object_path, NULL,
                                                            &error);
    if (error != NULL) {
      GRL_WARNING ("%s error:%s", G_STRFUNC, error->message);
      error = grl_dleyna_source_convert_error (error, GRL_CORE_ERROR_STORE_FAILED);
      ss->callback (ss->source, ss->media, NULL, ss->user_data, error);
      goto out;
    }

    if (grl_media_is_container (ss->media)) {
      grl_dleyna_media_container2_call_create_container (container, title, "container", child_types, NULL,
                                                         grl_dleyna_source_store_create_container_cb, ss);
    }
    else {
      grl_dleyna_media_container2_call_upload (container, title, filename, NULL,
                                               grl_dleyna_source_store_upload_cb, ss);
    }
    g_object_unref (container);
  }

out:
  g_clear_error (&error);
  g_free (title);
  g_free (filename);
}

static void
grl_dleyna_source_store_metadata (GrlSource *source,
                                  GrlSourceStoreMetadataSpec *sms)
{
  GrlDleynaSource *self = GRL_DLEYNA_SOURCE (source);
  GrlDleynaMediaDevice *device;
  GrlDleynaMediaObject2 *object;
  GDBusConnection *connection;
  GVariant *metadata;
  GPtrArray *delete_keys;
  const gchar *object_path;
  GError *error = NULL;

  GRL_DEBUG ("%s", G_STRFUNC);

  device = grl_dleyna_server_get_media_device (self->priv->server);
  connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (device));
  object_path = grl_dleyna_source_media_get_object_path (sms->media);

  /* This does not block as we don't load properties nor connect to signals*/
  object = grl_dleyna_media_object2_proxy_new_sync (connection,
                                                    G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                                    G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                                    DLEYNA_DBUS_NAME, object_path, NULL, &error);
  if (error != NULL) {
    GRL_WARNING ("%s error:%s", G_STRFUNC, error->message);
    error = grl_dleyna_source_convert_error (error, GRL_CORE_ERROR_STORE_METADATA_FAILED);
    sms->callback (sms->source, sms->media, NULL, sms->user_data, error);
    goto out;
  }

  delete_keys = g_ptr_array_new_with_free_func (g_free);
  metadata = build_variant_from_media (sms->media, sms->keys, delete_keys);
  g_ptr_array_add (delete_keys, NULL); /* Make sure it is NULL-terminated */
  grl_dleyna_media_object2_call_update (object, metadata, (const gchar * const *) delete_keys->pdata,
                                        NULL, grl_dleyna_source_store_metadata_update_cb, sms);
  g_ptr_array_unref (delete_keys);

out:
  g_clear_error (&error);
  g_object_unref (object);
}

static void
grl_dleyna_source_remove (GrlSource *source,
                          GrlSourceRemoveSpec *rs)
{
  GrlDleynaSource *self = GRL_DLEYNA_SOURCE (source);
  GrlDleynaMediaDevice *device;
  GrlDleynaMediaObject2 *object;
  GDBusConnection *connection;
  const gchar *object_path;
  GError *error = NULL;

  GRL_DEBUG ("%s", G_STRFUNC);

  device = grl_dleyna_server_get_media_device (self->priv->server);
  connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (device));
  object_path = grl_dleyna_source_media_get_object_path_from_id (rs->media_id);

  /* This does not block as we don't load properties nor connect to signals*/
  object = grl_dleyna_media_object2_proxy_new_sync (connection,
                                                    G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                                    G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                                    DLEYNA_DBUS_NAME, object_path, NULL, &error);
  if (error != NULL) {
    GRL_WARNING ("%s error:%s", G_STRFUNC, error->message);
    error = grl_dleyna_source_convert_error (error, GRL_CORE_ERROR_REMOVE_FAILED);
    rs->callback (rs->source, rs->media, rs->user_data, error);
    g_error_free (error);
    return;
  }

  grl_dleyna_media_object2_call_delete (object, NULL, grl_dleyna_source_remove_delete_cb, rs);
  g_object_unref (object);
}

static void
grl_dleyna_source_cancel (GrlSource *source,
                          guint operation_id)
{
  GCancellable *cancellable;

  GRL_DEBUG ( G_STRFUNC);

  cancellable = grl_operation_get_data (operation_id);
  if (cancellable != NULL) {
    g_cancellable_cancel (cancellable);
  }
}

static gboolean
grl_dleyna_source_notify_change_start (GrlSource *source,
                                       GError **error)
{
  GrlDleynaSource *self = GRL_DLEYNA_SOURCE (source);
  GrlDleynaMediaDevice *device;

  GRL_DEBUG (G_STRFUNC);
  device = grl_dleyna_server_get_media_device (self->priv->server);
  g_signal_connect_object (device, "changed", G_CALLBACK (grl_dleyna_source_changed_cb),
                           self, G_CONNECT_SWAPPED);

  return TRUE;
}

static gboolean
grl_dleyna_source_notify_change_stop (GrlSource *source,
                                      GError **error)
{
  GrlDleynaSource *self = GRL_DLEYNA_SOURCE (source);
  GrlDleynaMediaDevice *device;

  GRL_DEBUG (G_STRFUNC);
  device = grl_dleyna_server_get_media_device (self->priv->server);
  g_signal_handlers_disconnect_by_func (device, grl_dleyna_source_changed_cb, self);

  return TRUE;
}
