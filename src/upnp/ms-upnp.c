/*
 * Copyright (C) 2010 Igalia S.L.
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

#include <media-store.h>
#include <libgupnp/gupnp.h>
#include <libgupnp-av/gupnp-av.h>
#include <string.h>
#include <stdlib.h>

#include "ms-upnp.h"

#define MS_UPNP_GET_PRIVATE(object)				\
  (G_TYPE_INSTANCE_GET_PRIVATE((object), MS_UPNP_SOURCE_TYPE, MsUpnpPrivate))

/* --------- Logging  -------- */ 

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "ms-upnp"

/* --- Plugin information --- */

#define PLUGIN_ID   "ms-upnp"
#define PLUGIN_NAME "UPnP"
#define PLUGIN_DESC "A plugin for browsing UPnP servers"

#define SOURCE_ID_TEMPLATE    "ms-upnp-%s"
#define SOURCE_NAME_TEMPLATE  "UPnP - %s"
#define SOURCE_DESC_TEMPLATE  "A source for browsing the UPnP server '%s'"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"

/* --- Other --- */

#ifndef CONTENT_DIR_SERVICE
#define CONTENT_DIR_SERVICE "urn:schemas-upnp-org:service:ContentDirectory"
#endif

#define UPNP_SEARCH_SPEC \
  "title contains \"%s\" or "			\
  "album contains \"%s\" or "			\
  "artist contains \"%s\""

struct _MsUpnpPrivate {
  GUPnPDeviceProxy* device;
  GUPnPServiceProxy* service;
  gboolean search_enabled;
};

struct OperationSpec {
  MsMediaSource *source;
  guint operation_id;
  GList *keys;
  guint skip;
  guint count;
  MsMediaSourceResultCb callback;
  gpointer user_data;
};

struct SourceInfo {
  MsUpnpSource *source;
  MsPluginInfo *plugin;
};

static void setup_key_mappings (void);

static gchar *build_source_id (const gchar *udn);

static MsUpnpSource *ms_upnp_source_new (const gchar *id, const gchar *name);

gboolean ms_upnp_plugin_init (MsPluginRegistry *registry,
			      const MsPluginInfo *plugin);

static const GList *ms_upnp_source_supported_keys (MsMetadataSource *source);

static void ms_upnp_source_browse (MsMediaSource *source,
				   MsMediaSourceBrowseSpec *bs);

static void ms_upnp_source_search (MsMediaSource *source,
				   MsMediaSourceSearchSpec *ss);

static void ms_upnp_source_metadata (MsMediaSource *source,
				     MsMediaSourceMetadataSpec *ms);

static MsSupportedOps ms_upnp_source_supported_operations (MsMetadataSource *source);

static void device_available_cb (GUPnPControlPoint *cp,
				 GUPnPDeviceProxy *device,
				 gpointer user_data);
static void device_unavailable_cb (GUPnPControlPoint *cp,
				   GUPnPDeviceProxy *device,
				   gpointer user_data);

/* ===================== Globals  ================= */

static GHashTable *key_mapping = NULL;
static GHashTable *filter_key_mapping = NULL;

/* =================== UPnP Plugin  =============== */

gboolean
ms_upnp_plugin_init (MsPluginRegistry *registry, const MsPluginInfo *plugin)
{
  GError *error = NULL;
  GUPnPContext *context;
  GUPnPControlPoint *cp;

  g_debug ("ms_upnp_plugin_init\n");

  /* libsoup needs this */
  if (!g_thread_supported()) {
    g_thread_init (NULL);
  }

  context = gupnp_context_new (NULL, NULL, 0, &error);
  if (error) {
    g_critical ("Failed to create GUPnP context: %s", error->message);
    g_error_free (error);
    return FALSE;
  }

  cp = gupnp_control_point_new (context, "ssdp:all");
  if (!cp) {
    g_critical ("Failed to create control point");
    return FALSE;
  }
  g_signal_connect (cp,
		    "device-proxy-available",
		    G_CALLBACK (device_available_cb),
		    (gpointer) plugin);
  g_signal_connect (cp,
		    "device-proxy-unavailable",
		    G_CALLBACK (device_unavailable_cb),
		    NULL);
  
  gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);
  
  return TRUE;
}

MS_PLUGIN_REGISTER (ms_upnp_plugin_init, 
                    NULL, 
                    PLUGIN_ID,
                    PLUGIN_NAME, 
                    PLUGIN_DESC, 
                    PACKAGE_VERSION,
                    AUTHOR, 
                    LICENSE, 
                    SITE);

/* ================== UPnP GObject ================ */

static MsUpnpSource *
ms_upnp_source_new (const gchar *source_id, const gchar *name)
{
  gchar *source_name, *source_desc;
  MsUpnpSource *source;

  g_debug ("ms_upnp_source_new");
  source_name = g_strdup_printf (SOURCE_NAME_TEMPLATE, name);
  source_desc = g_strdup_printf (SOURCE_DESC_TEMPLATE, name);

  source = g_object_new (MS_UPNP_SOURCE_TYPE,
			 "source-id", source_id,
			 "source-name", source_name,
			 "source-desc", source_desc,
			 NULL);

  g_free (source_name);
  g_free (source_desc);

  return source;
}

static void
ms_upnp_source_class_init (MsUpnpSourceClass * klass)
{
  MsMediaSourceClass *source_class = MS_MEDIA_SOURCE_CLASS (klass);
  MsMetadataSourceClass *metadata_class = MS_METADATA_SOURCE_CLASS (klass);
  source_class->browse = ms_upnp_source_browse;
  source_class->search = ms_upnp_source_search;
  source_class->metadata = ms_upnp_source_metadata;
  metadata_class->supported_keys = ms_upnp_source_supported_keys;
  metadata_class->supported_operations = ms_upnp_source_supported_operations;

  g_type_class_add_private (klass, sizeof (MsUpnpPrivate));

  setup_key_mappings ();
}

static void
ms_upnp_source_init (MsUpnpSource *source)
{
  source->priv = MS_UPNP_GET_PRIVATE (source);
  memset (source->priv, 0, sizeof (MsUpnpPrivate));
}

G_DEFINE_TYPE (MsUpnpSource, ms_upnp_source, MS_TYPE_MEDIA_SOURCE);

/* ======================= Utilities ==================== */

static gchar *
build_source_id (const gchar *udn)
{
  return g_strdup_printf (SOURCE_ID_TEMPLATE, udn);
}

static void
gupnp_search_caps_cb (GUPnPServiceProxy *service,
		      GUPnPServiceProxyAction *action,
		      gpointer user_data)
{
  GError *error = NULL;
  gchar *caps = NULL;
  gchar *name;
  MsUpnpSource *source;
  MsPluginInfo *plugin;
  MsPluginRegistry *registry;
  struct SourceInfo *source_info;
  gboolean result;

  result =
    gupnp_service_proxy_end_action (service, action, &error,
				    "SearchCaps", G_TYPE_STRING, &caps,
				    NULL);
  if (!result) {
    g_warning ("Failed to execute GetSeachCaps operation");
    if (error) {
      g_warning ("Reason: %s", error->message);
      g_error_free (error);
    }
  }

  source_info = (struct SourceInfo *) user_data;
  source = source_info->source;
  plugin = source_info->plugin;

  name = ms_metadata_source_get_name (MS_METADATA_SOURCE (source));
  
  g_debug ("Search caps for source '%s': '%s'", name, caps);

  if (caps && caps[0] != '\0') {
    g_debug ("Setting search enabled for source '%s'", name );
    source->priv->search_enabled = TRUE;
  } else {
    g_debug ("Setting search disabled for source '%s'", name );
  }

  registry = ms_plugin_registry_get_instance ();
  ms_plugin_registry_register_source (registry,
				      plugin,
				      MS_MEDIA_PLUGIN (source));

  g_free (name);
  g_free (source_info);
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
  MsUpnpSource *source;
  MsPluginRegistry *registry;
  gchar *source_id;

  g_debug ("device_available_cb");

  type = gupnp_device_info_get_device_type (GUPNP_DEVICE_INFO (device));
  g_debug ("  type: %s", type);
  if (!g_pattern_match_simple ("urn:schemas-upnp-org:device:MediaServer:*",
			       type)) {
    return;
  }

  service = gupnp_device_info_get_service (GUPNP_DEVICE_INFO (device),
					   CONTENT_DIR_SERVICE);
  if (!service) {
    g_debug ("Device does not provide requied service, ignoring...");
    return;
  }

  udn = gupnp_device_info_get_udn (GUPNP_DEVICE_INFO (device));
  g_debug ("   udn: %s ", udn);
  
  name = gupnp_device_info_get_friendly_name (GUPNP_DEVICE_INFO (device));
  g_debug ("  name: %s", name);
      
  registry = ms_plugin_registry_get_instance ();
  source_id = build_source_id (udn);
  if (ms_plugin_registry_lookup_source (registry, source_id)) {
    g_debug ("A source with id '%s' is already registered. Skipping...",
	     source_id);
    goto free_resources;
  }

  /* We got a valid UPnP source */
  source = ms_upnp_source_new (source_id, name);
  source->priv->device = g_object_ref (device);
  source->priv->service = g_object_ref (service);  
  
  /* Now let's check if it supports search operations before registering */
  struct SourceInfo *source_info = g_new0 (struct SourceInfo, 1);
  source_info->source = source;
  source_info->plugin = (MsPluginInfo *) user_data;

  if (!gupnp_service_proxy_begin_action (GUPNP_SERVICE_PROXY (service),
					 "GetSearchCapabilities",
					 gupnp_search_caps_cb,
					 source_info,
					 NULL)) {
    g_warning ("Failed to start GetCapabilitiesSearch action");
    g_debug ("Setting search disabled for source '%s'", name );
    registry = ms_plugin_registry_get_instance ();
    ms_plugin_registry_register_source (registry,
					source_info->plugin,
					MS_MEDIA_PLUGIN (source_info->source));
    g_free (source_info);
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
  MsMediaPlugin *source;
  MsPluginRegistry *registry;
  gchar *source_id;

  g_debug ("device_unavailable_cb");

  udn = gupnp_device_info_get_udn (GUPNP_DEVICE_INFO (device));
  g_debug ("   udn: %s ", udn);

  registry = ms_plugin_registry_get_instance ();
  source_id = build_source_id (udn);
  source = ms_plugin_registry_lookup_source (registry, source_id);
  if (!source) {
    g_debug ("No source registered with this id '%s', ignoring", source_id);
  } else {
    ms_plugin_registry_unregister_source (registry, source);
  }

  g_free (source_id);
}

const static gchar *
get_upnp_key (const MsKeyID key_id)
{
  return g_hash_table_lookup (key_mapping, MSKEYID_TO_POINTER (key_id));
}

const static gchar *
get_upnp_key_for_filter (const MsKeyID key_id)
{
  return g_hash_table_lookup (filter_key_mapping, MSKEYID_TO_POINTER (key_id));
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
      (gchar *) get_upnp_key_for_filter (POINTER_TO_MSKEYID (iter->data));
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

  g_hash_table_insert (key_mapping, 
		       MSKEYID_TO_POINTER (MS_METADATA_KEY_TITLE),
		       "title");
  g_hash_table_insert (key_mapping,
		       MSKEYID_TO_POINTER (MS_METADATA_KEY_ARTIST),
		       "artist");
  g_hash_table_insert (key_mapping,
		       MSKEYID_TO_POINTER (MS_METADATA_KEY_ALBUM),
		       "album");
  g_hash_table_insert (key_mapping,
		       MSKEYID_TO_POINTER (MS_METADATA_KEY_GENRE),
		       "genre");
  g_hash_table_insert (key_mapping,
		       MSKEYID_TO_POINTER (MS_METADATA_KEY_URL),
		       "res");
  g_hash_table_insert (key_mapping,
		       MSKEYID_TO_POINTER (MS_METADATA_KEY_DATE),
		       "modified");

  /* For filter_key_mapping we only have to set mapping for
     optional keys (the others are included by default) */
  g_hash_table_insert (filter_key_mapping,
		       MSKEYID_TO_POINTER (MS_METADATA_KEY_ARTIST),
		       "upnp:artist");
  g_hash_table_insert (filter_key_mapping,
		       MSKEYID_TO_POINTER (MS_METADATA_KEY_ALBUM),
		       "upnp:album");
  g_hash_table_insert (filter_key_mapping,
		       MSKEYID_TO_POINTER (MS_METADATA_KEY_GENRE),
		       "upnp:genre");
/*   g_hash_table_insert (filter_key_mapping, */
/* 		       MSKEYID_TO_POINTER (MS_METADATA_KEY_URL), */
/* 		       "res"); */
  g_hash_table_insert (filter_key_mapping,
		       MSKEYID_TO_POINTER (MS_METADATA_KEY_DURATION),
		       "res@duration");
  g_hash_table_insert (filter_key_mapping,
		       MSKEYID_TO_POINTER (MS_METADATA_KEY_DATE),
		       "modified");
}

static gchar *
didl_res_get_protocol_info (xmlNode* res_node, gint field)
{
  gchar* pinfo;
  gchar* value;
  gchar** array;
  
  pinfo = gupnp_didl_lite_property_get_attribute (res_node, "protocolInfo");
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
didl_get_supported_resources (xmlNode *didl_node)
{
  GList *properties, *node;
  xmlNode *xml_node;
    gchar *protocol;
  
  properties = gupnp_didl_lite_object_get_property (didl_node, "res");
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
  int len = 0;
  int i = 0;
  int head = 0;
  int tail = 0;
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

static gchar *
get_value_for_key (MsKeyID key_id, xmlNode *didl_node, GList *props)
{
  GList* list;
  gchar* val = NULL;
  const gchar* upnp_key;
  
  upnp_key = get_upnp_key (key_id);

  switch (key_id) {
  case MS_METADATA_KEY_CHILDCOUNT:
    val = gupnp_didl_lite_property_get_attribute (didl_node, "childCount");
    break;
  case MS_METADATA_KEY_MIME:
    if (props) {
      val = didl_res_get_protocol_info ((xmlNode *) props->data, 2);
    }
    break;
  case MS_METADATA_KEY_DURATION:
    if (props) {
      val = gupnp_didl_lite_property_get_attribute ((xmlNode *) props->data,
						    "duration");
    }
    break;
  case MS_METADATA_KEY_URL:
    if (props) {
      val = gupnp_didl_lite_property_get_value ((xmlNode *) props->data);
    }
    break;
  default:
    if (upnp_key) {
      list = gupnp_didl_lite_object_get_property (didl_node, upnp_key);
      if (list) {
	val = gupnp_didl_lite_property_get_value ((xmlNode*) list->data);
	g_list_free (list);
      } else if (props && props->data) {
	val = gupnp_didl_lite_property_get_attribute ((xmlNode *) props->data,
						      upnp_key);
      }
    }
    break;
  }

  return val;
}

static void
set_metadata_value (MsContentMedia *media, MsKeyID key_id, const gchar *value)
{
  switch (key_id) {
  case MS_METADATA_KEY_TITLE:
    ms_content_media_set_title (media, value);
    break;
  case MS_METADATA_KEY_ARTIST:
    ms_content_audio_set_artist (media, value);
    break;
  case MS_METADATA_KEY_ALBUM:
    ms_content_audio_set_album (media, value);
    break;
  case MS_METADATA_KEY_GENRE:
    ms_content_audio_set_genre (media, value);
    break;
  case MS_METADATA_KEY_URL:
    ms_content_media_set_url (media, value);
    break;
  case MS_METADATA_KEY_MIME:
    ms_content_media_set_mime (media, value);
    break;
  case MS_METADATA_KEY_DATE:
    ms_content_media_set_date (media, value);
    break;
  case MS_METADATA_KEY_DURATION:
    {
      gint duration = didl_h_mm_ss_to_int (value);
      if (duration >= 0) {
	ms_content_media_set_duration (media, duration);
      }
    }
    break;
  case MS_METADATA_KEY_CHILDCOUNT:
    if (value && MS_IS_CONTENT_BOX (media)) {
      ms_content_box_set_childcount (MS_CONTENT_BOX (media), atoi (value));
    }
    break;
  default:
    break;
  }
}

static MsContentMedia *
build_media_from_didl (xmlNode *didl_node, GList *keys)
{
  gchar *id;
  MsContentMedia *media;
  GList *didl_props;
  gchar *class;
  GList *iter;

  g_debug ("build_media_from_didl");

  if (gupnp_didl_lite_object_is_container (didl_node)) {
    media = MS_CONTENT_MEDIA (ms_content_box_new ());
  } else {
    class = gupnp_didl_lite_object_get_upnp_class (didl_node);
    if (class) {
      if (g_str_has_prefix (class, "object.item.audioItem")) {
	media = ms_content_audio_new ();
      } else if (g_str_has_prefix (class, "object.item.videoItem")) {
	media = ms_content_video_new ();
      } else if (g_str_has_prefix (class, "object.item.imageItem")) {
	media = ms_content_image_new ();
      } else {
	media = ms_content_media_new ();
      }
    } else {
      media = ms_content_media_new ();
    }
  }

  id = gupnp_didl_lite_object_get_id (didl_node);
  ms_content_media_set_id (media, id);
  
  didl_props = didl_get_supported_resources (didl_node);

  iter = keys;
  while (iter) {
    MsKeyID key_id = POINTER_TO_MSKEYID (iter->data);
    gchar *value = get_value_for_key (key_id, didl_node, didl_props);
    if (value) {
      set_metadata_value (media, key_id, value);
    }
    iter = g_list_next (iter);
  }

  g_free (id);
  g_list_free (didl_props);

  return media;
}

static void
gupnp_browse_result_cb (GUPnPDIDLLiteParser *parser,
			xmlNode *didl_node,
			gpointer user_data)
{
  MsContentMedia *media;
  struct OperationSpec *os = (struct OperationSpec *) user_data;
  media = build_media_from_didl (didl_node, os->keys);
  os->callback (os->source,
		os->operation_id,
		media,
		--os->count,
		os->user_data,
		NULL);
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

  os = (struct OperationSpec *) user_data;

  g_debug ("gupnp_browse_cb");

  result =
    gupnp_service_proxy_end_action (service, action, &error,
				    "Result", G_TYPE_STRING, &didl,
				    "NumberReturned", G_TYPE_UINT, &returned,
				    "TotalMatches", G_TYPE_UINT, &matches,
				    NULL);

  if (!result) {
    g_warning ("Browse operation failed");
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    if (error) {
      g_warning ("  Reason: %s", error->message);
      g_error_free (error);
    }
    return;
  }

  if (!didl || !returned) {
    g_debug ("Got no results");
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, NULL);
    return;
  }

  /* Use os->count to emit "remaining" information */
  if (os->count > returned) {
    os->count = returned;
  }

  didl_parser = gupnp_didl_lite_parser_new ();
  gupnp_didl_lite_parser_parse_didl (didl_parser,
				     didl,
				     gupnp_browse_result_cb,
				     os,
				     &error);
  if (error) {
    g_warning ("Failed to parse DIDL result: %s", error->message);
    os->callback (os->source, os->operation_id, NULL, 0, os->user_data, error);
    g_error_free (error);
    return;
  }

  g_free (didl);
  g_object_unref (didl_parser);
}

static void
gupnp_metadata_result_cb (GUPnPDIDLLiteParser *parser,
			  xmlNode *didl_node,
			  gpointer user_data)
{
  MsContentMedia *media;
  MsMediaSourceMetadataSpec *ms = (MsMediaSourceMetadataSpec *) user_data;
  media = build_media_from_didl (didl_node, ms->keys);
  ms->callback (ms->source, media, ms->user_data, NULL);
}

static void
gupnp_metadata_cb (GUPnPServiceProxy *service,
		   GUPnPServiceProxyAction *action,
		   gpointer user_data)
{
  GError *error = NULL;
  gchar *didl = NULL;
  gboolean result;
  MsMediaSourceMetadataSpec *ms;
  GUPnPDIDLLiteParser *didl_parser;

  g_debug ("gupnp_metadata_cb");

  ms = (MsMediaSourceMetadataSpec *) user_data;

  result =
    gupnp_service_proxy_end_action (service, action, &error,
				    "Result", G_TYPE_STRING, &didl,
				    NULL);

  if (!result) {
    g_warning ("Metadata operation failed");
    ms->callback (ms->source, NULL, ms->user_data, error);
    if (error) {
      g_warning ("  Reason: %s", error->message);
      g_error_free (error);
    }
    return;
  }

  if (!didl) {
    g_debug ("Got no metadata");
    ms->callback (ms->source, ms->media,  ms->user_data, NULL);
    return;
  }
  
  didl_parser = gupnp_didl_lite_parser_new ();
  gupnp_didl_lite_parser_parse_didl (didl_parser,
				     didl,
				     gupnp_metadata_result_cb,
				     ms,
				     &error);
  if (error) {
    g_warning ("Failed to parse DIDL result: %s", error->message);
    ms->callback (ms->source, NULL, ms->user_data, error);
    g_error_free (error);
    return;
  }
  
  g_free (didl);
  g_object_unref (didl_parser);
}

/* ================== API Implementation ================ */

static const GList *
ms_upnp_source_supported_keys (MsMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = ms_metadata_key_list_new (MS_METADATA_KEY_ID,
				     MS_METADATA_KEY_TITLE, 
				     MS_METADATA_KEY_URL,
				     MS_METADATA_KEY_MIME,
				     MS_METADATA_KEY_DATE,
				     MS_METADATA_KEY_DURATION,
				     MS_METADATA_KEY_ARTIST,
				     MS_METADATA_KEY_ALBUM,
				     MS_METADATA_KEY_GENRE,
				     MS_METADATA_KEY_CHILDCOUNT,
				     NULL);
  }
  return keys;
}

static void
ms_upnp_source_browse (MsMediaSource *source, MsMediaSourceBrowseSpec *bs)
{
  GUPnPServiceProxyAction* action;
  gchar *upnp_filter;
  gchar *container_id;
  GError *error = NULL;
  struct OperationSpec *os;

  g_debug ("ms_upnp_source_browse");
	
  upnp_filter = get_upnp_filter (bs->keys);
  g_debug ("filter: '%s'", upnp_filter);

  os = g_new0 (struct OperationSpec, 1);
  os->source = bs->source;
  os->operation_id = bs->browse_id;
  os->keys = bs->keys;
  os->skip = bs->skip;
  os->count = bs->count;
  os->callback = bs->callback;
  os->user_data = bs->user_data;

  container_id = (gchar *) ms_content_media_get_id (bs->container);
  if (!container_id) {
    container_id = "0";
  }

  action =
    gupnp_service_proxy_begin_action (MS_UPNP_SOURCE (source)->priv->service,
				      "Browse", gupnp_browse_cb, os,
				      "ObjectID", G_TYPE_STRING, container_id,
				      "BrowseFlag", G_TYPE_STRING, "BrowseDirectChildren",
				      "Filter", G_TYPE_STRING, upnp_filter,
				      "StartingIndex", G_TYPE_UINT, 0,
				      "RequestedCount", G_TYPE_UINT, bs->count,
				      "SortCriteria", G_TYPE_STRING, "",
				      NULL);
  if (!action) {
    error = g_error_new (MS_ERROR,
			 MS_ERROR_BROWSE_FAILED,
			 "Failed to start browse action");
    bs->callback (bs->source, bs->browse_id, NULL, 0, bs->user_data, error);
    g_error_free (error);
  }
}

static void
ms_upnp_source_search (MsMediaSource *source, MsMediaSourceSearchSpec *ss)
{
  GUPnPServiceProxyAction* action;
  gchar *upnp_filter;
  GError *error = NULL;
  gchar *upnp_search;
  struct OperationSpec *os;

  g_debug ("ms_upnp_source_search");
	
  upnp_filter = get_upnp_filter (ss->keys);
  g_debug ("filter: '%s'", upnp_filter);

  upnp_search = get_upnp_search (ss->text);
  g_debug ("search: '%s'", upnp_search);

  os = g_new0 (struct OperationSpec, 1);
  os->source = ss->source;
  os->operation_id = ss->search_id;
  os->keys = ss->keys;
  os->skip = ss->skip;
  os->count = ss->count;
  os->callback = ss->callback;
  os->user_data = ss->user_data;

  action =
    gupnp_service_proxy_begin_action (MS_UPNP_SOURCE (source)->priv->service,
				      "Search", gupnp_browse_cb, os,
				      "ContainerID", G_TYPE_STRING, "0",
				      "SearchCriteria", G_TYPE_STRING, upnp_search,
				      "Filter", G_TYPE_STRING, upnp_filter,
				      "StartingIndex", G_TYPE_UINT, 0,
				      "RequestedCount", G_TYPE_UINT, ss->count,
				      "SortCriteria", G_TYPE_STRING, "",
				      NULL);
  if (!action) {
    error = g_error_new (MS_ERROR,
			 MS_ERROR_SEARCH_FAILED,
			 "Failed to start browse action");
    ss->callback (ss->source, ss->search_id, NULL, 0, ss->user_data, error);
    g_error_free (error);
  }
}

static void
ms_upnp_source_metadata (MsMediaSource *source,
			 MsMediaSourceMetadataSpec *ms)
{
  GUPnPServiceProxyAction* action;
  gchar *upnp_filter;
  gchar *id;
  GError *error = NULL;

  g_debug ("ms_upnp_source_metadata");
	
  upnp_filter = get_upnp_filter (ms->keys);

  g_debug ("filter: '%s'", upnp_filter);

  id = (gchar *) ms_content_media_get_id (ms->media);
  if (!id) {
    id = "0";
  }

  action =
    gupnp_service_proxy_begin_action (MS_UPNP_SOURCE (source)->priv->service,
				      "Browse", gupnp_metadata_cb, ms,
				      "ObjectID", G_TYPE_STRING, id,
				      "BrowseFlag", G_TYPE_STRING, "BrowseMetadata",
				      "Filter", G_TYPE_STRING, upnp_filter,
				      "StartingIndex", G_TYPE_UINT, 0,
				      "RequestedCount", G_TYPE_UINT, 0,
				      "SortCriteria", G_TYPE_STRING, "",
				      NULL);
  if (!action) {
    error = g_error_new (MS_ERROR,
			 MS_ERROR_METADATA_FAILED,
			 "Failed to start metadata action");
    ms->callback (ms->source, NULL, ms->user_data, error);
    g_error_free (error);
  }
}

static MsSupportedOps
ms_upnp_source_supported_operations (MsMetadataSource *metadata_source)
{
  MsSupportedOps caps;
  MsUpnpSource *source;

  /* Some sources may support search() while other not, so we rewrite 
     supported_operations() to take that into account */

  source = MS_UPNP_SOURCE (metadata_source);
  caps = MS_OP_BROWSE | MS_OP_METADATA;
  if (source->priv->search_enabled)
    caps |= MS_OP_SEARCH;

  return caps;
}
