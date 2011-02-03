/*
 * Copyright (C) 2010, 2011 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
 *
 * This component is based on Maemo's mafw-upnp-source source code.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
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
#include <libgupnp/gupnp.h>
#include <libgupnp-av/gupnp-av.h>
#include <string.h>
#include <stdlib.h>
#include <libxml/tree.h>

#include "grl-upnp.h"

#define GRL_UPNP_GET_PRIVATE(object)                                    \
  (G_TYPE_INSTANCE_GET_PRIVATE((object), GRL_UPNP_SOURCE_TYPE, GrlUpnpPrivate))

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT upnp_log_domain
GRL_LOG_DOMAIN_STATIC(upnp_log_domain);

/* --- Plugin information --- */

#define PLUGIN_ID   UPNP_PLUGIN_ID

#define SOURCE_ID_TEMPLATE    "grl-upnp-%s"
#define SOURCE_NAME_TEMPLATE  "UPnP - %s"
#define SOURCE_DESC_TEMPLATE  "A source for browsing the UPnP server '%s'"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

/* --- Other --- */

#ifndef CONTENT_DIR_SERVICE
#define CONTENT_DIR_SERVICE "urn:schemas-upnp-org:service:ContentDirectory"
#endif

#define UPNP_SEARCH_SPEC				\
  "dc:title contains \"%s\" or "			\
  "upnp:album contains \"%s\" or "			\
  "upnp:artist contains \"%s\""

struct _GrlUpnpPrivate {
  GUPnPDeviceProxy* device;
  GUPnPServiceProxy* service;
  GUPnPControlPoint *cp;
  gboolean search_enabled;
  gchar *upnp_name;
};

struct OperationSpec {
  GrlMediaSource *source;
  guint operation_id;
  GList *keys;
  guint skip;
  guint count;
  GrlMediaSourceResultCb callback;
  gpointer user_data;
};

struct SourceInfo {
  gchar *source_id;
  gchar *source_name;
  GUPnPDeviceProxy* device;
  GUPnPServiceProxy* service;
  GrlPluginInfo *plugin;
};

static void setup_key_mappings (void);

static gchar *build_source_id (const gchar *udn);

static GrlUpnpSource *grl_upnp_source_new (const gchar *id, const gchar *name);

gboolean grl_upnp_plugin_init (GrlPluginRegistry *registry,
                               const GrlPluginInfo *plugin,
                               GList *configs);

static void grl_upnp_source_finalize (GObject *plugin);

static const GList *grl_upnp_source_supported_keys (GrlMetadataSource *source);

static void grl_upnp_source_browse (GrlMediaSource *source,
                                    GrlMediaSourceBrowseSpec *bs);

static void grl_upnp_source_search (GrlMediaSource *source,
                                    GrlMediaSourceSearchSpec *ss);

static void grl_upnp_source_query (GrlMediaSource *source,
                                   GrlMediaSourceQuerySpec *qs);

static void grl_upnp_source_metadata (GrlMediaSource *source,
                                      GrlMediaSourceMetadataSpec *ms);

static GrlSupportedOps grl_upnp_source_supported_operations (GrlMetadataSource *source);

static void context_available_cb (GUPnPContextManager *context_manager,
				  GUPnPContext *context,
				  gpointer user_data);
static void device_available_cb (GUPnPControlPoint *cp,
				 GUPnPDeviceProxy *device,
				 gpointer user_data);
static void device_unavailable_cb (GUPnPControlPoint *cp,
				   GUPnPDeviceProxy *device,
				   gpointer user_data);

/* ===================== Globals  ================= */

static GHashTable *key_mapping = NULL;
static GHashTable *filter_key_mapping = NULL;
static GUPnPContextManager *context_manager = NULL;

/* =================== UPnP Plugin  =============== */

gboolean
grl_upnp_plugin_init (GrlPluginRegistry *registry,
                      const GrlPluginInfo *plugin,
                      GList *configs)
{
  GRL_LOG_DOMAIN_INIT (upnp_log_domain, "upnp");

  GRL_DEBUG ("grl_upnp_plugin_init");

  /* libsoup needs this */
  if (!g_thread_supported()) {
    g_thread_init (NULL);
  }

  context_manager = gupnp_context_manager_new (NULL, 0);
  g_signal_connect (context_manager,
                    "context-available",
                    G_CALLBACK (context_available_cb),
                    (gpointer)plugin);

  return TRUE;
}

static void
grl_upnp_plugin_deinit (void)
{
  GRL_DEBUG ("grl_upnp_plugin_deinit");

  if (context_manager != NULL) {
    g_object_unref (context_manager);
    context_manager = NULL;
  }
}

GRL_PLUGIN_REGISTER (grl_upnp_plugin_init,
                     grl_upnp_plugin_deinit,
                     PLUGIN_ID);

/* ================== UPnP GObject ================ */

G_DEFINE_TYPE (GrlUpnpSource, grl_upnp_source, GRL_TYPE_MEDIA_SOURCE);

static GrlUpnpSource *
grl_upnp_source_new (const gchar *source_id, const gchar *name)
{
  gchar *source_name, *source_desc;
  GrlUpnpSource *source;

  GRL_DEBUG ("grl_upnp_source_new");
  source_name = g_strdup_printf (SOURCE_NAME_TEMPLATE, name);
  source_desc = g_strdup_printf (SOURCE_DESC_TEMPLATE, name);

  source = g_object_new (GRL_UPNP_SOURCE_TYPE,
			 "source-id", source_id,
			 "source-name", source_name,
			 "source-desc", source_desc,
			 NULL);

  source->priv->upnp_name = g_strdup (name);

  g_free (source_name);
  g_free (source_desc);

  return source;
}

static void
grl_upnp_source_class_init (GrlUpnpSourceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GrlMediaSourceClass *source_class = GRL_MEDIA_SOURCE_CLASS (klass);
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);

  gobject_class->finalize = grl_upnp_source_finalize;

  metadata_class->supported_keys = grl_upnp_source_supported_keys;
  metadata_class->supported_operations = grl_upnp_source_supported_operations;

  source_class->browse = grl_upnp_source_browse;
  source_class->search = grl_upnp_source_search;
  source_class->query = grl_upnp_source_query;
  source_class->metadata = grl_upnp_source_metadata;

  g_type_class_add_private (klass, sizeof (GrlUpnpPrivate));

  setup_key_mappings ();
}

static void
grl_upnp_source_init (GrlUpnpSource *source)
{
  source->priv = GRL_UPNP_GET_PRIVATE (source);
}

static void
grl_upnp_source_finalize (GObject *object)
{
  GrlUpnpSource *source;

  GRL_DEBUG ("grl_upnp_source_finalize");

  source = GRL_UPNP_SOURCE (object);

  g_object_unref (source->priv->device);
  g_object_unref (source->priv->service);
  g_free (source->priv->upnp_name);

  G_OBJECT_CLASS (grl_upnp_source_parent_class)->finalize (object);
}

/* ======================= Utilities ==================== */

static gchar *
build_source_id (const gchar *udn)
{
  return g_strdup_printf (SOURCE_ID_TEMPLATE, udn);
}

static void
free_source_info (struct SourceInfo *info)
{
  g_free (info->source_id);
  g_free (info->source_name);
  g_object_unref (info->device);
  g_object_unref (info->service);
  g_slice_free (struct SourceInfo, info);
}

static void
gupnp_search_caps_cb (GUPnPServiceProxy *service,
		      GUPnPServiceProxyAction *action,
		      gpointer user_data)
{
  GError *error = NULL;
  gchar *caps = NULL;
  gchar *name;
  GrlUpnpSource *source;
  gchar *source_id;
  GrlPluginRegistry *registry;
  struct SourceInfo *source_info;
  gboolean result;

  result =
    gupnp_service_proxy_end_action (service, action, &error,
				    "SearchCaps", G_TYPE_STRING, &caps,
				    NULL);
  if (!result) {
    GRL_WARNING ("Failed to execute GetSearchCaps operation");
    if (error) {
      GRL_WARNING ("Reason: %s", error->message);
      g_error_free (error);
    }
  }

  source_info = (struct SourceInfo *) user_data;
  name = source_info->source_name;
  source_id = source_info->source_id;

  registry = grl_plugin_registry_get_default ();
  if (grl_plugin_registry_lookup_source (registry, source_id)) {
    GRL_DEBUG ("A source with id '%s' is already registered. Skipping...",
               source_id);
    goto free_resources;
  }

  source = grl_upnp_source_new (source_id, name);
  source->priv->device = g_object_ref (source_info->device);
  source->priv->service = g_object_ref (source_info->service);

  GRL_DEBUG ("Search caps for source '%s': '%s'", name, caps);

  if (caps && caps[0] != '\0') {
    GRL_DEBUG ("Setting search enabled for source '%s'", name );
    source->priv->search_enabled = TRUE;
  } else {
    GRL_DEBUG ("Setting search disabled for source '%s'", name );
  }

  grl_plugin_registry_register_source (registry,
                                       source_info->plugin,
                                       GRL_MEDIA_PLUGIN (source),
                                       NULL);

 free_resources:
  free_source_info (source_info);
}

static void
context_available_cb (GUPnPContextManager *context_manager,
		      GUPnPContext *context,
		      gpointer user_data)
{
  GUPnPControlPoint *cp;

  GRL_DEBUG ("%s", __func__);

  cp = gupnp_control_point_new (context, "urn:schemas-upnp-org:device:MediaServer:1");
  g_signal_connect (cp,
		    "device-proxy-available",
		    G_CALLBACK (device_available_cb),
		    user_data);
  g_signal_connect (cp,
		    "device-proxy-unavailable",
		    G_CALLBACK (device_unavailable_cb),
		    NULL);

  gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);

  /* Let context manager take care of the control point life cycle */
  gupnp_context_manager_manage_control_point (context_manager, cp);
  g_object_unref (cp);
}

static void
device_available_cb (GUPnPControlPoint *cp,
		     GUPnPDeviceProxy *device,
		     gpointer user_data)
{
  gchar* name;
  const gchar* udn;
  const char *type;
  GUPnPServiceInfo *service;
  GrlPluginRegistry *registry;
  gchar *source_id;

  GRL_DEBUG ("device_available_cb");

  type = gupnp_device_info_get_device_type (GUPNP_DEVICE_INFO (device));
  GRL_DEBUG ("  type: %s", type);

  service = gupnp_device_info_get_service (GUPNP_DEVICE_INFO (device),
					   CONTENT_DIR_SERVICE);
  if (!service) {
    GRL_DEBUG ("Device does not provide required service, ignoring...");
    return;
  }

  udn = gupnp_device_info_get_udn (GUPNP_DEVICE_INFO (device));
  GRL_DEBUG ("   udn: %s ", udn);

  name = gupnp_device_info_get_friendly_name (GUPNP_DEVICE_INFO (device));
  GRL_DEBUG ("  name: %s", name);

  registry = grl_plugin_registry_get_default ();
  source_id = build_source_id (udn);
  if (grl_plugin_registry_lookup_source (registry, source_id)) {
    GRL_DEBUG ("A source with id '%s' is already registered. Skipping...",
               source_id);
    goto free_resources;
  }

  /* We got a valid UPnP source */
  /* Now let's check if it supports search operations before registering */
  struct SourceInfo *source_info = g_slice_new0 (struct SourceInfo);
  source_info->source_id = g_strdup (source_id);
  source_info->source_name = g_strdup (name);
  source_info->device = g_object_ref (device);
  source_info->service = g_object_ref (service);
  source_info->plugin = (GrlPluginInfo *) user_data;

  if (!gupnp_service_proxy_begin_action (GUPNP_SERVICE_PROXY (service),
					 "GetSearchCapabilities",
					 gupnp_search_caps_cb,
					 source_info,
					 NULL)) {
    GrlUpnpSource *source = grl_upnp_source_new (source_id, name);
    GRL_WARNING ("Failed to start GetCapabilitiesSearch action");
    GRL_DEBUG ("Setting search disabled for source '%s'", name );
    registry = grl_plugin_registry_get_default ();
    grl_plugin_registry_register_source (registry,
                                         source_info->plugin,
                                         GRL_MEDIA_PLUGIN (source),
                                         NULL);
    free_source_info (source_info);
  }

 free_resources:
  g_object_unref (service);
  g_free (source_id);
}

static void
device_unavailable_cb (GUPnPControlPoint *cp,
		       GUPnPDeviceProxy *device,
		       gpointer user_data)
{
  const gchar* udn;
  GrlMediaPlugin *source;
  GrlPluginRegistry *registry;
  gchar *source_id;

  GRL_DEBUG ("device_unavailable_cb");

  udn = gupnp_device_info_get_udn (GUPNP_DEVICE_INFO (device));
  GRL_DEBUG ("   udn: %s ", udn);

  registry = grl_plugin_registry_get_default ();
  source_id = build_source_id (udn);
  source = grl_plugin_registry_lookup_source (registry, source_id);
  if (!source) {
    GRL_DEBUG ("No source registered with id '%s', ignoring", source_id);
  } else {
    grl_plugin_registry_unregister_source (registry, source, NULL);
  }

  g_free (source_id);
}

const static gchar *
get_upnp_key (const GrlKeyID key_id)
{
  return g_hash_table_lookup (key_mapping, key_id);
}

const static gchar *
get_upnp_key_for_filter (const GrlKeyID key_id)
{
  return g_hash_table_lookup (filter_key_mapping, key_id);
}

static gchar *
get_upnp_filter (const GList *keys)
{
  GString *filter;
  GList *iter;
  gchar *upnp_key;
  guint first = TRUE;

  filter = g_string_new ("");
  iter = (GList *) keys;
  while (iter) {
    upnp_key =
      (gchar *) get_upnp_key_for_filter (iter->data);
    if (upnp_key) {
      if (!first) {
	g_string_append (filter, ",");
      }
      g_string_append (filter, upnp_key);
      first = FALSE;
    }
    iter = g_list_next (iter);
  }

  return g_string_free (filter, FALSE);
}

static gchar *
get_upnp_search (const gchar *text)
{
  return g_strdup_printf (UPNP_SEARCH_SPEC, text, text, text);
}

static void
setup_key_mappings (void)
{
  /* For key_mapping we only have to set mapping for keys that
     are not handled directly with the corresponding fw key
     (see ket_valur_for_key) */
  key_mapping = g_hash_table_new (g_direct_hash, g_direct_equal);
  filter_key_mapping = g_hash_table_new (g_direct_hash, g_direct_equal);

  g_hash_table_insert (key_mapping, GRL_METADATA_KEY_TITLE, "title");
  g_hash_table_insert (key_mapping, GRL_METADATA_KEY_ARTIST, "artist");
  g_hash_table_insert (key_mapping, GRL_METADATA_KEY_ALBUM, "album");
  g_hash_table_insert (key_mapping, GRL_METADATA_KEY_GENRE, "genre");
  g_hash_table_insert (key_mapping, GRL_METADATA_KEY_URL, "res");
  g_hash_table_insert (key_mapping, GRL_METADATA_KEY_DATE, "modified");
  g_hash_table_insert (filter_key_mapping, GRL_METADATA_KEY_TITLE, "title");
  g_hash_table_insert (filter_key_mapping, GRL_METADATA_KEY_URL, "res");
  g_hash_table_insert (filter_key_mapping, GRL_METADATA_KEY_DATE, "modified");
  g_hash_table_insert (filter_key_mapping, GRL_METADATA_KEY_ARTIST, "upnp:artist");
  g_hash_table_insert (filter_key_mapping, GRL_METADATA_KEY_ALBUM, "upnp:album");
  g_hash_table_insert (filter_key_mapping, GRL_METADATA_KEY_GENRE, "upnp:genre");
  g_hash_table_insert (filter_key_mapping, GRL_METADATA_KEY_DURATION, "res@duration");
  g_hash_table_insert (filter_key_mapping, GRL_METADATA_KEY_DATE, "modified");
}

static gchar *
didl_res_get_protocol_info (xmlNode* res_node, gint field)
{
  gchar* pinfo;
  gchar* value;
  gchar** array;

  pinfo = (gchar *) xmlGetProp (res_node, (const xmlChar *) "protocolInfo");
  if (pinfo == NULL) {
    return NULL;
  }

  /* 0:protocol, 1:network, 2:mime-type and 3:additional info. */
  array = g_strsplit (pinfo, ":", 4);
  g_free(pinfo);
  if (g_strv_length (array) < 4) {
    value = NULL;
  } else {
    value = g_strdup (array[field]);
  }

  g_strfreev (array);

  return value;
}

static GList *
didl_get_supported_resources (GUPnPDIDLLiteObject *didl)
{
  GList *properties, *node;
  xmlNode *xml_node;
  gchar *protocol;

  properties = gupnp_didl_lite_object_get_properties (didl, "res");

  node = properties;
  while (node) {
    xml_node = (xmlNode *) node->data;
    if (!xml_node) {
      node = properties = g_list_delete_link (properties, node);
      continue;
    }

    protocol = didl_res_get_protocol_info (xml_node, 0);
    if (protocol && strcmp (protocol, "http-get") != 0) {
      node = properties = g_list_delete_link (properties, node);
      g_free (protocol);
      continue;
    }
    g_free (protocol);
    node = g_list_next (node);
  }

  return properties;
}

static gint
didl_h_mm_ss_to_int (const gchar *time)
{
  guint len = 0;
  guint i = 0;
  guint head = 0;
  guint tail = 0;
  int result = 0;
  gchar* tmp = NULL;
  gboolean has_hours = FALSE;

  if (!time) {
    return -1;
  }

  len = strlen (time);
  tmp = g_new0 (gchar, sizeof (gchar) * len);

  /* Find the first colon (it can be anywhere) and also count the
   * amount of colons to know if there are hours or not */
  for (i = 0; i < len; i++) {
    if (time[i] == ':') {
      if (tail != 0) {
	has_hours = TRUE;
      } else {
	tail = i;
      }
    }
  }

  if (tail > len || head > tail) {
    g_free (tmp);
    return -1;
  }

  /* Hours */
  if (has_hours == TRUE) {
    memcpy (tmp, time + head, tail - head);
    tmp[tail - head + 1] = '\0';
    result += 3600 * atoi (tmp);
    /* The next colon should be exactly 2 chars right */
    head = tail + 1;
    tail = head + 2;
  } else {
    /* The format is now exactly MM:SS */
    head = 0;
    tail = 2;
  }

  /* Bail out if tail goes too far or head is bigger than tail */
  if (tail > len || head > tail) {
    g_free (tmp);
    return -1;
  }

  /* Minutes */
  memcpy (tmp, time + head, tail - head);
  tmp[2] = '\0';
  result += 60 * atoi (tmp);

  /* The next colon should again be exactly 2 chars right */
  head = tail + 1;
  tail = head + 2;

  /* Bail out if tail goes too far or head is bigger than tail */
  if (tail > len || head > tail) {
    g_free (tmp);
    return -1;
  }

  /* Extract seconds */
  memcpy (tmp, time + head, tail - head);
  tmp[2] = '\0';
  result += atoi(tmp);
  g_free (tmp);

  return result;

}

static gboolean
is_image (xmlNode *node)
{
  gchar *mime_type;
  gboolean ret;

  mime_type = didl_res_get_protocol_info (node, 2);
  ret = g_str_has_prefix (mime_type, "image/");

  g_free (mime_type);
  return ret;
}

static gboolean
is_http_get (xmlNode *node)
{
  gboolean ret;
  gchar *protocol;

  protocol = didl_res_get_protocol_info (node, 0);
  ret = g_str_has_prefix (protocol, "http-get");

  g_free (protocol);
  return ret;
}

static gboolean
has_thumbnail_marker (xmlNode *node)
{
  gchar *dlna_stuff;
  gboolean ret;

  dlna_stuff = didl_res_get_protocol_info (node, 3);
  ret = strstr("JPEG_TN", dlna_stuff) != NULL;

  g_free (dlna_stuff);
  return ret;
}

static gchar *
get_thumbnail (GList *nodes)
{
  GList *element;
  gchar *val = NULL;
  guint counter = 0;

  /* chose, depending on availability, the first with DLNA.ORG_PN=JPEG_TN, or
   * the last http-get with mimetype image/something if there is more than one
   * http-get.
   * This covers at least mediatomb and rygel.
   * This could be improved by handling resolution and/or size */

  for (element=nodes; element; element=g_list_next (element)) {
    xmlNode *node = (xmlNode *)element->data;

    if (is_http_get (node)) {
      counter++;
      if (is_image (node)) {
        if (val)
          g_free (val);
        val = (gchar *) xmlNodeGetContent (node);

        if (has_thumbnail_marker (node))  /* that's definitely it! */
          return val;
      }
    }
  }

  if (val && counter == 1) {
    /* There was only one element with http-get protocol: that's the uri of the
     * media itself, not a thumbnail */
    g_free (val);
    val = NULL;
  }

  return val;
}

static gchar *
get_value_for_key (GrlKeyID key_id,
                   GUPnPDIDLLiteObject *didl,
                   GList *props)
{
  GList* list;
  gchar* val = NULL;
  const gchar* upnp_key;

  xmlNode *didl_node = gupnp_didl_lite_object_get_xml_node (didl);
  upnp_key = get_upnp_key (key_id);

  if (key_id == GRL_METADATA_KEY_CHILDCOUNT) {
    val = (gchar *) xmlGetProp (didl_node,
                                (const xmlChar *) "childCount");
  } else if (key_id == GRL_METADATA_KEY_MIME && props) {
    val = didl_res_get_protocol_info ((xmlNode *) props->data, 2);
  } else if (key_id == GRL_METADATA_KEY_DURATION && props) {
    val = (gchar *) xmlGetProp ((xmlNodePtr) props->data,
                                (const xmlChar *) "duration");
  } else if (key_id == GRL_METADATA_KEY_URL && props) {
    val = (gchar *) xmlNodeGetContent ((xmlNode *) props->data);
  } else if (key_id == GRL_METADATA_KEY_THUMBNAIL && props) {
    val = g_strdup (gupnp_didl_lite_object_get_album_art (didl));
    if (!val)
      val = get_thumbnail (props);
  } else if (upnp_key) {
    list = gupnp_didl_lite_object_get_properties (didl, upnp_key);
    if (list) {
      val = (gchar *) xmlNodeGetContent ((xmlNode*) list->data);
      g_list_free (list);
    } else if (props && props->data) {
      val = (gchar *) xmlGetProp ((xmlNodePtr) props->data,
                                  (const xmlChar *) upnp_key);
    }
  }

  return val;
}

static void
set_metadata_value (GrlMedia *media,
                    GrlKeyID key_id,
                    const gchar *value)
{
  if (key_id == GRL_METADATA_KEY_TITLE) {
    grl_media_set_title (media, value);
  } else if (key_id == GRL_METADATA_KEY_ARTIST) {
    grl_media_audio_set_artist (GRL_MEDIA_AUDIO(media), value);
  } else if (key_id == GRL_METADATA_KEY_ALBUM) {
    grl_media_audio_set_album (GRL_MEDIA_AUDIO(media), value);
  } else if (key_id == GRL_METADATA_KEY_GENRE) {
    grl_media_audio_set_genre (GRL_MEDIA_AUDIO(media), value);
  } else if (key_id == GRL_METADATA_KEY_URL) {
    grl_media_set_url (media, value);
  } else if (key_id == GRL_METADATA_KEY_MIME) {
    grl_media_set_mime (media, value);
  } else if (key_id == GRL_METADATA_KEY_DATE) {
    grl_media_set_date (media, value);
  } else if (key_id == GRL_METADATA_KEY_DURATION) {
    gint duration = didl_h_mm_ss_to_int (value);
    if (duration >= 0) {
      grl_media_set_duration (media, duration);
    }
  } else if (key_id == GRL_METADATA_KEY_CHILDCOUNT && value && GRL_IS_MEDIA_BOX (media)) {
      grl_media_box_set_childcount (GRL_MEDIA_BOX (media), atoi (value));
  } else if (key_id == GRL_METADATA_KEY_THUMBNAIL) {
    grl_media_set_thumbnail (media, value);
  }
}

static GrlMedia *
build_media_from_didl (GrlMedia *content,
                       GUPnPDIDLLiteObject *didl_node,
                       GList *keys)
{
  const gchar *id;
  const gchar *class;

  GrlMedia *media = NULL;
  GList *didl_props;
  GList *iter;

  GRL_DEBUG ("build_media_from_didl");

  if (content) {
    media = content;
  } else {

    if (GUPNP_IS_DIDL_LITE_CONTAINER (didl_node)) {
      media = grl_media_box_new ();
    } else {
      if (!media) {
        class = gupnp_didl_lite_object_get_upnp_class (didl_node);
        if (class) {
          if (g_str_has_prefix (class, "object.item.audioItem")) {
            media = grl_media_audio_new ();
          } else if (g_str_has_prefix (class, "object.item.videoItem")) {
            media = grl_media_video_new ();
          } else if (g_str_has_prefix (class, "object.item.imageItem")) {
            media = grl_media_image_new ();
          } else {
            media = grl_media_new ();
          }
        } else {
          media = grl_media_new ();
        }
      }
    }
  }

  id = gupnp_didl_lite_object_get_id (didl_node);
  /* Root category's id is always NULL */
  if (g_strcmp0 (id, "0") == 0) {
    grl_media_set_id (media, NULL);
  } else {
    grl_media_set_id (media, id);
  }

  didl_props = didl_get_supported_resources (didl_node);

  iter = keys;
  while (iter) {
    gchar *value = get_value_for_key (iter->data, didl_node, didl_props);
    if (value) {
      set_metadata_value (media, iter->data, value);
    }
    iter = g_list_next (iter);
  }

  g_list_free (didl_props);

  return media;
}

static void
gupnp_browse_result_cb (GUPnPDIDLLiteParser *parser,
			GUPnPDIDLLiteObject *didl,
			gpointer user_data)
{
  GrlMedia *media;
  struct OperationSpec *os = (struct OperationSpec *) user_data;
  if (gupnp_didl_lite_object_get_id (didl)) {
    media = build_media_from_didl (NULL, didl, os->keys);
    os->callback (os->source,
		  os->operation_id,
		  media,
		  --os->count,
		  os->user_data,
		  NULL);
  }
}

static void
gupnp_browse_cb (GUPnPServiceProxy *service,
		 GUPnPServiceProxyAction *action,
		 gpointer user_data)
{
  GError *error = NULL;
  gchar *didl = NULL;
  guint returned = 0;
  guint matches = 0;
  gboolean result;
  struct OperationSpec *os;
  GUPnPDIDLLiteParser *didl_parser;

  GRL_DEBUG ("gupnp_browse_cb");

  os = (struct OperationSpec *) user_data;
  didl_parser = gupnp_didl_lite_parser_new ();

  result =
    gupnp_service_proxy_end_action (service, action, &error,
				    "Result", G_TYPE_STRING, &didl,
				    "NumberReturned", G_TYPE_UINT, &returned,
				    "TotalMatches", G_TYPE_UINT, &matches,
				    NULL);

  if (!result) {
    GRL_WARNING ("Operation (browse, search or query) failed");
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    if (error) {
      GRL_WARNING ("  Reason: %s", error->message);
      g_error_free (error);
    }

    goto free_resources;
  }

  if (!didl || !returned) {
    GRL_DEBUG ("Got no results");
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, NULL);

    goto free_resources;
  }

  /* Use os->count to emit "remaining" information */
  if (os->count > returned) {
    os->count = returned;
  }

  g_signal_connect (G_OBJECT (didl_parser),
                    "object-available",
                    G_CALLBACK (gupnp_browse_result_cb),
                    os);
  gupnp_didl_lite_parser_parse_didl (didl_parser,
                                     didl,
                                     &error);
  if (error) {
    GRL_WARNING ("Failed to parse DIDL result: %s", error->message);
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);

    goto free_resources;
  }

 free_resources:
  g_slice_free (struct OperationSpec, os);
  g_free (didl);
  g_object_unref (didl_parser);
}

static void
gupnp_metadata_result_cb (GUPnPDIDLLiteParser *parser,
			  GUPnPDIDLLiteObject *didl,
			  gpointer user_data)
{
  GrlMediaSourceMetadataSpec *ms = (GrlMediaSourceMetadataSpec *) user_data;
  if (gupnp_didl_lite_object_get_id (didl)) {
    build_media_from_didl (ms->media, didl, ms->keys);
    ms->callback (ms->source, ms->media, ms->user_data, NULL);
  }
}

static void
gupnp_metadata_cb (GUPnPServiceProxy *service,
		   GUPnPServiceProxyAction *action,
		   gpointer user_data)
{
  GError *error = NULL;
  gchar *didl = NULL;
  gboolean result;
  GrlMediaSourceMetadataSpec *ms;
  GUPnPDIDLLiteParser *didl_parser;

  GRL_DEBUG ("gupnp_metadata_cb");

  ms = (GrlMediaSourceMetadataSpec *) user_data;
  didl_parser = gupnp_didl_lite_parser_new ();

  result =
    gupnp_service_proxy_end_action (service, action, &error,
				    "Result", G_TYPE_STRING, &didl,
				    NULL);

  if (!result) {
    GRL_WARNING ("Metadata operation failed");
    ms->callback (ms->source, ms->media, ms->user_data, error);
    if (error) {
      GRL_WARNING ("  Reason: %s", error->message);
      g_error_free (error);
    }

    goto free_resources;
  }

  if (!didl) {
    GRL_DEBUG ("Got no metadata");
    ms->callback (ms->source, ms->media,  ms->user_data, NULL);

    goto free_resources;
  }

  g_signal_connect (G_OBJECT (didl_parser),
                    "object-available",
                    G_CALLBACK (gupnp_metadata_result_cb),
                    ms);
  gupnp_didl_lite_parser_parse_didl (didl_parser,
                                     didl,
                                     &error);
  if (error) {
    GRL_WARNING ("Failed to parse DIDL result: %s", error->message);
    ms->callback (ms->source, ms->media, ms->user_data, error);
    g_error_free (error);
    goto free_resources;
  }

 free_resources:
  g_free (didl);
  g_object_unref (didl_parser);
}

/* ================== API Implementation ================ */

static const GList *
grl_upnp_source_supported_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_URL,
                                      GRL_METADATA_KEY_MIME,
                                      GRL_METADATA_KEY_DATE,
                                      GRL_METADATA_KEY_DURATION,
                                      GRL_METADATA_KEY_ARTIST,
                                      GRL_METADATA_KEY_ALBUM,
                                      GRL_METADATA_KEY_GENRE,
                                      GRL_METADATA_KEY_CHILDCOUNT,
                                      GRL_METADATA_KEY_THUMBNAIL,
                                      NULL);
  }
  return keys;
}

static void
grl_upnp_source_browse (GrlMediaSource *source, GrlMediaSourceBrowseSpec *bs)
{
  GUPnPServiceProxyAction* action;
  gchar *upnp_filter;
  gchar *container_id;
  GError *error = NULL;
  struct OperationSpec *os;

  GRL_DEBUG ("grl_upnp_source_browse");

  upnp_filter = get_upnp_filter (bs->keys);
  GRL_DEBUG ("filter: '%s'", upnp_filter);

  os = g_slice_new0 (struct OperationSpec);
  os->source = bs->source;
  os->operation_id = bs->browse_id;
  os->keys = bs->keys;
  os->skip = bs->skip;
  os->count = bs->count;
  os->callback = bs->callback;
  os->user_data = bs->user_data;

  container_id = (gchar *) grl_media_get_id (bs->container);
  if (!container_id) {
    container_id = "0";
  }

  action =
    gupnp_service_proxy_begin_action (GRL_UPNP_SOURCE (source)->priv->service,
				      "Browse", gupnp_browse_cb,
                                      os,
				      "ObjectID", G_TYPE_STRING,
                                      container_id,
				      "BrowseFlag", G_TYPE_STRING,
                                      "BrowseDirectChildren",
				      "Filter", G_TYPE_STRING,
                                      upnp_filter,
				      "StartingIndex", G_TYPE_UINT,
                                      bs->skip,
				      "RequestedCount", G_TYPE_UINT,
                                      bs->count,
				      "SortCriteria", G_TYPE_STRING,
                                      "",
				      NULL);
  if (!action) {
    error = g_error_new (GRL_CORE_ERROR,
			 GRL_CORE_ERROR_BROWSE_FAILED,
			 "Failed to start browse action");
    bs->callback (bs->source, bs->browse_id, NULL, 0, bs->user_data, error);
    g_error_free (error);
    g_slice_free (struct OperationSpec, os);
  }

  g_free (upnp_filter);
}

static void
grl_upnp_source_search (GrlMediaSource *source, GrlMediaSourceSearchSpec *ss)
{
  GUPnPServiceProxyAction* action;
  gchar *upnp_filter;
  GError *error = NULL;
  gchar *upnp_search;
  struct OperationSpec *os;

  GRL_DEBUG ("grl_upnp_source_search");

  upnp_filter = get_upnp_filter (ss->keys);
  GRL_DEBUG ("filter: '%s'", upnp_filter);

  upnp_search = get_upnp_search (ss->text);
  GRL_DEBUG ("search: '%s'", upnp_search);

  os = g_slice_new0 (struct OperationSpec);
  os->source = ss->source;
  os->operation_id = ss->search_id;
  os->keys = ss->keys;
  os->skip = ss->skip;
  os->count = ss->count;
  os->callback = ss->callback;
  os->user_data = ss->user_data;

  action =
    gupnp_service_proxy_begin_action (GRL_UPNP_SOURCE (source)->priv->service,
				      "Search", gupnp_browse_cb,
                                      os,
				      "ContainerID", G_TYPE_STRING,
                                      "0",
				      "SearchCriteria", G_TYPE_STRING,
                                      upnp_search,
				      "Filter", G_TYPE_STRING,
                                      upnp_filter,
				      "StartingIndex", G_TYPE_UINT,
                                      ss->skip,
				      "RequestedCount", G_TYPE_UINT,
                                      ss->count,
				      "SortCriteria", G_TYPE_STRING,
                                      "",
				      NULL);
  if (!action) {
    error = g_error_new (GRL_CORE_ERROR,
			 GRL_CORE_ERROR_SEARCH_FAILED,
			 "Failed to start browse action");
    ss->callback (ss->source, ss->search_id, NULL, 0, ss->user_data, error);
    g_error_free (error);
    g_slice_free (struct OperationSpec, os);
  }

  g_free (upnp_filter);
  g_free (upnp_search);
}

/*
 * Query format is the UPnP ContentDirectory SearchCriteria format, e.g.
 * 'upnp:artist contains "Rick Astley" and
 *  (upnp:class derivedfrom "object.item.audioItem")'
 *
 * Note that we don't guarantee or check that the server actually
 * supports the given criteria. Offering the searchcaps as
 * additional metadata to clients that _really_ are interested might
 * be useful.
 */
static void
grl_upnp_source_query (GrlMediaSource *source, GrlMediaSourceQuerySpec *qs)
{
  GUPnPServiceProxyAction* action;
  gchar *upnp_filter;
  GError *error = NULL;
  struct OperationSpec *os;

  GRL_DEBUG (__func__);

  upnp_filter = get_upnp_filter (qs->keys);
  GRL_DEBUG ("filter: '%s'", upnp_filter);

  GRL_DEBUG ("query: '%s'", qs->query);

  os = g_slice_new0 (struct OperationSpec);
  os->source = qs->source;
  os->operation_id = qs->query_id;
  os->keys = qs->keys;
  os->skip = qs->skip;
  os->count = qs->count;
  os->callback = qs->callback;
  os->user_data = qs->user_data;

  action =
    gupnp_service_proxy_begin_action (GRL_UPNP_SOURCE (source)->priv->service,
				      "Search", gupnp_browse_cb, os,
				      "ContainerID", G_TYPE_STRING,
				      "0",
				      "SearchCriteria", G_TYPE_STRING,
				      qs->query,
				      "Filter", G_TYPE_STRING,
				      upnp_filter,
				      "StartingIndex", G_TYPE_UINT,
				      qs->skip,
				      "RequestedCount", G_TYPE_UINT,
				      qs->count,
				      "SortCriteria", G_TYPE_STRING,
				      "",
				      NULL);
  if (!action) {
    error = g_error_new (GRL_CORE_ERROR,
			 GRL_CORE_ERROR_QUERY_FAILED,
			 "Failed to start query action");
    qs->callback (qs->source, qs->query_id, NULL, 0, qs->user_data, error);
    g_error_free (error);
    g_slice_free (struct OperationSpec, os);
  }

  g_free (upnp_filter);
}

static void
grl_upnp_source_metadata (GrlMediaSource *source,
                          GrlMediaSourceMetadataSpec *ms)
{
  GUPnPServiceProxyAction* action;
  gchar *upnp_filter;
  gchar *id;
  GError *error = NULL;

  GRL_DEBUG ("grl_upnp_source_metadata");

  upnp_filter = get_upnp_filter (ms->keys);

  GRL_DEBUG ("filter: '%s'", upnp_filter);

  id = (gchar *) grl_media_get_id (ms->media);
  if (!id) {
    grl_media_set_title (ms->media, GRL_UPNP_SOURCE (source)->priv->upnp_name);
    id = "0";
  }

  action =
    gupnp_service_proxy_begin_action (GRL_UPNP_SOURCE (source)->priv->service,
				      "Browse", gupnp_metadata_cb,
                                      ms,
				      "ObjectID", G_TYPE_STRING,
                                      id,
				      "BrowseFlag", G_TYPE_STRING,
                                      "BrowseMetadata",
				      "Filter", G_TYPE_STRING,
                                      upnp_filter,
				      "StartingIndex", G_TYPE_UINT,
                                      0,
				      "RequestedCount", G_TYPE_UINT,
                                      0,
				      "SortCriteria", G_TYPE_STRING,
                                      "",
				      NULL);
  if (!action) {
    error = g_error_new (GRL_CORE_ERROR,
			 GRL_CORE_ERROR_METADATA_FAILED,
			 "Failed to start metadata action");
    ms->callback (ms->source, ms->media, ms->user_data, error);
    g_error_free (error);
  }

  g_free (upnp_filter);
}

static GrlSupportedOps
grl_upnp_source_supported_operations (GrlMetadataSource *metadata_source)
{
  GrlSupportedOps caps;
  GrlUpnpSource *source;

  /* Some sources may support search() while other not, so we rewrite
     supported_operations() to take that into account.
     See also note in grl_upnp_source_query() */

  source = GRL_UPNP_SOURCE (metadata_source);
  caps = GRL_OP_BROWSE | GRL_OP_METADATA;
  if (source->priv->search_enabled)
    caps = caps | GRL_OP_SEARCH | GRL_OP_QUERY;

  return caps;
}
