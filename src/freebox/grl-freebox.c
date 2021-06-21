/*
 * Copyright (C) 2013 Bastien Nocera
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
#include <string.h>
#include <stdlib.h>
#include <pls/grl-pls.h>

#include "grl-freebox.h"
#include "freebox-monitor.h"

#define FREEBOXTV_URL    "http://mafreebox.freebox.fr/freeboxtv/playlist.m3u"
#define FREEBOXRADIO_URL "resource:///org/gnome/grilo/plugins/freebox/radios.m3u"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT freebox_log_domain
GRL_LOG_DOMAIN_STATIC(freebox_log_domain);

/* --- Plugin information --- */

#define TV_SOURCE_ID   "grl-freeboxtv"
#define TV_SOURCE_NAME _("Freebox TV")
#define TV_SOURCE_DESC _("A source for browsing Freebox TV channels")

#define RADIO_SOURCE_ID   "grl-freeboxradio"
#define RADIO_SOURCE_NAME _("Freebox Radio")
#define RADIO_SOURCE_DESC _("A source for browsing Freebox radio channels")

/* --- Grilo Freebox Private --- */

struct _GrlFreeboxSourcePrivate {
  GrlMedia *media;
  int       last_seen_channel;
};

/* --- Data types --- */

static GrlFreeboxSource *grl_freebox_source_new_tv (void);
static GrlFreeboxSource *grl_freebox_source_new_radio (void);

static void grl_freebox_source_finalize (GObject *object);

gboolean grl_freebox_plugin_init (GrlRegistry *registry,
                                  GrlPlugin   *plugin,
                                  GList       *configs);

static const GList *grl_freebox_source_supported_keys (GrlSource *source);

static void grl_freebox_source_browse (GrlSource           *source,
                                       GrlSourceBrowseSpec *bs);

/* =================== Freebox Plugin  =============== */

static void
freebox_found (FreeboxMonitor *mon,
               const char     *name,
               GrlPlugin      *plugin)
{
  GrlFreeboxSource *source;
  GrlRegistry *registry;
  const char *sources[] = {
    "source-tv",
    "source-radio"
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS(sources); i++) {
    if (g_object_get_data (G_OBJECT (plugin), sources[i]) != NULL)
      return;

    GRL_DEBUG ("Found a Freebox: %s", name);

    if (g_strcmp0 (sources[i], "source-tv") == 0)
      source = grl_freebox_source_new_tv ();
    else
      source = grl_freebox_source_new_radio ();

    registry = grl_registry_get_default ();

    g_object_set_data (G_OBJECT (plugin), sources[i], source);
    grl_registry_register_source (registry,
                                  plugin,
                                  GRL_SOURCE (source),
                                  NULL);
  }
}

static void
freebox_lost (FreeboxMonitor *mon,
              const char     *name,
              GrlPlugin      *plugin)
{
  GrlRegistry *registry;
  const char *sources[] = {
    "source-tv",
    "source-radio"
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS(sources); i++) {
    GrlFreeboxSource *source = g_object_get_data (G_OBJECT (plugin), sources[i]);
    if (source == NULL)
      continue;

    GRL_DEBUG ("Remove a Freebox: %s", name);

    registry = grl_registry_get_default ();

    grl_registry_unregister_source (registry,
                                    GRL_SOURCE (source),
                                    NULL);
  }
}

gboolean
grl_freebox_plugin_init (GrlRegistry *registry,
                         GrlPlugin   *plugin,
                         GList       *configs)
{
  FreeboxMonitor *mon;

  GRL_LOG_DOMAIN_INIT (freebox_log_domain, "freebox");

  GRL_DEBUG ("%s", __FUNCTION__);

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  mon = freebox_monitor_new ();
  g_signal_connect (mon, "found",
                    G_CALLBACK (freebox_found), plugin);
  g_signal_connect (mon, "lost",
                    G_CALLBACK (freebox_lost), plugin);

  return TRUE;
}

GRL_PLUGIN_DEFINE (GRL_MAJOR,
                   GRL_MINOR,
                   FREEBOX_PLUGIN_ID,
                   "Freebox",
                   "A plugin for Freebox TV channels",
                   "GNOME",
                   VERSION,
                   "LGPL-2.1-or-later",
                   "http://www.gnome.org",
                   grl_freebox_plugin_init,
                   NULL,
                   NULL);

/* ================== Freebox GObject ================ */

G_DEFINE_TYPE_WITH_PRIVATE (GrlFreeboxSource, grl_freebox_source, GRL_TYPE_SOURCE)

static GrlFreeboxSource *
grl_freebox_source_new_tv (void)
{
  GIcon *icon;
  GFile *file;
  GrlFreeboxSource *object;
  const char *tags[] = {
    "tv",
    "country:fr",
    NULL
  };

  GRL_DEBUG ("%s", __FUNCTION__);

  file = g_file_new_for_uri ("resource:///org/gnome/grilo/plugins/freebox/free.png");
  icon = g_file_icon_new (file);
  g_object_unref (file);
  object = g_object_new (GRL_FREEBOX_SOURCE_TYPE,
                         "source-id", TV_SOURCE_ID,
                         "source-name", TV_SOURCE_NAME,
                         "source-desc", TV_SOURCE_DESC,
                         "supported-media", GRL_SUPPORTED_MEDIA_VIDEO,
                         "source-icon", icon,
                         "source-tags", tags,
                         NULL);
  grl_media_set_url (GRL_FREEBOX_SOURCE(object)->priv->media, FREEBOXTV_URL);
  g_object_unref (icon);

  return object;
}

static GrlFreeboxSource *
grl_freebox_source_new_radio (void)
{
  GIcon *icon;
  GFile *file;
  GrlFreeboxSource *object;
  const char *tags[] = {
    "radio",
    "country:fr",
    NULL
  };

  GRL_DEBUG ("%s", __FUNCTION__);

  file = g_file_new_for_uri ("resource:///org/gnome/grilo/plugins/freebox/free.png"); //FIXME
  icon = g_file_icon_new (file);
  g_object_unref (file);
  object = g_object_new (GRL_FREEBOX_SOURCE_TYPE,
                         "source-id", RADIO_SOURCE_ID,
                         "source-name", RADIO_SOURCE_NAME,
                         "source-desc", RADIO_SOURCE_DESC,
                         "supported-media", GRL_SUPPORTED_MEDIA_AUDIO,
                         "source-icon", icon,
                         "source-tags", tags,
                         NULL);
  grl_media_set_url (GRL_FREEBOX_SOURCE(object)->priv->media, FREEBOXRADIO_URL);
  g_object_unref (icon);

  return object;
}

static void
grl_freebox_source_class_init (GrlFreeboxSourceClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

  object_class->finalize = grl_freebox_source_finalize;

  source_class->supported_keys = grl_freebox_source_supported_keys;
  source_class->browse = grl_freebox_source_browse;
}

static void
grl_freebox_source_init (GrlFreeboxSource *source)
{
  GrlFreeboxSourcePrivate *priv;

  priv = source->priv = grl_freebox_source_get_instance_private (source);

  priv->media = grl_media_new ();
}

static void
grl_freebox_source_finalize (GObject *object)
{
  GrlFreeboxSource *source = GRL_FREEBOX_SOURCE (object);
  GrlFreeboxSourcePrivate *priv = GRL_FREEBOX_SOURCE (source)->priv;

  g_object_unref (priv->media);

  G_OBJECT_CLASS (grl_freebox_source_parent_class)->finalize (object);
}

/* ================== API Implementation ================ */

static const GList *
grl_freebox_source_supported_keys (GrlSource *source)
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
cleanup_title (const char *title)
{
  const char *flavours[] = {
    " (auto)",
    " (bas débit)",
    " (standard)",
    " (HD)"
  };
  guint i;
  const char *s;

  s = strstr (title, " - ") + strlen (" - ");
  g_return_val_if_fail (s != NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (flavours); i++) {
    if (g_str_has_suffix (s, flavours[i]))
      return g_strndup (s, strlen (s) - strlen (flavours[i]));
  }
  return g_strdup (s);
}

static char *
remove_flavour (const char *url)
{
  char *s;

  s = strstr (url, "&flavour=");
  if (s == NULL)
    return g_strdup (url);
  return g_strndup (url, s - url);
}

static GrlMedia *
filter_func_tv (GrlSource   *source,
                GrlMedia    *media,
                gpointer     user_data)
{
  GrlFreeboxSourcePrivate *priv = GRL_FREEBOX_SOURCE (source)->priv;
  GrlMedia *ret;
  const gchar *title;
  char *new_title;
  char *url;
  int channel_num;

  title = grl_media_get_title (media);
  if (title == NULL) {
    g_object_unref (media);
    return NULL;
  }

  /* Title are of the form:
   * channel_num - Name (flavour)
   * such as:
   * 689 - Star gold (standard)
   *
   * And channel numbers are ever increasing */
  channel_num = atoi (title);
  if (channel_num == priv->last_seen_channel) {
    /* Already processed this title */
    g_object_unref (media);
    return NULL;
  }
  /* if channel_num is lower than last_seen_channel,
   * we're actually processing the playlist again,
   * so don't skip it */
  priv->last_seen_channel = channel_num;

  new_title = cleanup_title (title);
  url = remove_flavour (grl_media_get_url (media));

  ret = grl_media_video_new ();
  grl_media_set_url (ret, url);
  grl_media_set_id (ret, url);
  grl_media_set_title (ret, new_title);
  g_free (new_title);
  g_free (url);

  g_object_unref (media);

  return ret;
}

static GrlMedia *
filter_func_radio (GrlSource   *source,
                   GrlMedia    *media,
                   gpointer     user_data)
{
  GrlMedia *ret;
  const gchar *title;
  char *new_title;
  char *id;

  title = grl_media_get_title (media);
  if (title == NULL) {
    g_object_unref (media);
    return NULL;
  }

  /* Title are of the form:
   * channel_num - Name
   * such as:
   * 100003 - France Inter */
  new_title = cleanup_title (title);

  ret = grl_media_audio_new ();
  grl_media_set_url (ret, grl_media_get_url (media));
  id = g_strdup_printf ("%s-%d",
                        grl_media_get_url (media),
                        grl_data_get_int (GRL_DATA (media), GRL_METADATA_KEY_AUDIO_TRACK));
  grl_media_set_id (ret, id);
  g_free (id);
  grl_data_set_int (GRL_DATA (ret), GRL_METADATA_KEY_AUDIO_TRACK,
                    grl_data_get_int (GRL_DATA (media), GRL_METADATA_KEY_AUDIO_TRACK));
  /* We'll add icon when http://dev.freebox.fr/bugs/task/14946 is fixed */
  grl_media_set_title (ret, new_title);
  g_free (new_title);

  g_object_unref (media);

  return ret;
}

static void
grl_freebox_source_browse (GrlSource           *source,
                           GrlSourceBrowseSpec *bs)
{
  GrlFreeboxSourcePrivate *priv = GRL_FREEBOX_SOURCE (source)->priv;

  bs->container = g_object_ref (priv->media);

  if (g_strcmp0 (grl_source_get_id (source), TV_SOURCE_ID) == 0) {
    grl_pls_browse_by_spec (source,
                            filter_func_tv,
                            bs);
  } else {
    grl_pls_browse_by_spec (source,
                            filter_func_radio,
                            bs);
  }
}
