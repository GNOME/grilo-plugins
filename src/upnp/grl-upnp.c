/*
 * Copyright (C) 2010 Igalia S.L.
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

#ifndef GUPNPAV_OLD_VERSION
#include <libxml/tree.h>
#endif

#include "grl-upnp.h"

#define GRL_UPNP_GET_PRIVATE(object)                                    \
  (G_TYPE_INSTANCE_GET_PRIVATE((object), GRL_UPNP_SOURCE_TYPE, GrlUpnpPrivate))

/* --------- Logging  -------- */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "grl-upnp"

/* --- Plugin information --- */

#define PLUGIN_ID   "grl-upnp"
#define PLUGIN_NAME "UPnP"
#define PLUGIN_DESC "A plugin for browsing UPnP servers"

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
  gboolean search_enabled;
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
                               const GrlConfig *config);

static void grl_upnp_source_finalize (GObject *plugin);

static const GList *grl_upnp_source_supported_keys (GrlMetadataSource *source);

static void grl_upnp_source_browse (GrlMediaSource *source,
                                    GrlMediaSourceBrowseSpec *bs);

static void grl_upnp_source_search (GrlMediaSource *source,
                                    GrlMediaSourceSearchSpec *ss);

static void grl_upnp_source_metadata (GrlMediaSource *source,
                                      GrlMediaSourceMetadataSpec *ms);

static GrlSupportedOps grl_upnp_source_supported_operations (GrlMetadataSource *source);

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
grl_upnp_plugin_init (GrlPluginRegistry *registry,
                      const GrlPluginInfo *plugin,
                      const GrlConfig *config)
{
  GError *error = NULL;
  GUPnPContext *context;
  GUPnPControlPoint *cp;

  g_debug ("grl_upnp_plugin_init\n");

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

GRL_PLUGIN_REGISTER (grl_upnp_plugin_init,
                     NULL,
                     PLUGIN_ID,
                     PLUGIN_NAME,
                     PLUGIN_DESC,
                     PACKAGE_VERSION,
                     AUTHOR,
                     LICENSE,
                     SITE);

/* ================== UPnP GObject ================ */

G_DEFINE_TYPE (GrlUpnpSource, grl_upnp_source, GRL_TYPE_MEDIA_SOURCE);

static GrlUpnpSource *
grl_upnp_source_new (const gchar *source_id, const gchar *name)
{
  gchar *source_name, *source_desc;
  GrlUpnpSource *source;

  g_debug ("grl_upnp_source_new");
  source_name = g_strdup_printf (SOURCE_NAME_TEMPLATE, name);
  source_desc = g_strdup_printf (SOURCE_DESC_TEMPLATE, name);

  source = g_object_new (GRL_UPNP_SOURCE_TYPE,
			 "source-id", source_id,
			 "source-name", source_name,
			 "source-desc", source_desc,
			 NULL);

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
  source_class->metadata = grl_upnp_source_metadata;

  g_type_class_add_private (klass, sizeof (GrlUpnpPrivate));

  setup_key_mappings ();
}

static void
grl_upnp_source_init (GrlUpnpSource *source)
{
  source->priv = GRL_UPNP_GET_PRIVATE (source);
  memset (source->priv, 0, sizeof (GrlUpnpPrivate));
}

static void
grl_upnp_source_finalize (GObject *object)
{
  GrlUpnpSource *source;

  g_debug ("grl_upnp_source_finalize");

  source = GRL_UPNP_SOURCE (object);

  g_object_unref (source->priv->device);
  g_object_unref (source->priv->service);

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
  g_free (info);
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
    g_warning ("Failed to execute GetSeachCaps operation");
    if (error) {
      g_warning ("Reason: %s", error->message);
      g_error_free (error);
    }
  }

  source_info = (struct SourceInfo *) user_data;
  name = source_info->source_name;
  source_id = source_info->source_id;

  registry = grl_plugin_registry_get_instance ();
  if (grl_plugin_registry_lookup_source (registry, source_id)) {
    g_debug ("A source with id '%s' is already registered. Skipping...",
	     source_id);
    goto free_resources;
  }

  source = grl_upnp_source_new (source_id, name);
  source->priv->device = g_object_ref (source_info->device);
  source->priv->service = g_object_ref (source_info->service);

  g_debug ("Search caps for source '%s': '%s'", name, caps);

  if (caps && caps[0] != '\0') {
    g_debug ("Setting search enabled for source '%s'", name );
    source->priv->search_enabled = TRUE;
  } else {
    g_debug ("Setting search disabled for source '%s'", name );
  }

  grl_plugin_registry_register_source (registry,
                                       source_info->plugin,
                                       GRL_MEDIA_PLUGIN (source));

 free_resources:
  free_source_info (source_info);
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

  registry = grl_plugin_registry_get_instance ();
  source_id = build_source_id (udn);
  if (grl_plugin_registry_lookup_source (registry, source_id)) {
    g_debug ("A source with id '%s' is already registered. Skipping...",
	     source_id);
    goto free_resources;
  }

  /* We got a valid UPnP source */
  /* Now let's check if it supports search operations before registering */
  struct SourceInfo *source_info = g_new0 (struct SourceInfo, 1);
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
    g_warning ("Failed to start GetCapabilitiesSearch action");
    g_debug ("Setting search disabled for source '%s'", name );
    registry = grl_plugin_registry_get_instance ();
    grl_plugin_registry_register_source (registry,
                                         source_info->plugin,
                                         GRL_MEDIA_PLUGIN (source));
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

  g_debug ("device_unavailable_cb");

  udn = gupnp_device_info_get_udn (GUPNP_DEVICE_INFO (device));
  g_debug ("   udn: %s ", udn);

  registry = grl_plugin_registry_get_instance ();
  source_id = build_source_id (udn);
  source = grl_plugin_registry_lookup_source (registry, source_id);
  if (!source) {
    g_debug ("No source registered with id '%s', ignoring", source_id);
  } else {
    grl_plugin_registry_unregister_source (registry, source);
  }

  g_free (source_id);
}

const static gchar *
get_upnp_key (const GrlKeyID key_id)
{
  return g_hash_table_lookup (key_mapping, GRLKEYID_TO_POINTER (key_id));
}

const static gchar *
get_upnp_key_for_filter (const GrlKeyID key_id)
{
  return g_hash_table_lookup (filter_key_mapping,
                              GRLKEYID_TO_POINTER (key_id));
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
      (gchar *) get_upnp_key_for_filter (POINTER_TO_GRLKEYID (iter->data));
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
		       GRLKEYID_TO_POINTER (GRL_METADATA_KEY_TITLE),
		       "title");
  g_hash_table_insert (key_mapping,
		       GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ARTIST),
		       "artist");
  g_hash_table_insert (key_mapping,
		       GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ALBUM),
		       "album");
  g_hash_table_insert (key_mapping,
		       GRLKEYID_TO_POINTER (GRL_METADATA_KEY_GENRE),
		       "genre");
  g_hash_table_insert (key_mapping,
		       GRLKEYID_TO_POINTER (GRL_METADATA_KEY_URL),
		       "res");
  g_hash_table_insert (key_mapping,
		       GRLKEYID_TO_POINTER (GRL_METADATA_KEY_DATE),
		       "modified");

  /* For filter_key_mapping we only have to set mapping for
     optional keys (the others are included by default) */
  g_hash_table_insert (filter_key_mapping,
		       GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ARTIST),
		       "upnp:artist");
  g_hash_table_insert (filter_key_mapping,
		       GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ALBUM),
		       "upnp:album");
  g_hash_table_insert (filter_key_mapping,
		       GRLKEYID_TO_POINTER (GRL_METADATA_KEY_GENRE),
		       "upnp:genre");
  g_hash_table_insert (filter_key_mapping,
		       GRLKEYID_TO_POINTER (GRL_METADATA_KEY_DURATION),
		       "res@duration");
  g_hash_table_insert (filter_key_mapping,
		       GRLKEYID_TO_POINTER (GRL_METADATA_KEY_DATE),
		       "modified");
}

static gchar *
didl_res_get_protocol_info (xmlNode* res_node, gint field)
{
  gchar* pinfo;
  gchar* value;
  gchar** array;

#ifdef GUPNPAV_OLD_VERSION
  pinfo = gupnp_didl_lite_property_get_attribute (res_node, "protocolInfo");
#else
  pinfo = (gchar *) xmlGetProp (res_node, (const xmlChar *) "protocolInfo");
#endif

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
didl_get_supported_resources (
#ifdef GUPNPAV_OLD_VERSION
                              xmlNode *didl_node)
#else
                              GUPnPDIDLLiteObject *didl)
#endif
{
  GList *properties, *node;
  xmlNode *xml_node;
  gchar *protocol;

#ifdef GUPNPAV_OLD_VERSION
  properties = gupnp_didl_lite_object_get_property (didl_node, "res");
#else
  properties = gupnp_didl_lite_object_get_properties (didl, "res");
#endif

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

static gchar *
get_value_for_key (GrlKeyID key_id,
#ifdef GUPNPAV_OLD_VERSION
                   xmlNode *didl_node,
#else
                   GUPnPDIDLLiteObject *didl,
#endif
                   GList *props)
{
  GList* list;
  gchar* val = NULL;
  const gchar* upnp_key;

#ifndef GUPNPAV_OLD_VERSION
  xmlNode *didl_node = gupnp_didl_lite_object_get_xml_node (didl);
#endif

  upnp_key = get_upnp_key (key_id);

  switch (key_id) {
  case GRL_METADATA_KEY_CHILDCOUNT:

#ifdef GUPNPAV_OLD_VERSION
    val = gupnp_didl_lite_property_get_attribute (didl_node, "childCount");
#else
    val = (gchar *) xmlGetProp (didl_node, (const xmlChar *) "childCount");
#endif

    break;
  case GRL_METADATA_KEY_MIME:
    if (props) {
      val = didl_res_get_protocol_info ((xmlNode *) props->data, 2);
    }
    break;
  case GRL_METADATA_KEY_DURATION:
    if (props) {

#ifdef GUPNPAV_OLD_VERSION
      val = gupnp_didl_lite_property_get_attribute ((xmlNode *) props->data,
						    "duration");
#else
      val = (gchar *) xmlGetProp ((xmlNodePtr) props->data,
                                  (const xmlChar *) "duration");
#endif

    }
    break;
  case GRL_METADATA_KEY_URL:
    if (props) {

#ifdef GUPNPAV_OLD_VERSION
      val = gupnp_didl_lite_property_get_value ((xmlNode *) props->data);
#else
      val = (gchar *) xmlNodeGetContent ((xmlNode *) props->data);
#endif

    }
    break;
  default:
    if (upnp_key) {

#ifdef GUPNPAV_OLD_VERSION
      list = gupnp_didl_lite_object_get_property (didl_node, upnp_key);
      if (list) {
	val = gupnp_didl_lite_property_get_value ((xmlNode*) list->data);
	g_list_free (list);
      } else if (props && props->data) {
	val = gupnp_didl_lite_property_get_attribute ((xmlNode *) props->data,
						      upnp_key);
      }
#else
      list = gupnp_didl_lite_object_get_properties (didl, upnp_key);
      if (list) {
	val = (gchar *) xmlNodeGetContent ((xmlNode*) list->data);
	g_list_free (list);
      } else if (props && props->data) {
        val = (gchar *) xmlGetProp ((xmlNodePtr) props->data,
                                    (const xmlChar *) upnp_key);
      }
#endif

    }
    break;
  }

  return val;
}

static void
set_metadata_value (GrlMedia *media,
                    GrlKeyID key_id,
                    const gchar *value)
{
  switch (key_id) {
  case GRL_METADATA_KEY_TITLE:
    grl_media_set_title (media, value);
    break;
  case GRL_METADATA_KEY_ARTIST:
    grl_media_audio_set_artist (media, value);
    break;
  case GRL_METADATA_KEY_ALBUM:
    grl_media_audio_set_album (media, value);
    break;
  case GRL_METADATA_KEY_GENRE:
    grl_media_audio_set_genre (media, value);
    break;
  case GRL_METADATA_KEY_URL:
    grl_media_set_url (media, value);
    break;
  case GRL_METADATA_KEY_MIME:
    grl_media_set_mime (media, value);
    break;
  case GRL_METADATA_KEY_DATE:
    grl_media_set_date (media, value);
    break;
  case GRL_METADATA_KEY_DURATION:
    {
      gint duration = didl_h_mm_ss_to_int (value);
      if (duration >= 0) {
	grl_media_set_duration (media, duration);
      }
    }
    break;
  case GRL_METADATA_KEY_CHILDCOUNT:
    if (value && GRL_IS_MEDIA_BOX (media)) {
      grl_media_box_set_childcount (GRL_MEDIA_BOX (media), atoi (value));
    }
    break;
  default:
    break;
  }
}

static GrlMedia *
build_media_from_didl (GrlMedia *content,
#ifdef GUPNPAV_OLD_VERSION
                       xmlNode *didl_node,
#else
                       GUPnPDIDLLiteObject *didl_node,
#endif
                       GList *keys)
{
#ifdef GUPNPAV_OLD_VERSION
  gchar *id;
  gchar *class;
#else
  const gchar *id;
  const gchar *class;
#endif

  GrlMedia *media = NULL;
  GList *didl_props;
  GList *iter;

  g_debug ("build_media_from_didl");

  if (content) {
    media = content;
  }

#ifdef GUPNPAV_OLD_VERSION
  if (gupnp_didl_lite_object_is_container (didl_node)) {
#else
  if (GUPNP_IS_DIDL_LITE_CONTAINER (didl_node)) {
#endif

    media = grl_media_box_new ();
  } else {
    if (!media) {
      class = gupnp_didl_lite_object_get_upnp_class (didl_node);
      if (class) {
	if (g_str_has_prefix (class, "object.item.audioItem")) {
	  media = grl_media_audio_new ();
	} else if (g_str_has_prefix (class, "object.item.videoItem")) {
	  media = grl_data_video_new ();
	} else if (g_str_has_prefix (class, "object.item.imageItem")) {
	  media = grl_data_image_new ();
	} else {
	  media = grl_media_new ();
	}
      } else {
	media = grl_media_new ();
      }
    }
  }

  id = gupnp_didl_lite_object_get_id (didl_node);
  grl_media_set_id (media, id);

  didl_props = didl_get_supported_resources (didl_node);

  iter = keys;
  while (iter) {
    GrlKeyID key_id = POINTER_TO_GRLKEYID (iter->data);
    gchar *value = get_value_for_key (key_id, didl_node, didl_props);
    if (value) {
      set_metadata_value (media, key_id, value);
    }
    iter = g_list_next (iter);
  }

#ifdef GUPNPAV_OLD_VERSION
  g_free (id);
#endif

  g_list_free (didl_props);

  return media;
}

static void
gupnp_browse_result_cb (GUPnPDIDLLiteParser *parser,
#ifdef GUPNPAV_OLD_VERSION
			xmlNode *didl,
#else
                        GUPnPDIDLLiteObject *didl,
#endif
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

#ifdef GUPNPAV_OLD_VERSION
  gupnp_didl_lite_parser_parse_didl (didl_parser,
				     didl,
				     gupnp_browse_result_cb,
				     os,
				     &error);
#else
  g_signal_connect (G_OBJECT (didl_parser),
                    "object-available",
                    G_CALLBACK (gupnp_browse_result_cb),
                    os);
  gupnp_didl_lite_parser_parse_didl (didl_parser,
                                     didl,
                                     &error);
#endif

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
#ifdef GUPNPAV_OLD_VERSION
			  xmlNode *didl,
#else
                          GUPnPDIDLLiteObject *didl,
#endif
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

  g_debug ("gupnp_metadata_cb");

  ms = (GrlMediaSourceMetadataSpec *) user_data;

  result =
    gupnp_service_proxy_end_action (service, action, &error,
				    "Result", G_TYPE_STRING, &didl,
				    NULL);

  if (!result) {
    g_warning ("Metadata operation failed");
    ms->callback (ms->source, ms->media, ms->user_data, error);
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

#ifdef GUPNPAV_OLD_VERSION
  gupnp_didl_lite_parser_parse_didl (didl_parser,
				     didl,
				     gupnp_metadata_result_cb,
				     ms,
				     &error);
#else
  g_signal_connect (G_OBJECT (didl_parser),
                    "object-available",
                    G_CALLBACK (gupnp_metadata_result_cb),
                    ms);
  gupnp_didl_lite_parser_parse_didl (didl_parser,
                                     didl,
                                     &error);
#endif

  if (error) {
    g_warning ("Failed to parse DIDL result: %s", error->message);
    ms->callback (ms->source, ms->media, ms->user_data, error);
    g_error_free (error);
    return;
  }

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

  g_debug ("grl_upnp_source_browse");

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
                                      0,
				      "RequestedCount", G_TYPE_UINT,
                                      bs->count,
				      "SortCriteria", G_TYPE_STRING,
                                      "",
				      NULL);
  if (!action) {
    error = g_error_new (GRL_ERROR,
			 GRL_ERROR_BROWSE_FAILED,
			 "Failed to start browse action");
    bs->callback (bs->source, bs->browse_id, NULL, 0, bs->user_data, error);
    g_error_free (error);
  }
}

static void
grl_upnp_source_search (GrlMediaSource *source, GrlMediaSourceSearchSpec *ss)
{
  GUPnPServiceProxyAction* action;
  gchar *upnp_filter;
  GError *error = NULL;
  gchar *upnp_search;
  struct OperationSpec *os;

  g_debug ("grl_upnp_source_search");

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
                                      0,
				      "RequestedCount", G_TYPE_UINT,
                                      ss->count,
				      "SortCriteria", G_TYPE_STRING,
                                      "",
				      NULL);
  if (!action) {
    error = g_error_new (GRL_ERROR,
			 GRL_ERROR_SEARCH_FAILED,
			 "Failed to start browse action");
    ss->callback (ss->source, ss->search_id, NULL, 0, ss->user_data, error);
    g_error_free (error);
  }
}

static void
grl_upnp_source_metadata (GrlMediaSource *source,
                          GrlMediaSourceMetadataSpec *ms)
{
  GUPnPServiceProxyAction* action;
  gchar *upnp_filter;
  gchar *id;
  GError *error = NULL;

  g_debug ("grl_upnp_source_metadata");

  upnp_filter = get_upnp_filter (ms->keys);

  g_debug ("filter: '%s'", upnp_filter);

  id = (gchar *) grl_media_get_id (ms->media);
  if (!id) {
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
    error = g_error_new (GRL_ERROR,
			 GRL_ERROR_METADATA_FAILED,
			 "Failed to start metadata action");
    ms->callback (ms->source, ms->media, ms->user_data, error);
    g_error_free (error);
  }
}

static GrlSupportedOps
grl_upnp_source_supported_operations (GrlMetadataSource *metadata_source)
{
  GrlSupportedOps caps;
  GrlUpnpSource *source;

  /* Some sources may support search() while other not, so we rewrite
     supported_operations() to take that into account */

  source = GRL_UPNP_SOURCE (metadata_source);
  caps = GRL_OP_BROWSE | GRL_OP_METADATA;
  if (source->priv->search_enabled)
    caps |= GRL_OP_SEARCH;

  return caps;
}
