/*
 * Copyright (C) 2010-2011 Igalia S.L.
 *
 * Contact: Guillaume Emont <gemont@igalia.com>
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

#include "grl-local-metadata.h"

#define GRL_LOG_DOMAIN_DEFAULT local_metadata_log_domain
GRL_LOG_DOMAIN_STATIC(local_metadata_log_domain);

#define PLUGIN_ID   LOCALMETADATA_PLUGIN_ID

#define SOURCE_ID   "grl-local-metadata"
#define SOURCE_NAME "Local Metadata Provider"
#define SOURCE_DESC "A source providing locally available metadata"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"


static GrlLocalMetadataSource *grl_local_metadata_source_new (void);

static void grl_local_metadata_source_resolve (GrlMetadataSource *source,
                                              GrlMetadataSourceResolveSpec *rs);

static const GList *grl_local_metadata_source_supported_keys (GrlMetadataSource *source);

static const GList * grl_local_metadata_source_key_depends (GrlMetadataSource *source,
                                                            GrlKeyID key_id);

gboolean grl_local_metadata_source_plugin_init (GrlPluginRegistry *registry,
                                               const GrlPluginInfo *plugin,
                                               GList *configs);


/* =================== GrlLocalMetadata Plugin  =============== */

gboolean
grl_local_metadata_source_plugin_init (GrlPluginRegistry *registry,
                                      const GrlPluginInfo *plugin,
                                      GList *configs)
{
  GRL_LOG_DOMAIN_INIT (local_metadata_log_domain, "local-metadata");

  GRL_DEBUG ("grl_local_metadata_source_plugin_init");

  GrlLocalMetadataSource *source = grl_local_metadata_source_new ();
  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source),
                                       NULL);
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_local_metadata_source_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== GrlLocalMetadata GObject ================ */

static GrlLocalMetadataSource *
grl_local_metadata_source_new (void)
{
  GRL_DEBUG ("grl_local_metadata_source_new");
  return g_object_new (GRL_LOCAL_METADATA_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       NULL);
}

static void
grl_local_metadata_source_class_init (GrlLocalMetadataSourceClass * klass)
{
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);
  metadata_class->supported_keys = grl_local_metadata_source_supported_keys;
  metadata_class->key_depends = grl_local_metadata_source_key_depends;
  metadata_class->resolve = grl_local_metadata_source_resolve;
}

static void
grl_local_metadata_source_init (GrlLocalMetadataSource *source)
{
}

G_DEFINE_TYPE (GrlLocalMetadataSource,
               grl_local_metadata_source,
               GRL_TYPE_METADATA_SOURCE);

/* ======================= Utilities ==================== */
static void
got_file_info (GFile *file, GAsyncResult *result,
               GrlMetadataSourceResolveSpec *rs)
{
  GFileInfo *info;
  GError *error = NULL;
  const gchar *thumbnail_path;

  GRL_DEBUG ("got_file_info");

  info = g_file_query_info_finish (file, result, &error);
  if (error)
    goto error;

  thumbnail_path =
      g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);


  if (thumbnail_path) {
    gchar *thumbnail_uri = g_filename_to_uri (thumbnail_path, NULL, &error);
    if (error)
      goto error;

    GRL_INFO ("Got thumbnail %s for media: %s", thumbnail_uri,
              grl_media_get_url (rs->media));
    grl_media_set_thumbnail (rs->media, thumbnail_uri);
    g_free (thumbnail_uri);

    rs->callback (rs->source, rs->media, rs->user_data, NULL);
  } else {
    GRL_INFO ("Could not find thumbnail for media: %s",
              grl_media_get_url (rs->media));
    rs->callback (rs->source, rs->media, rs->user_data, NULL);
  }

  goto exit;

error:
    {
      GError *new_error = g_error_new (GRL_CORE_ERROR, GRL_CORE_ERROR_RESOLVE_FAILED,
                                       "Got error: %s", error->message);
      rs->callback (rs->source, rs->media, rs->user_data, new_error);

      g_error_free (error);
      g_error_free (new_error);
    }

exit:
  if (info)
    g_object_unref (info);
}

static void
resolve_image (GrlMetadataSourceResolveSpec *rs)
{
  GFile *file;

  GRL_DEBUG ("resolve_image");

  file = g_file_new_for_uri (grl_media_get_url (rs->media));

  g_file_query_info_async (file, G_FILE_ATTRIBUTE_THUMBNAIL_PATH,
                           G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT, NULL,
                           (GAsyncReadyCallback)got_file_info, rs);
}

static void
resolve_album_art (GrlMetadataSourceResolveSpec *rs)
{
  /* FIXME: implement this, according to
   * http://live.gnome.org/MediaArtStorageSpec */
  GError *error;
  error = g_error_new (GRL_CORE_ERROR, GRL_CORE_ERROR_RESOLVE_FAILED,
    "Thumbnail resolution for GrlMediaAudio not implemented in local-metadata");
  rs->callback (rs->source, rs->media, rs->user_data, error);
  g_error_free (error);
}

/* ================== API Implementation ================ */

static const GList *
grl_local_metadata_source_supported_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_THUMBNAIL,
                                      NULL);
  }
  return keys;
}

static const GList *
grl_local_metadata_source_key_depends (GrlMetadataSource *source,
                                       GrlKeyID key_id)
{
  static GList *deps = NULL;
  if (!deps) {
    deps = grl_metadata_key_list_new (GRL_METADATA_KEY_URL, NULL);
  }

  if (key_id == GRL_METADATA_KEY_THUMBNAIL)
    return deps;

  return NULL;
}

static void
grl_local_metadata_source_resolve (GrlMetadataSource *source,
                                  GrlMetadataSourceResolveSpec *rs)
{
  const gchar *url;
  gchar *scheme;
  GError *error = NULL;

  GRL_DEBUG ("grl_local_metadata_source_resolve");

  url = grl_media_get_url (rs->media);
  scheme = g_uri_parse_scheme (url);

  if (0 != g_strcmp0 (scheme, "file"))
    error = g_error_new (GRL_CORE_ERROR, GRL_CORE_ERROR_RESOLVE_FAILED,
                         "local-metadata needs a url in the file:// scheme");
  else if (!g_list_find (rs->keys, GRL_METADATA_KEY_THUMBNAIL))
    error = g_error_new (GRL_CORE_ERROR, GRL_CORE_ERROR_RESOLVE_FAILED,
                         "local-metadata can only resolve the thumbnail key");

  if (error) {
    /* No can do! */
    rs->callback (source, rs->media, rs->user_data, error);
    g_error_free (error);
    goto exit;
  }

  if (GRL_IS_MEDIA_VIDEO (rs->media)
      || GRL_IS_MEDIA_IMAGE (rs->media)) {
    resolve_image (rs);
  } else if (GRL_IS_MEDIA_AUDIO (rs->media)) {
    resolve_album_art (rs);
  } else {
    /* What's that media type? */
    rs->callback (source, rs->media, rs->user_data, NULL);
  }

exit:
  g_free (scheme);
}

