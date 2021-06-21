/*
 * Copyright (C) 2016 Grilo Project
 *
 * Contact: Victor Toso <me@victortoso.com>
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

#include <gst/gst.h>
#include <glib/gi18n-lib.h>

#include "grl-chromaprint.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT chromaprint_log_domain
GRL_LOG_DOMAIN_STATIC (chromaprint_log_domain);

/* --- Plugin information --- */

#define SOURCE_ID     "grl-chromaprint"
#define SOURCE_NAME   "Chromaprint"
#define SOURCE_DESC   _("A plugin to get metadata using gstreamer framework")


/* --- chromaprint keys  --- */

static GrlKeyID GRL_CHROMAPRINT_METADATA_KEY_FINGERPRINT = GRL_METADATA_KEY_INVALID;

/* GStreamer Elements */
#define GST_BIN_AUDIO           "grl-gst-audiobin"
#define GST_ELEMENT_CHROMAPRINT "grl-gst-chromaprint"

struct _GrlChromaprintPrivate {
  GList *supported_keys;
};

typedef struct _OperationSpec {
  GrlSource  *source;
  guint       operation_id;
  GList      *keys;
  GrlMedia   *media;
  gpointer    user_data;
  gint        duration;
  gchar      *fingerprint;
  GstElement *pipeline;
  GrlSourceResolveCb callback;
} OperationSpec;

/* Copied from gst-plugins-base/gst/playback/gstplay-enum.h */
typedef enum
{
  GST_PLAY_FLAG_VIDEO = (1 << 0),
  GST_PLAY_FLAG_AUDIO = (1 << 1),
  GST_PLAY_FLAG_TEXT = (1 << 2),
  GST_PLAY_FLAG_VIS = (1 << 3),
  GST_PLAY_FLAG_SOFT_VOLUME = (1 << 4),
  GST_PLAY_FLAG_NATIVE_AUDIO = (1 << 5),
  GST_PLAY_FLAG_NATIVE_VIDEO = (1 << 6),
  GST_PLAY_FLAG_DOWNLOAD = (1 << 7),
  GST_PLAY_FLAG_BUFFERING = (1 << 8),
  GST_PLAY_FLAG_DEINTERLACE = (1 << 9),
  GST_PLAY_FLAG_SOFT_COLORBALANCE = (1 << 10),
  GST_PLAY_FLAG_FORCE_FILTERS = (1 << 11),
} GstPlayFlags;

static const GList *grl_chromaprint_source_supported_keys (GrlSource *source);

static gboolean grl_chromaprint_source_may_resolve (GrlSource *source,
                                                    GrlMedia  *media,
                                                    GrlKeyID   key_id,
                                                    GList    **missing_keys);

static void grl_chromaprint_source_resolve (GrlSource            *source,
                                            GrlSourceResolveSpec *rs);

static void grl_chromaprint_source_finalize (GObject *object);

static GrlChromaprintSource* grl_chromaprint_source_new (void);

/* ================== Chromaprint Plugin  ================= */

static gboolean
grl_chromaprint_plugin_init (GrlRegistry *registry,
                             GrlPlugin   *plugin,
                             GList       *configs)
{
  GrlChromaprintSource *source;

  GRL_LOG_DOMAIN_INIT (chromaprint_log_domain, "chromaprint");

  GRL_DEBUG ("chromaprint_plugin_init");

  gst_init (NULL, NULL);

  source = grl_chromaprint_source_new ();
  grl_registry_register_source (registry,
                                plugin,
                                GRL_SOURCE (source),
                                NULL);
  return TRUE;
}

static void
grl_chromaprint_plugin_register_keys (GrlRegistry *registry,
                                      GrlPlugin   *plugin)
{
  GParamSpec *spec;

  spec = g_param_spec_string ("chromaprint",
                              "chromaprint",
                              "The fingerprint of the audio.",
                              NULL,
                              G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE),
  GRL_CHROMAPRINT_METADATA_KEY_FINGERPRINT =
    grl_registry_register_metadata_key (registry, spec, GRL_METADATA_KEY_INVALID, NULL);
}

GRL_PLUGIN_DEFINE (GRL_MAJOR,
                   GRL_MINOR,
                   SOURCE_ID,
                   SOURCE_NAME,
                   "A plugin to get metadata using chromaprint framework",
                   "Victor Toso",
                   VERSION,
                   "LGPL-2.1-or-later",
                   "http://victortoso.com",
                   grl_chromaprint_plugin_init,
                   NULL,
                   grl_chromaprint_plugin_register_keys);

/* ================== Chromaprint GObject ================= */

G_DEFINE_TYPE_WITH_PRIVATE (GrlChromaprintSource, grl_chromaprint_source, GRL_TYPE_SOURCE)

static GrlChromaprintSource *
grl_chromaprint_source_new ()
{
  GObject *object;
  GrlChromaprintSource *source;

  GRL_DEBUG ("chromaprint_source_new");

  object = g_object_new (GRL_CHROMAPRINT_SOURCE_TYPE,
                         "source-id", SOURCE_ID,
                         "source-name", SOURCE_NAME,
                         "source-desc", SOURCE_DESC,
                         "supported-media", GRL_SUPPORTED_MEDIA_AUDIO,
                         NULL);

  source = GRL_CHROMAPRINT_SOURCE (object);
  return source;
}

static void
grl_chromaprint_source_class_init (GrlChromaprintSourceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

  source_class->supported_keys = grl_chromaprint_source_supported_keys;
  source_class->may_resolve = grl_chromaprint_source_may_resolve;
  source_class->resolve = grl_chromaprint_source_resolve;
  gobject_class->finalize = grl_chromaprint_source_finalize;
}

static void
grl_chromaprint_source_init (GrlChromaprintSource *source)
{
  GRL_DEBUG ("chromaprint_source_init");

  source->priv = grl_chromaprint_source_get_instance_private (source);

  /* All supported keys in a GList */
  source->priv->supported_keys =
    grl_metadata_key_list_new (GRL_CHROMAPRINT_METADATA_KEY_FINGERPRINT,
                               GRL_METADATA_KEY_DURATION,
                               GRL_METADATA_KEY_INVALID);
}

static void
grl_chromaprint_source_finalize (GObject *object)
{
  GrlChromaprintSource *source;

  GRL_DEBUG ("grl_chromaprint_source_finalize");

  source = GRL_CHROMAPRINT_SOURCE (object);

  g_list_free (source->priv->supported_keys);

  G_OBJECT_CLASS (grl_chromaprint_source_parent_class)->finalize (object);
}

/* ======================= Utilities ==================== */
static gchar *
get_uri_to_file (const gchar *url)
{
  GFile *file;
  gchar *uri;

  file = g_file_new_for_commandline_arg (url);
  uri = g_file_get_uri (file);
  g_object_unref (file);
  return uri;
}

static void
free_operation_spec (OperationSpec *os)
{
  g_list_free (os->keys);
  g_clear_pointer (&os->fingerprint, g_free);
  g_slice_free (OperationSpec, os);
}

static void
chromaprint_build_media (OperationSpec *os)
{
  GList *it;
  gint missing_keys = 0;

  for (it = os->keys; it != NULL; it = it->next) {
    GrlKeyID key_id = GRLPOINTER_TO_KEYID (it->data);
    switch (key_id) {
    case GRL_METADATA_KEY_DURATION:
      grl_media_set_duration (os->media, os->duration);
      break;

    default:
      if (key_id == GRL_CHROMAPRINT_METADATA_KEY_FINGERPRINT) {
        grl_data_set_string (GRL_DATA (os->media),
                             GRL_CHROMAPRINT_METADATA_KEY_FINGERPRINT,
                             os->fingerprint);

      } else {
        missing_keys++;
      }
    }
  }

  if (missing_keys > 0) {
    GRL_DEBUG ("Operation-id %d missed %d keys",
               os->operation_id, missing_keys);
  }
}


static void
chromaprint_gstreamer_done (OperationSpec *os)
{
  if (os->fingerprint == NULL)
    goto resolve_end;

  GRL_DEBUG ("duration: %d", os->duration);
  GRL_DEBUG ("fingerprint: %s", os->fingerprint);
  chromaprint_build_media (os);

resolve_end:
  os->callback (os->source, os->operation_id, os->media, os->user_data, NULL);
  free_operation_spec (os);
}


static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    user_data)
{
  OperationSpec *os;

  os = (OperationSpec *) user_data;

  switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS: {
      gint64 len;
      guint  seconds;
      gchar *str;
      GstElement *chromaprint;

      chromaprint = gst_bin_get_by_name (GST_BIN (os->pipeline),
                                         GST_ELEMENT_CHROMAPRINT);
      g_object_get (G_OBJECT (chromaprint), "fingerprint", &str, NULL);
      gst_element_query_duration (os->pipeline, GST_FORMAT_TIME, &len);
      seconds = GST_TIME_AS_SECONDS (len);

      gst_element_set_state (os->pipeline, GST_STATE_NULL);
      gst_object_unref (GST_OBJECT (os->pipeline));
      gst_object_unref (GST_OBJECT (chromaprint));

      os->duration = seconds;
      os->fingerprint = str;
      chromaprint_gstreamer_done (os);
      return FALSE;
    }

    case GST_MESSAGE_ERROR: {
      gchar  *debug;
      GError *error;

      gst_message_parse_error (msg, &error, &debug);
      g_free (debug);

      GRL_DEBUG ("Error: %s\n", error->message);
      g_error_free (error);

      gst_element_set_state (os->pipeline, GST_STATE_NULL);
      gst_object_unref (GST_OBJECT (os->pipeline));
      chromaprint_gstreamer_done (os);
      return FALSE;
    }

    default:
      break;
  }

  return TRUE;
}

static void
chromaprint_execute_resolve (OperationSpec *os)
{
  GstElement *playbin, *sink, *chromaprint;
  GstBus *bus;
  gchar *uri;
  gint flags;

  uri = get_uri_to_file (grl_media_get_url (os->media));

  /* Create the elemtens */
  playbin = gst_element_factory_make ("playbin", "playbin");
  if (playbin == NULL) {
      GRL_WARNING ("error upon creation of 'playbin' element");
      goto err_playbin;
  }

  sink = gst_element_factory_make ("fakesink", "sink");
  if (sink == NULL) {
      GRL_WARNING ("error upon creation of 'fakesink' element");
      goto err_sink;
  }

  chromaprint = gst_element_factory_make ("chromaprint", GST_ELEMENT_CHROMAPRINT);
  if (chromaprint == NULL) {
      GRL_WARNING ("error upon creation of 'chromaprint' element");
      goto err_chromaprint;
  }

  g_object_set (playbin,
                "uri", uri,
                "audio-filter", chromaprint,
                "audio-sink", sink,
                NULL);
  g_free (uri);

  /* Disable video from playbin */
  g_object_get (playbin, "flags", &flags, NULL);
  flags &= ~GST_PLAY_FLAG_VIDEO;
  g_object_set (playbin, "flags", flags, NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (playbin));
  gst_bus_add_watch (bus, bus_call, os);
  gst_object_unref (bus);

  os->pipeline = playbin;
  gst_element_set_state (playbin, GST_STATE_PLAYING);
  return;

err_chromaprint:
  gst_object_unref (chromaprint);
err_sink:
  gst_object_unref (sink);
err_playbin:
  gst_object_unref (playbin);
  g_clear_pointer (&uri, g_free);

  os->callback (os->source, os->operation_id, os->media, os->user_data, NULL);
  free_operation_spec (os);
}


/* ================== API Implementation ================ */
static void
grl_chromaprint_source_resolve (GrlSource            *source,
                                GrlSourceResolveSpec *rs)
{
  OperationSpec *os = NULL;

  GRL_DEBUG ("chromaprint_resolve");

  os = g_slice_new0 (OperationSpec);
  os->source = rs->source;
  os->operation_id = rs->operation_id;
  os->keys = g_list_copy (rs->keys);
  os->callback = rs->callback;
  os->media = rs->media;
  os->user_data = rs->user_data;

  /* FIXME: here we should resolve depending on media type (audio/video) */
  chromaprint_execute_resolve (os);
}

static gboolean
grl_chromaprint_source_may_resolve (GrlSource *source,
                                    GrlMedia  *media,
                                    GrlKeyID   key_id,
                                    GList    **missing_keys)
{
  GrlChromaprintSource *gst_source = GRL_CHROMAPRINT_SOURCE (source);
  gchar *uri;

  GRL_DEBUG ("chromaprint_may_resolve");

  /* Check if this key is supported */
  if (!g_list_find (gst_source->priv->supported_keys,
                    GRLKEYID_TO_POINTER (key_id)))
    return FALSE;

  /* Check if resolve type and media type match */
  if (media && !grl_media_is_audio (media))
    return FALSE;

  /* Check if the media has an url to a valid name */
  if (!media || !grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_URL)) {
    if (missing_keys)
      *missing_keys = grl_metadata_key_list_new (GRL_METADATA_KEY_URL, NULL);

    return FALSE;
  }

  uri = get_uri_to_file (grl_media_get_url (media));
  if (uri == NULL)
    return FALSE;

  g_free (uri);
  return TRUE;
}

static const GList *
grl_chromaprint_source_supported_keys (GrlSource *source)
{
  GrlChromaprintSource *gst_source = GRL_CHROMAPRINT_SOURCE (source);

  return gst_source->priv->supported_keys;
}
