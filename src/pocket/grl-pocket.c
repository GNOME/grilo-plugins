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
#include <totem-pl-parser-mini.h>

#include "grl-pocket.h"
#include "gnome-pocket.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT pocket_log_domain
GRL_LOG_DOMAIN_STATIC(pocket_log_domain);

/* --- Plugin information --- */

#define PLUGIN_ID   POCKET_PLUGIN_ID

#define SOURCE_ID   "grl-pocket"
#define SOURCE_NAME _("Pocket")
#define SOURCE_DESC _("A source for browsing Pocket videos")

/* --- Grilo Pocket Private --- */

#define GRL_POCKET_SOURCE_GET_PRIVATE(object)           \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                       \
                               GRL_POCKET_SOURCE_TYPE,  \
                               GrlPocketSourcePrivate))

struct _GrlPocketSourcePrivate {
  GnomePocket *pocket;
  gboolean     cache_loaded;
};

/* --- Data types --- */

typedef struct {
  GrlSourceBrowseSpec *bs;
  GCancellable        *cancellable;
  GrlPocketSource     *source;
} OperationData;

static GrlPocketSource *grl_pocket_source_new (GnomePocket *pocket);

gboolean grl_pocket_plugin_init (GrlRegistry *registry,
                                 GrlPlugin   *plugin,
                                 GList       *configs);

static const GList *grl_pocket_source_supported_keys (GrlSource *source);

static void grl_pocket_source_cancel (GrlSource *source,
                                      guint      operation_id);
static void grl_pocket_source_browse (GrlSource           *source,
                                       GrlSourceBrowseSpec *bs);

/* =================== Pocket Plugin  =============== */

static void
is_available (GObject    *gobject,
              GParamSpec *pspec,
              GrlPlugin  *plugin)
{
  gboolean avail;
  GrlPocketSource *source;
  GnomePocket *pocket;

  source = g_object_get_data (G_OBJECT (plugin), "source");
  pocket = g_object_get_data (G_OBJECT (plugin), "pocket");
  g_object_get (pocket, "available", &avail, NULL);

  if (!avail) {
    GrlRegistry *registry;

    if (source == NULL)
      return;

    GRL_DEBUG ("Removing Pocket");

    registry = grl_registry_get_default ();

    grl_registry_unregister_source (registry,
                                    GRL_SOURCE (source),
                                    NULL);
  } else {
    GrlRegistry *registry;

    if (source != NULL)
      return;

    GRL_DEBUG ("Adding Pocket");

    source = grl_pocket_source_new (pocket);
    registry = grl_registry_get_default ();

    g_object_set_data (G_OBJECT (plugin), "source", source);
    grl_registry_register_source (registry,
                                  plugin,
                                  GRL_SOURCE (source),
                                  NULL);
  }
}

gboolean
grl_pocket_plugin_init (GrlRegistry *registry,
                        GrlPlugin   *plugin,
                        GList       *configs)
{
  GnomePocket *pocket;

  GRL_LOG_DOMAIN_INIT (pocket_log_domain, "pocket");

  GRL_DEBUG ("%s", __FUNCTION__);

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  pocket = gnome_pocket_new ();
  g_object_set_data (G_OBJECT (plugin), "pocket", pocket);
  g_signal_connect (pocket, "notify::available",
                    G_CALLBACK (is_available), plugin);

  return TRUE;
}

static void
grl_pocket_plugin_deinit (GrlPlugin *plugin)
{
  GnomePocket *pocket;

  GRL_DEBUG ("grl_pocket_plugin_deinit");

  pocket = g_object_get_data (G_OBJECT (plugin), "pocket");
  g_clear_object (&pocket);
  g_object_set_data (G_OBJECT (plugin), "pocket", NULL);
}

GRL_PLUGIN_REGISTER (grl_pocket_plugin_init,
                     grl_pocket_plugin_deinit,
                     PLUGIN_ID);

/* ================== Pocket GObject ================ */


G_DEFINE_TYPE (GrlPocketSource,
               grl_pocket_source,
               GRL_TYPE_SOURCE);

static GrlPocketSource *
grl_pocket_source_new (GnomePocket *pocket)
{
  GIcon *icon;
  GFile *file;
  GrlPocketSource *object;

  g_return_val_if_fail (GNOME_IS_POCKET (pocket), NULL);

  GRL_DEBUG ("%s", __FUNCTION__);

  file = g_file_new_for_uri ("resource:///org/gnome/grilo/plugins/pocket/channel-pocket.svg");
  icon = g_file_icon_new (file);
  g_object_unref (file);
  object = g_object_new (GRL_POCKET_SOURCE_TYPE,
                         "source-id", SOURCE_ID,
                         "source-name", SOURCE_NAME,
                         "source-desc", SOURCE_DESC,
                         "supported-media", GRL_MEDIA_TYPE_VIDEO,
                         "source-icon", icon,
                         NULL);
  GRL_POCKET_SOURCE (object)->priv->pocket = pocket;

  return object;
}

static void
grl_pocket_source_class_init (GrlPocketSourceClass * klass)
{
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

  source_class->supported_keys = grl_pocket_source_supported_keys;
  source_class->browse = grl_pocket_source_browse;
  source_class->cancel = grl_pocket_source_cancel;

  g_type_class_add_private (klass, sizeof (GrlPocketSourcePrivate));
}

static void
grl_pocket_source_init (GrlPocketSource *source)
{
  source->priv = GRL_POCKET_SOURCE_GET_PRIVATE(source);
}

/* ================== API Implementation ================ */

static const GList *
grl_pocket_source_supported_keys (GrlSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_URL,
                                      GRL_METADATA_KEY_FAVOURITE,
                                      GRL_METADATA_KEY_CREATION_DATE,
                                      NULL);
  }
  return keys;
}

static GrlMedia *
item_to_media (GnomePocketItem *item)
{
  GrlMedia *ret;
  GDateTime *date;
  gboolean has_video;

  /* There are a few false positives, but this makes detection
   * much faster as well */
  has_video = (item->has_video == POCKET_HAS_MEDIA_INCLUDED ||
               item->has_video == POCKET_IS_MEDIA);
  if (!has_video) {
    GRL_DEBUG ("Ignoring ID %s as it wasn't detected as a video, or as including a video (URL: %s)",
               item->id, item->url);
    return NULL;
  }

  if (!totem_pl_parser_can_parse_from_uri (item->url, FALSE)) {
    GRL_DEBUG ("Ignoring ID %s as it wasn't detected as from a videosite (URL: %s)",
               item->id, item->url);
    return NULL;
  }

  ret = grl_media_video_new ();
  grl_media_set_url (ret, item->url);
  grl_media_set_title (ret, item->title);
  grl_media_set_favourite (ret, item->favorite);

  date = g_date_time_new_from_unix_utc (item->time_added);
  grl_media_set_creation_date (ret, date);
  g_date_time_unref (date);

  return ret;
}

static void
refresh_cb (GObject      *object,
            GAsyncResult *res,
            gpointer      user_data)
{
  OperationData *op_data = user_data;
  GError *error = NULL;
  GList *items, *l;

  if (gnome_pocket_refresh_finish (op_data->source->priv->pocket,
                                   res, &error) == FALSE) {
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      goto out;
    }

    op_data->bs->callback (op_data->bs->source,
                           op_data->bs->operation_id,
                           NULL,
                           0,
                           op_data->bs->user_data,
                           error);
    goto out;
  }

  items = gnome_pocket_get_items (op_data->source->priv->pocket);
  for (l = items; l != NULL; l = l->next) {
    GnomePocketItem *item = l->data;
    GrlMedia *media;

    media = item_to_media (item);
    if (media == NULL)
      continue;

    op_data->bs->callback (op_data->bs->source,
                           op_data->bs->operation_id,
                           media,
                           GRL_SOURCE_REMAINING_UNKNOWN,
                           op_data->bs->user_data,
                           NULL);
  }

  op_data->bs->callback (op_data->bs->source,
                         op_data->bs->operation_id,
                         NULL,
                         0,
                         op_data->bs->user_data,
                         NULL);

out:
  g_clear_object (&op_data->cancellable);
  g_slice_free (OperationData, op_data);
}

static void
load_cached_cb (GObject      *object,
                GAsyncResult *res,
                gpointer      user_data)
{
  OperationData *op_data = user_data;
  GError *error = NULL;

  if (gnome_pocket_load_cached_finish (op_data->source->priv->pocket,
                                       res, &error) == FALSE) {
    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      goto out;
    }
  }

  op_data->source->priv->cache_loaded = TRUE;
  gnome_pocket_refresh (op_data->source->priv->pocket,
                        op_data->cancellable,
                        refresh_cb,
                        op_data);
  return;

out:
  g_clear_object (&op_data->cancellable);
  g_slice_free (OperationData, op_data);
}

static void
grl_pocket_source_browse (GrlSource           *source,
                          GrlSourceBrowseSpec *bs)
{
  GrlPocketSourcePrivate *priv = GRL_POCKET_SOURCE (source)->priv;
  OperationData *op_data;

  GRL_DEBUG (__FUNCTION__);

  op_data = g_slice_new0 (OperationData);
  op_data->bs = bs;
  op_data->cancellable = g_cancellable_new ();
  op_data->source = GRL_POCKET_SOURCE (source);
  grl_operation_set_data (bs->operation_id, op_data);

  if (!priv->cache_loaded) {
    gnome_pocket_load_cached (priv->pocket,
                              op_data->cancellable,
                              load_cached_cb,
                              op_data);
  } else {
    gnome_pocket_refresh (priv->pocket,
                          op_data->cancellable,
                          refresh_cb,
                          op_data);
  }
}

static void
grl_pocket_source_cancel (GrlSource *source,
                          guint      operation_id)
{
  OperationData *op_data;

  GRL_DEBUG ("grl_pocket_source_cancel");

  op_data = (OperationData *) grl_operation_get_data (operation_id);
  if (op_data)
    g_cancellable_cancel (op_data->cancellable);
}
