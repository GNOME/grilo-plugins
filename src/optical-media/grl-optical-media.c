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
#include <glib/gi18n-lib.h>
#include <totem-pl-parser.h>
#include <string.h>
#include <stdlib.h>

#include "grl-optical-media.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT optical_media_log_domain
GRL_LOG_DOMAIN_STATIC(optical_media_log_domain);

/* --- Plugin information --- */

#define SOURCE_ID   "grl-optical-media"
#define SOURCE_NAME _("Optical Media")
#define SOURCE_DESC _("A source for browsing optical media")

/* --- Grilo OpticalMedia Private --- */

#define NUM_MONITOR_SIGNALS 3

struct _GrlOpticalMediaSourcePrivate {
  GVolumeMonitor *monitor;
  guint monitor_signal_ids[NUM_MONITOR_SIGNALS];
  /* List of GrlMedia */
  GList *list;
  GHashTable *ignored_schemes;
  GCancellable *cancellable;
  gboolean notify_changes;
};

/* --- Data types --- */

static GrlOpticalMediaSource *grl_optical_media_source_new (void);

static void grl_optical_media_source_finalize (GObject *object);

gboolean grl_optical_media_plugin_init (GrlRegistry *registry,
                                        GrlPlugin *plugin,
                                        GList *configs);

static const GList *grl_optical_media_source_supported_keys (GrlSource *source);

static void grl_optical_media_source_browse (GrlSource *source,
                                             GrlSourceBrowseSpec *bs);

static gboolean grl_optical_media_source_notify_change_start (GrlSource *source,
                                                              GError **error);

static gboolean grl_optical_media_source_notify_change_stop (GrlSource *source,
                                                             GError **error);

static void grl_optical_media_source_cancel (GrlSource *source,
                                             guint operation_id);

static void
on_g_volume_monitor_removed_event (GVolumeMonitor        *monitor,
                                   GMount                *mount,
                                   GrlOpticalMediaSource *source);
static void
on_g_volume_monitor_changed_event (GVolumeMonitor        *monitor,
                                   GMount                *mount,
                                   GrlOpticalMediaSource *source);
static void
on_g_volume_monitor_added_event (GVolumeMonitor        *monitor,
                                 GMount                *mount,
                                 GrlOpticalMediaSource *source);

/* =================== OpticalMedia Plugin  =============== */

static char *
normalise_scheme (const char *scheme)
{
  char *s;

  if (scheme == NULL)
    return NULL;

  if (!g_ascii_isalnum (scheme[0])) {
    GRL_DEBUG ("Ignoring 'ignore-scheme' '%s' as it is not valid", scheme);
    return NULL;
  }

  for (s = (char *) (scheme + 1); *s != '\0'; s++) {
    if (!g_ascii_isalnum (*s) &&
        *s != '+' &&
        *s != '-' &&
        *s != '.') {
      GRL_DEBUG ("Ignoring 'ignore-scheme' '%s' as it is not valid", scheme);
      return NULL;
    }
  }

  return g_ascii_strdown (scheme, -1);
}

gboolean
grl_optical_media_plugin_init (GrlRegistry *registry,
                               GrlPlugin *plugin,
                               GList *configs)
{
  GrlOpticalMediaSource *source;

  GRL_LOG_DOMAIN_INIT (optical_media_log_domain, "optical_media");

  GRL_DEBUG ("%s", __FUNCTION__);

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  source = grl_optical_media_source_new ();
  source->priv->ignored_schemes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  for (; configs; configs = g_list_next (configs)) {
    GrlConfig *config = configs->data;
    gchar *scheme, *normalised_scheme;

    scheme = grl_config_get_string (config, GRILO_CONF_IGNORED_SCHEME);
    normalised_scheme = normalise_scheme (scheme);
    g_free (scheme);
    if (normalised_scheme)
      g_hash_table_insert (source->priv->ignored_schemes, normalised_scheme, GINT_TO_POINTER(1));
    else
      g_free (normalised_scheme);
  }

  grl_registry_register_source (registry,
                                plugin,
                                GRL_SOURCE (source),
                                NULL);

  return TRUE;
}

GRL_PLUGIN_DEFINE (GRL_MAJOR,
                   GRL_MINOR,
                   OPTICAL_MEDIA_PLUGIN_ID,
                   "Optical Media",
                   "A plugin for browsing optical media",
                   "GNOME",
                   VERSION,
                   "LGPL-2.1-or-later",
                   "http://www.gnome.org",
                   grl_optical_media_plugin_init,
                   NULL,
                   NULL);

/* ================== OpticalMedia GObject ================ */


G_DEFINE_TYPE_WITH_PRIVATE (GrlOpticalMediaSource, grl_optical_media_source, GRL_TYPE_SOURCE)

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
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

  object_class->finalize = grl_optical_media_source_finalize;

  source_class->supported_keys = grl_optical_media_source_supported_keys;
  source_class->cancel = grl_optical_media_source_cancel;
  source_class->browse = grl_optical_media_source_browse;

  source_class->notify_change_start = grl_optical_media_source_notify_change_start;
  source_class->notify_change_stop = grl_optical_media_source_notify_change_stop;
}

static void
grl_optical_media_source_init (GrlOpticalMediaSource *source)
{
  source->priv = grl_optical_media_source_get_instance_private (source);

  source->priv->cancellable = g_cancellable_new ();
  source->priv->monitor = g_volume_monitor_get ();

  source->priv->monitor_signal_ids[0] = g_signal_connect (G_OBJECT (source->priv->monitor), "mount-added",
                                                          G_CALLBACK (on_g_volume_monitor_added_event), source);
  source->priv->monitor_signal_ids[1] = g_signal_connect (G_OBJECT (source->priv->monitor), "mount-changed",
                                                          G_CALLBACK (on_g_volume_monitor_changed_event), source);
  source->priv->monitor_signal_ids[2] = g_signal_connect (G_OBJECT (source->priv->monitor), "mount-removed",
                                                          G_CALLBACK (on_g_volume_monitor_removed_event), source);
}

static void
grl_optical_media_source_finalize (GObject *object)
{
  GrlOpticalMediaSource *source = GRL_OPTICAL_MEDIA_SOURCE (object);
  guint i;

  g_cancellable_cancel (source->priv->cancellable);
  g_clear_object (&source->priv->cancellable);
  g_hash_table_destroy (source->priv->ignored_schemes);
  source->priv->ignored_schemes = NULL;

  for (i = 0; i < NUM_MONITOR_SIGNALS; i++) {
    g_signal_handler_disconnect (G_OBJECT (source->priv->monitor),
                                 source->priv->monitor_signal_ids[i]);
  }

  g_list_free_full (source->priv->list, g_object_unref);

  g_clear_object (&source->priv->monitor);

  G_OBJECT_CLASS (grl_optical_media_source_parent_class)->finalize (object);
}

/* ======================= Utilities ==================== */

static char *
create_mount_id (GMount *mount)
{
  GFile *root;
  char *uri;

  root = g_mount_get_root (mount);
  uri = g_file_get_uri (root);
  g_object_unref (root);

  return uri;
}

static char *
get_uri_for_gicon (GIcon *icon)
{
  char *uri;

  uri = NULL;

  if (G_IS_EMBLEMED_ICON (icon) != FALSE) {
    GIcon *new_icon;
    new_icon = g_emblemed_icon_get_icon (G_EMBLEMED_ICON (icon));
    icon = new_icon;
  }

  if (G_IS_FILE_ICON (icon) != FALSE) {
    GFile *file;

    file = g_file_icon_get_file (G_FILE_ICON (icon));
    uri = g_file_get_uri (file);

    return uri;
  }

  /* We leave the themed icons up to the applications to set */

  return uri;
}

static void
media_set_metadata (GMount   *mount,
                    GrlMedia *media)
{
  char *name, *icon_uri;
  GIcon *icon;

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
}

static gint
find_mount (gconstpointer a,
            gconstpointer b)
{
  GrlMedia *media = (GrlMedia *) a;
  GMount *mount = (GMount *) b;
  char *id;
  gint ret;

  id = create_mount_id (mount);
  ret = g_strcmp0 (id, grl_media_get_id (media));
  g_free (id);
  return ret;
}

static gboolean
ignore_drive (GDrive *drive)
{
  GIcon *icon;

  if (g_drive_can_eject (drive) == FALSE ||
      g_drive_has_media (drive) == FALSE) {
    GRL_DEBUG ("%s: Not adding %s as cannot eject or has no media", __FUNCTION__,
               g_drive_get_name (drive));
    return TRUE;
  }

  /* Hack to avoid USB devices showing up
   * https://bugzilla.gnome.org/show_bug.cgi?id=679624 */
  icon = g_drive_get_icon (drive);
  if (icon && G_IS_THEMED_ICON (icon)) {
    const gchar * const * names;
    names = g_themed_icon_get_names (G_THEMED_ICON (icon));
    if (names && names[0] && !g_str_has_prefix (names[0], "drive-optical")) {
      g_object_unref (icon);
      GRL_DEBUG ("%s: Not adding drive %s as is not optical drive", __FUNCTION__,
                 g_drive_get_name (drive));
      return TRUE;
    }
  }
  g_clear_object (&icon);

  return FALSE;
}

static gboolean
ignore_volume (GVolume *volume)
{
  gboolean ret = TRUE;
  char *path;
  GDrive *drive;

  /* Ignore drive? */
  drive = g_volume_get_drive (volume);
  if (drive != NULL && ignore_drive (drive)) {
    g_object_unref (drive);
    return TRUE;
  }
  g_clear_object (&drive);

  path = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);

  if (path != NULL) {
    ret = FALSE;
    g_free (path);
  } else {
    GRL_DEBUG ("%s: Not adding volume %s as it has no identifier", __FUNCTION__,
               g_volume_get_name (volume));
  }

  return ret;
}

static gboolean
ignore_mount (GMount *mount)
{
  GFile *root;
  GVolume *volume;
  gboolean ret = TRUE;

  root = g_mount_get_root (mount);

  if (g_file_has_uri_scheme (root, "burn") != FALSE || g_file_has_uri_scheme (root, "cdda") != FALSE) {
    /* We don't add Audio CDs, or blank media */
    g_object_unref (root);
    GRL_DEBUG ("%s: Not adding mount %s as is burn or cdda", __FUNCTION__,
               g_mount_get_name (mount));
    return TRUE;
  }
  g_object_unref (root);

  volume = g_mount_get_volume (mount);
  if (volume == NULL)
    return ret;

  ret = ignore_volume (volume);
  g_object_unref (volume);

  return ret;
}

static GrlMedia *
create_media_from_mount (GMount *mount)
{
  char *id;
  GrlMedia *media;

  /* Is it an audio CD or a blank media */
  if (ignore_mount (mount)) {
    GRL_DEBUG ("%s: Ignoring mount %s", __FUNCTION__,
               g_mount_get_name (mount));
    g_object_unref (mount);
    return NULL;
  }

  id = create_mount_id (mount);
  if (id == NULL) {
    GRL_DEBUG ("%s: Not adding mount %s as has no device path", __FUNCTION__,
               g_mount_get_name (mount));
    return NULL;
  }

  media = grl_media_video_new ();

  grl_media_set_id (media, id);
  g_free (id);

  media_set_metadata (mount, media);
  grl_media_set_mime (media, "x-special/device-block");

  GRL_DEBUG ("%s: Adding mount %s (id: %s)", __FUNCTION__,
             g_mount_get_name (mount), grl_media_get_id (media));

  return media;
}

static void
parsed_finished_item (TotemPlParser         *pl,
                      GAsyncResult          *result,
                      GrlOpticalMediaSource *source)
{
  GrlMedia **media;
  TotemPlParserResult retval;

  media = g_object_get_data (G_OBJECT (pl), "media");
  retval = totem_pl_parser_parse_finish (TOTEM_PL_PARSER (pl), result, NULL);
  if (retval == TOTEM_PL_PARSER_RESULT_SUCCESS &&
      grl_media_get_url (*media) != NULL) {
    source->priv->list = g_list_append (source->priv->list, g_object_ref (*media));
    if (source->priv->notify_changes) {
      grl_source_notify_change (GRL_SOURCE (source), *media, GRL_CONTENT_ADDED, FALSE);
    }
  }

  g_object_unref (*media);
  g_object_unref (pl);
}

static void
entry_parsed_cb (TotemPlParser  *parser,
                 const char     *uri,
                 GHashTable     *metadata,
                 GrlMedia      **media)
{
  char *scheme;

  g_return_if_fail (*media != NULL);
  if (grl_media_get_url (*media) != NULL) {
    GRL_WARNING ("Was going to set media '%s' to URL '%s' but already has URL '%s'",
                 grl_media_get_id (*media),
                 uri,
                 grl_media_get_url (*media));
    return;
  }

  scheme = g_uri_parse_scheme (uri);
  if (scheme != NULL && !g_str_equal (scheme, "file"))
    grl_media_set_url (*media, uri);
  g_free (scheme);
}

static void
on_g_volume_monitor_added_event (GVolumeMonitor        *monitor,
                                 GMount                *mount,
                                 GrlOpticalMediaSource *source)
{
  GrlMedia **media;
  TotemPlParser *pl;

  if (ignore_mount (mount))
    return;

  media = (GrlMedia **) g_new0 (gpointer, 1);
  *media = create_media_from_mount (mount);
  if (*media == NULL) {
    g_free (media);
    return;
  }

  pl = totem_pl_parser_new ();
  g_object_set_data (G_OBJECT (pl), "media", media);
  g_object_set (pl, "recurse", FALSE, NULL);
  g_signal_connect (G_OBJECT (pl), "entry-parsed",
                    G_CALLBACK (entry_parsed_cb), media);
  totem_pl_parser_parse_async (pl,
                               grl_media_get_id (*media),
                               FALSE,
                               source->priv->cancellable,
                               (GAsyncReadyCallback) parsed_finished_item,
                               source);
}

static void
on_g_volume_monitor_removed_event (GVolumeMonitor        *monitor,
                                   GMount                *mount,
                                   GrlOpticalMediaSource *source)
{
  GList *l;
  GrlMedia *media;

  l = g_list_find_custom (source->priv->list, mount, find_mount);
  if (!l)
    return;

  media = l->data;
  source->priv->list = g_list_remove (source->priv->list, media);
  if (source->priv->notify_changes) {
    grl_source_notify_change (GRL_SOURCE (source), media, GRL_CONTENT_REMOVED, FALSE);
  }
  g_object_unref (media);
}

static void
on_g_volume_monitor_changed_event (GVolumeMonitor        *monitor,
                                   GMount                *mount,
                                   GrlOpticalMediaSource *source)
{
  GList *l;

  l = g_list_find_custom (source->priv->list, mount, find_mount);
  if (!l)
    return;

  media_set_metadata (mount, l->data);

  if (source->priv->notify_changes) {
    grl_source_notify_change (GRL_SOURCE (source), l->data, GRL_CONTENT_CHANGED, FALSE);
  }
}

/* ================== API Implementation ================ */

static const GList *
grl_optical_media_source_supported_keys (GrlSource *source)
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

typedef struct {
  TotemPlParser *parser;
  GCancellable *cancellable;
  GrlSource *source;
  GrlSourceBrowseSpec *bs;
  GList *media_list;
  GrlMedia *media;
} BrowseData;

static void resolve_disc_urls (BrowseData *data);

static gboolean
ignore_url (BrowseData *data)
{
  GrlOpticalMediaSource *source = GRL_OPTICAL_MEDIA_SOURCE (data->bs->source);
  GrlMedia *media = data->media;
  char *scheme, *scheme_lower;
  gboolean ret = FALSE;
  const char *url;

  url = grl_media_get_url (media);
  if (url == NULL)
    return TRUE;

  scheme = g_uri_parse_scheme (url);
  scheme_lower = g_ascii_strdown (scheme, -1);
  g_free (scheme);
  ret = g_hash_table_lookup (source->priv->ignored_schemes, scheme_lower) != NULL;
  g_free (scheme_lower);

  return ret;
}

static void
parsed_finished (TotemPlParser *pl, GAsyncResult *result, BrowseData *data)
{
  TotemPlParserResult retval;
  GError *error = NULL;

  retval = totem_pl_parser_parse_finish (TOTEM_PL_PARSER (pl), result, &error);

  /* Do the fallback ourselves */
  if (retval == TOTEM_PL_PARSER_RESULT_IGNORED) {
    GRL_DEBUG ("%s: Falling back for %s as has it's been ignored", __FUNCTION__,
               grl_media_get_id (data->media));
    grl_media_set_url (data->media, grl_media_get_id (data->media));
    retval = TOTEM_PL_PARSER_RESULT_SUCCESS;
  }

  if (retval == TOTEM_PL_PARSER_RESULT_SUCCESS &&
      !ignore_url (data)) {
    GrlOpticalMediaSource *source;

    source = GRL_OPTICAL_MEDIA_SOURCE (data->bs->source);

    GRL_DEBUG ("%s: Adding %s which resolved to %s", __FUNCTION__,
               grl_media_get_id (data->media),
               grl_media_get_url (data->media));
    data->bs->callback (GRL_SOURCE (source),
                        data->bs->operation_id,
                        data->media,
                        -1,
                        data->bs->user_data,
                        NULL);
    source->priv->list = g_list_append (source->priv->list, g_object_ref (data->media));
  } else {
    if (retval == TOTEM_PL_PARSER_RESULT_ERROR ||
        retval == TOTEM_PL_PARSER_RESULT_CANCELLED) {
      GRL_WARNING ("Failed to parse '%s': %s",
                   grl_media_get_id (data->media),
                   error ? error->message : "No reason");
      g_error_free (error);
    }
    g_object_unref (data->media);
  }
  data->media = NULL;

  resolve_disc_urls (data);
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
                        data->bs->operation_id,
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
grl_optical_media_source_browse (GrlSource *source,
                                 GrlSourceBrowseSpec *bs)
{
  GList *mounts, *l;
  GrlOpticalMediaSourcePrivate *priv = GRL_OPTICAL_MEDIA_SOURCE (source)->priv;
  BrowseData *data;
  GList *media_list;

  GRL_DEBUG ("%s", __FUNCTION__);

  g_list_free_full (priv->list, g_object_unref);
  priv->list = NULL;

  media_list = NULL;

  /* Look for loopback-mounted ISO images and discs */
  mounts = g_volume_monitor_get_mounts (priv->monitor);
  for (l = mounts; l != NULL; l = l->next) {
    GMount *mount = l->data;

    if (!ignore_mount (mount)) {
      GrlMedia *media;
      media = create_media_from_mount (mount);
      if (media)
        media_list = g_list_prepend (media_list, media);
    }

    g_object_unref (mount);
  }
  g_list_free (mounts);

  /* Got nothing? */
  if (media_list == NULL) {
    /* Tell the caller we're done */
    bs->callback (bs->source,
                  bs->operation_id,
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

  grl_operation_set_data (bs->operation_id, data->cancellable);

  data->parser = totem_pl_parser_new ();
  g_object_set (data->parser, "recurse", FALSE, NULL);
  g_signal_connect (G_OBJECT (data->parser), "entry-parsed",
                    G_CALLBACK (entry_parsed_cb), &data->media);

  resolve_disc_urls (data);
}

static gboolean
grl_optical_media_source_notify_change_start (GrlSource *source,
                                              GError **error)
{
  GrlOpticalMediaSourcePrivate *priv = GRL_OPTICAL_MEDIA_SOURCE (source)->priv;

  priv->notify_changes = TRUE;

  return TRUE;
}

static gboolean
grl_optical_media_source_notify_change_stop (GrlSource *source,
                                             GError **error)
{
  GrlOpticalMediaSourcePrivate *priv = GRL_OPTICAL_MEDIA_SOURCE (source)->priv;

  priv->notify_changes = FALSE;

  return TRUE;
}

static void
grl_optical_media_source_cancel (GrlSource *source, guint operation_id)
{
  GCancellable *cancellable;

  cancellable = G_CANCELLABLE (grl_operation_get_data (operation_id));

  if (cancellable) {
    g_cancellable_cancel (cancellable);
  }
}
