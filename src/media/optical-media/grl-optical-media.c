/*
 * Copyright (C) 2012 Bastien Nocera
 *
 * Contact: Bastien Nocera <hadess@hadess.net>
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
#include <gio/gio.h>
#include <totem-pl-parser.h>
#include <string.h>
#include <stdlib.h>

#include "grl-optical-media.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT optical_media_log_domain
GRL_LOG_DOMAIN_STATIC(optical_media_log_domain);

/* --- Plugin information --- */

#define PLUGIN_ID   OPTICAL_MEDIA_PLUGIN_ID

#define SOURCE_ID   "grl-optical-media"
#define SOURCE_NAME "Optical Media"
#define SOURCE_DESC "A source for browsing optical media"

/* --- Grilo OpticalMedia Private --- */

#define GRL_OPTICAL_MEDIA_SOURCE_GET_PRIVATE(object)           \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                       \
                               GRL_OPTICAL_MEDIA_SOURCE_TYPE,  \
                               GrlOpticalMediaSourcePrivate))

#define NUM_MONITOR_SIGNALS 4

struct _GrlOpticalMediaSourcePrivate {
  GVolumeMonitor *monitor;
  guint monitor_signal_ids[NUM_MONITOR_SIGNALS];
};

/* --- Data types --- */

static GrlOpticalMediaSource *grl_optical_media_source_new (void);

static void grl_optical_media_source_finalize (GObject *object);

gboolean grl_optical_media_plugin_init (GrlPluginRegistry *registry,
                                        const GrlPluginInfo *plugin,
                                        GList *configs);

static const GList *grl_optical_media_source_supported_keys (GrlMetadataSource *source);

static void grl_optical_media_source_browse (GrlMediaSource *source,
                                             GrlMediaSourceBrowseSpec *bs);

static void grl_optical_media_source_cancel (GrlMetadataSource *source,
                                             guint operation_id);

static void
on_g_volume_monitor_event (GVolumeMonitor *monitor,
                           gpointer device,
                           GrlOpticalMediaSource *source);

/* =================== OpticalMedia Plugin  =============== */

gboolean
grl_optical_media_plugin_init (GrlPluginRegistry *registry,
                               const GrlPluginInfo *plugin,
                               GList *configs)
{
  GRL_LOG_DOMAIN_INIT (optical_media_log_domain, "optical_media");

  GRL_DEBUG ("%s", __FUNCTION__);

  GrlOpticalMediaSource *source = grl_optical_media_source_new ();

  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source),
                                       NULL);

  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_optical_media_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== OpticalMedia GObject ================ */


G_DEFINE_TYPE (GrlOpticalMediaSource,
               grl_optical_media_source,
               GRL_TYPE_MEDIA_SOURCE);

static GrlOpticalMediaSource *
grl_optical_media_source_new (void)
{
  GRL_DEBUG ("%s", __FUNCTION__);

  return g_object_new (GRL_OPTICAL_MEDIA_SOURCE_TYPE,
                       "source-id", SOURCE_ID,
                       "source-name", SOURCE_NAME,
                       "source-desc", SOURCE_DESC,
                       NULL);
}

static void
grl_optical_media_source_class_init (GrlOpticalMediaSourceClass * klass)
{
  GrlMediaSourceClass *source_class = GRL_MEDIA_SOURCE_CLASS (klass);
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = grl_optical_media_source_finalize;

  source_class->browse = grl_optical_media_source_browse;

  metadata_class->supported_keys = grl_optical_media_source_supported_keys;
  metadata_class->cancel = grl_optical_media_source_cancel;

  g_type_class_add_private (klass, sizeof (GrlOpticalMediaSourcePrivate));
}

static void
grl_optical_media_source_init (GrlOpticalMediaSource *source)
{
  const char * monitor_signals[NUM_MONITOR_SIGNALS] = {
    "volume-added",
    "volume-removed",
    "mount-added",
    "mount-removed",
  };
  guint i;

  source->priv = GRL_OPTICAL_MEDIA_SOURCE_GET_PRIVATE (source);

  source->priv->monitor = g_volume_monitor_get ();

  for (i = 0; i < G_N_ELEMENTS (monitor_signals); i++) {
    source->priv->monitor_signal_ids[i] = g_signal_connect (G_OBJECT (source->priv->monitor),
                                                            monitor_signals[i],
                                                            G_CALLBACK (on_g_volume_monitor_event), source);
  }
}

static void
grl_optical_media_source_finalize (GObject *object)
{
  GrlOpticalMediaSource *source = GRL_OPTICAL_MEDIA_SOURCE (object);
  guint i;

  for (i = 0; i < NUM_MONITOR_SIGNALS; i++) {
    g_signal_handler_disconnect (G_OBJECT (source->priv->monitor),
                                 source->priv->monitor_signal_ids[i]);
  }

  g_object_unref (source->priv->monitor);
  source->priv->monitor = NULL;

  G_OBJECT_CLASS (grl_optical_media_source_parent_class)->finalize (object);
}

/* ======================= Utilities ==================== */

static void
on_g_volume_monitor_event (GVolumeMonitor *monitor,
                           gpointer device,
                           GrlOpticalMediaSource *source)
{
  grl_media_source_notify_change (GRL_MEDIA_SOURCE (source), NULL, GRL_CONTENT_CHANGED, TRUE);
}

/* ================== API Implementation ================ */

static const GList *
grl_optical_media_source_supported_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_URL,
                                      GRL_METADATA_KEY_MIME,
                                      NULL);
  }
  return keys;
}

static char *
get_uri_for_gicon (GIcon *icon)
{
  char *uri;

  uri = NULL;

  if (G_IS_EMBLEMED_ICON (icon) != FALSE) {
    GIcon *new_icon;
    new_icon = g_emblemed_icon_get_icon (G_EMBLEMED_ICON (icon));
    g_object_unref (icon);
    icon = g_object_ref (new_icon);
  }

  if (G_IS_FILE_ICON (icon) != FALSE) {
    GFile *file;

    file = g_file_icon_get_file (G_FILE_ICON (icon));
    uri = g_file_get_uri (file);
    g_object_unref (file);

    return uri;
  }

  /* We leave the themed icons up to the applications to set */

  return uri;
}

static GList *
add_mount (GList *media_list,
           GMount *mount,
           GrlOpticalMediaSource *source)
{
  char *name, *icon_uri;
  GIcon *icon;
  GrlMedia *media;
  char *id;

  GVolume *volume;
  GFile *root;

  /* Check whether we have an archive mount */
  volume = g_mount_get_volume (mount);
  if (volume != NULL) {
    g_object_unref (volume);
    return media_list;
  }

  root = g_mount_get_root (mount);
  if (g_file_has_uri_scheme (root, "archive") == FALSE) {
    g_object_unref (root);
    return media_list;
  }

  media = grl_media_video_new ();

  id = g_file_get_uri (root);
  grl_media_set_id (media, id);
  g_free (id);

  /* Work out an icon to display */
  icon = g_mount_get_icon (mount);
  icon_uri = get_uri_for_gicon (icon);
  g_object_unref (icon);
  grl_media_set_thumbnail (media, icon_uri);
  g_free (icon_uri);

  /* Get the mount's pretty name for the menu label */
  name = g_mount_get_name (mount);
  g_strstrip (name);
  grl_media_set_title (media, name);
  g_free (name);

  grl_media_set_mime (media, "x-special/device-block");

  return g_list_prepend (media_list, media);
}

static GList *
add_volume (GList *media_list,
            GVolume *volume,
            GDrive *drive,
            GrlOpticalMediaSource *source)
{
  char *name, *icon_uri;
  GIcon *icon;
  char *device_path, *id;
  GrlMedia * media;
  GMount *mount;

  device_path = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
  if (device_path == NULL)
    return media_list;

  /* Is it an audio CD or a blank media */
  mount = g_volume_get_mount (volume);
  if (mount != NULL) {
    GFile *root;

    root = g_mount_get_root (mount);
    g_object_unref (mount);

    if (g_file_has_uri_scheme (root, "burn") != FALSE || g_file_has_uri_scheme (root, "cdda") != FALSE) {
      /* We don't add Audio CDs, or blank media */
      g_object_unref (root);
      g_free (device_path);
      return media_list;
    }
    g_object_unref (root);
  }

  media = grl_media_video_new ();

  id = g_filename_to_uri (device_path, NULL, NULL);
  g_free (device_path);

  grl_media_set_id (media, id);
  g_free (id);

  /* Work out an icon to display */
  icon = g_volume_get_icon (volume);
  icon_uri = get_uri_for_gicon (icon);
  g_object_unref (icon);
  grl_media_set_thumbnail (media, icon_uri);
  g_free (icon_uri);

  /* Get the volume's pretty name for the menu label */
  name = g_volume_get_name (volume);
  g_strstrip (name);
  grl_media_set_title (media, name);
  g_free (name);

  grl_media_set_mime (media, "x-special/device-block");

  return g_list_prepend (media_list, media);
}

static GList *
add_drive (GList *media_list,
           GDrive *drive,
           GrlOpticalMediaSource *source)
{
  GList *volumes, *i;

  if (g_drive_can_eject (drive) == FALSE ||
      g_drive_has_media (drive) == FALSE) {
    return media_list;
  }

  /* Repeat for all the drive's volumes */
  volumes = g_drive_get_volumes (drive);

  for (i = volumes; i != NULL; i = i->next) {
    GVolume *volume = i->data;
    media_list = add_volume (media_list, volume, drive, source);
    g_object_unref (volume);
  }

  g_list_free (volumes);

  return media_list;
}

typedef struct {
  TotemPlParser *parser;
  GCancellable *cancellable;
  GrlMediaSource *source;
  GrlMediaSourceBrowseSpec *bs;
  GList *media_list;
  GrlMedia *media;
} BrowseData;

static void resolve_disc_urls (BrowseData *data);

static void
parsed_finished (TotemPlParser *pl, GAsyncResult *result, BrowseData *data)
{
  TotemPlParserResult retval;
  GError *error = NULL;

  retval = totem_pl_parser_parse_finish (TOTEM_PL_PARSER (pl), result, &error);

  /* Do the fallback ourselves */
  if (retval == TOTEM_PL_PARSER_RESULT_IGNORED) {
    grl_media_set_url (data->media, grl_media_get_id (data->media));
    retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
  }

  if (retval == TOTEM_PL_PARSER_RESULT_SUCCESS &&
      grl_media_get_url (data->media) != NULL) {
    data->bs->callback (data->bs->source,
                        data->bs->browse_id,
                        data->media,
                        -1,
                        data->bs->user_data,
                        NULL);
  } else {
    if (retval == TOTEM_PL_PARSER_RESULT_ERROR) {
      GRL_WARNING ("Failed to parse '%s': %s",
                   grl_media_get_id (data->media),
                   error->message);
      g_error_free (error);
    }
    g_object_unref (data->media);
  }
  data->media = NULL;

  resolve_disc_urls (data);
}

static void
entry_parsed_cb (TotemPlParser *parser,
                 const char    *uri,
                 GHashTable    *metadata,
                 BrowseData    *data)
{
  g_return_if_fail (data->media != NULL);
  if (grl_media_get_url (data->media) != NULL) {
    GRL_WARNING ("Was going to set media '%s' to URL '%s' but already has URL '%s'",
                 grl_media_get_id (data->media),
                 uri,
                 grl_media_get_url (data->media));
    return;
  }

  grl_media_set_url (data->media, uri);
}

static void
resolve_disc_urls (BrowseData *data)
{
  g_assert (data->media == NULL);

  if (data->media_list == NULL ||
      g_cancellable_is_cancelled (data->cancellable)) {
    /* If we got cancelled, there's still some media
     * to resolve here */
    if (data->media_list) {
      g_list_free_full (data->media_list, g_object_unref);
    }
    /* No media left, we're done */
    data->bs->callback (data->bs->source,
                        data->bs->browse_id,
                        NULL,
                        0,
                        data->bs->user_data,
                        NULL);
    g_object_unref (data->cancellable);
    g_object_unref (data->parser);
    g_free (data);
    return;
  }

  data->media = data->media_list->data;
  data->media_list = g_list_delete_link (data->media_list, data->media_list);

  totem_pl_parser_parse_async (data->parser,
                               grl_media_get_id (data->media),
                               FALSE,
                               data->cancellable,
                               (GAsyncReadyCallback) parsed_finished,
                               data);
}

static void
grl_optical_media_source_browse (GrlMediaSource *source,
                                 GrlMediaSourceBrowseSpec *bs)
{
  GList *drives;
  GList *mounts;
  GList *l;
  GrlOpticalMediaSourcePrivate *priv = GRL_OPTICAL_MEDIA_SOURCE (source)->priv;
  BrowseData *data;
  GList *media_list;

  GRL_DEBUG ("%s", __FUNCTION__);

  media_list = NULL;

  /* Get the drives */
  drives = g_volume_monitor_get_connected_drives (priv->monitor);
  for (l = drives; l != NULL; l = l->next) {
    GDrive *drive = l->data;

    media_list = add_drive (media_list, drive, GRL_OPTICAL_MEDIA_SOURCE (source));
    g_object_unref (drive);
  }
  g_list_free (drives);

  /* Look for mounted archives */
  mounts = g_volume_monitor_get_mounts (priv->monitor);
  for (l = mounts; l != NULL; l = l->next) {
    GMount *mount = l->data;

    media_list = add_mount (media_list, mount, GRL_OPTICAL_MEDIA_SOURCE (source));
    g_object_unref (mount);
  }
  g_list_free (mounts);

  /* Got nothing? */
  if (media_list == NULL) {
    /* Tell the caller we're done */
    bs->callback (bs->source,
                  bs->browse_id,
                  NULL,
                  0,
                  bs->user_data,
                  NULL);
    return;
  }

  media_list = g_list_reverse (media_list);

  /* And go to resolve all those devices */
  data = g_new0 (BrowseData, 1);
  data->source = source;
  data->bs = bs;
  data->media_list = media_list;
  data->cancellable = g_cancellable_new ();

  grl_operation_set_data (bs->browse_id, data->cancellable);

  data->parser = totem_pl_parser_new ();
  g_signal_connect (G_OBJECT (data->parser), "entry-parsed",
                    G_CALLBACK (entry_parsed_cb), data);

  resolve_disc_urls (data);
}

static void
grl_optical_media_source_cancel (GrlMetadataSource *source, guint operation_id)
{
  GCancellable *cancellable;

  cancellable = G_CANCELLABLE (grl_operation_get_data (operation_id));

  if (cancellable) {
    g_cancellable_cancel (cancellable);
  }
}
