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

#include "grl-guardianvideos.h"

#define GUARDIANVIDEOS_URL "http://www.guardian.co.uk/video/rss"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT guardianvideos_log_domain
GRL_LOG_DOMAIN_STATIC(guardianvideos_log_domain);

/* --- Plugin information --- */

#define PLUGIN_ID   GUARDIANVIDEOS_PLUGIN_ID

#define SOURCE_ID   "grl-guardianvideos"
#define SOURCE_NAME _("The Guardian Videos")
#define SOURCE_DESC _("A source for browsing videos from the Guardian")

/* --- Grilo GuardianVideos Private --- */

#define GRL_GUARDIANVIDEOS_SOURCE_GET_PRIVATE(object)           \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                       \
                               GRL_GUARDIANVIDEOS_SOURCE_TYPE,  \
                               GrlGuardianVideosSourcePrivate))

struct _GrlGuardianVideosSourcePrivate {
  GrlMedia *media;
  int       last_seen_channel;
};

/* --- Data types --- */

static GrlGuardianVideosSource *grl_guardianvideos_source_new (void);

static void grl_guardianvideos_source_finalize (GObject *object);

gboolean grl_guardianvideos_plugin_init (GrlRegistry *registry,
                                  GrlPlugin   *plugin,
                                  GList       *configs);

static const GList *grl_guardianvideos_source_supported_keys (GrlSource *source);

static void grl_guardianvideos_source_browse (GrlSource           *source,
                                       GrlSourceBrowseSpec *bs);

/* =================== GuardianVideos Plugin  =============== */

gboolean
grl_guardianvideos_plugin_init (GrlRegistry *registry,
                         GrlPlugin   *plugin,
                         GList       *configs)
{
  GrlGuardianVideosSource *source;

  GRL_LOG_DOMAIN_INIT (guardianvideos_log_domain, "guardianvideos");

  GRL_DEBUG ("%s", __FUNCTION__);

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  source = grl_guardianvideos_source_new ();
  registry = grl_registry_get_default ();

  grl_registry_register_source (registry,
                                plugin,
                                GRL_SOURCE (source),
                                NULL);

  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_guardianvideos_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== GuardianVideos GObject ================ */


G_DEFINE_TYPE (GrlGuardianVideosSource,
               grl_guardianvideos_source,
               GRL_TYPE_SOURCE);

static GrlGuardianVideosSource *
grl_guardianvideos_source_new (void)
{
  GIcon *icon;
  GFile *file;
  GrlGuardianVideosSource *object;

  GRL_DEBUG ("%s", __FUNCTION__);

  file = g_file_new_for_uri ("resource:///org/gnome/grilo/plugins/guardianvideos/guardian.png");
  icon = g_file_icon_new (file);
  g_object_unref (file);
  object = g_object_new (GRL_GUARDIANVIDEOS_SOURCE_TYPE,
                         "source-id", SOURCE_ID,
                         "source-name", SOURCE_NAME,
                         "source-desc", SOURCE_DESC,
                         "supported-media", GRL_MEDIA_TYPE_VIDEO,
                         "source-icon", icon,
                         NULL);
  return object;
}

static void
grl_guardianvideos_source_class_init (GrlGuardianVideosSourceClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

  object_class->finalize = grl_guardianvideos_source_finalize;

  source_class->supported_keys = grl_guardianvideos_source_supported_keys;
  source_class->browse = grl_guardianvideos_source_browse;

  g_type_class_add_private (klass, sizeof (GrlGuardianVideosSourcePrivate));
}

static void
grl_guardianvideos_source_init (GrlGuardianVideosSource *source)
{
  GrlGuardianVideosSourcePrivate *priv;

  priv = source->priv = GRL_GUARDIANVIDEOS_SOURCE_GET_PRIVATE(source);

  priv->media = grl_media_new ();
  grl_media_set_url (priv->media, GUARDIANVIDEOS_URL);
}

static void
grl_guardianvideos_source_finalize (GObject *object)
{
  GrlGuardianVideosSource *source = GRL_GUARDIANVIDEOS_SOURCE (object);
  GrlGuardianVideosSourcePrivate *priv = GRL_GUARDIANVIDEOS_SOURCE (source)->priv;

  g_object_unref (priv->media);

  G_OBJECT_CLASS (grl_guardianvideos_source_parent_class)->finalize (object);
}

/* ================== API Implementation ================ */

static const GList *
grl_guardianvideos_source_supported_keys (GrlSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_URL,
                                      GRL_METADATA_KEY_THUMBNAIL,
                                      GRL_METADATA_KEY_DESCRIPTION,
                                      NULL);
  }
  return keys;
}

static GrlMedia *
filter_func (GrlSource   *source,
             GrlMedia    *media,
             gpointer     user_data)
{
  GrlMedia *ret;
  const char *url;

  url = grl_media_get_url (media);

  ret = grl_media_video_new ();
  grl_media_set_url (ret, url);
  grl_media_set_id (ret, url);
  grl_media_set_title (ret, grl_media_get_title (media));
  grl_media_set_thumbnail (ret, grl_media_get_thumbnail (media));
  grl_media_set_description (ret, grl_media_get_description (media));

  g_object_unref (media);

  return ret;
}

static void
grl_guardianvideos_source_browse (GrlSource           *source,
                           GrlSourceBrowseSpec *bs)
{
  GrlGuardianVideosSourcePrivate *priv = GRL_GUARDIANVIDEOS_SOURCE (source)->priv;

  grl_pls_browse (source,
                  priv->media,
                  bs->keys,
                  bs->options,
                  filter_func,
                  bs->callback,
                  bs->user_data);
}
