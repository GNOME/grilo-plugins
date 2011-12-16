/*
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
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

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <rest/rest-proxy.h>

#include "grl-bliptv.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT bliptv_log_domain
GRL_LOG_DOMAIN_STATIC(bliptv_log_domain);

/* --- Plugin information --- */

#define PLUGIN_ID   BLIPTV_PLUGIN_ID

#define SOURCE_ID   "grl-bliptv"
#define SOURCE_NAME "Blip.tv"
#define SOURCE_DESC "A source for browsing and searching Blip.tv videos"


G_DEFINE_TYPE (GrlBliptvSource, grl_bliptv_source, GRL_TYPE_MEDIA_SOURCE)

#define BLIPTV_SOURCE_PRIVATE(o)                                        \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o),                                    \
                                GRL_TYPE_BLIPTV_SOURCE,                 \
                                GrlBliptvSourcePrivate))

struct _GrlBliptvSourcePrivate
{
  guint not_used;
};

typedef struct
{
  GrlMediaSource *source;
  guint           operation_id;
  guint           count;

  GrlMediaSourceResultCb callback;
  gpointer               user_data;

  RestProxy     *proxy;
  RestProxyCall *call;
} BliptvOperation;

typedef struct
{
  GrlKeyID     grl_key;
  const gchar *exp;
} BliptvAssoc;

/**/

static GList *bliptv_mappings = NULL;

static void bliptv_setup_mapping (void);

static GrlBliptvSource *grl_bliptv_source_new (void);

gboolean grl_bliptv_plugin_init (GrlPluginRegistry *registry,
                                 const GrlPluginInfo *plugin,
                                 GList *configs);

static const GList *grl_bliptv_source_supported_keys (GrlMetadataSource *source);

static GrlCaps * grl_bliptv_source_get_caps (GrlMetadataSource *source,
                                             GrlSupportedOps operation);

static void grl_bliptv_source_browse (GrlMediaSource *source,
                                      GrlMediaSourceBrowseSpec *bs);

static void grl_bliptv_source_search (GrlMediaSource *source,
                                      GrlMediaSourceSearchSpec *ss);

static void grl_bliptv_source_cancel (GrlMediaSource *source,
                                      guint operation_id);

/**/

gboolean
grl_bliptv_plugin_init (GrlPluginRegistry *registry,
                        const GrlPluginInfo *plugin,
                        GList *configs)
{
  GRL_LOG_DOMAIN_INIT (bliptv_log_domain, "bliptv");

  bliptv_setup_mapping ();

  GrlBliptvSource *source = grl_bliptv_source_new ();
  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source),
                                       NULL);
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_bliptv_plugin_init,
                     NULL,
                     PLUGIN_ID);


/* ================== Blip.tv GObject ================ */

static GrlBliptvSource *
grl_bliptv_source_new (void)
{
  return g_object_new (GRL_TYPE_BLIPTV_SOURCE,
                       "source-id", SOURCE_ID,
                       "source-name", SOURCE_NAME,
                       "source-desc", SOURCE_DESC,
                       NULL);
}

static void
grl_bliptv_source_dispose (GObject *object)
{
  G_OBJECT_CLASS (grl_bliptv_source_parent_class)->dispose (object);
}

static void
grl_bliptv_source_finalize (GObject *object)
{
  G_OBJECT_CLASS (grl_bliptv_source_parent_class)->finalize (object);
}

static void
grl_bliptv_source_class_init (GrlBliptvSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GrlMediaSourceClass *source_class = GRL_MEDIA_SOURCE_CLASS (klass);
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GrlBliptvSourcePrivate));

  object_class->dispose = grl_bliptv_source_dispose;
  object_class->finalize = grl_bliptv_source_finalize;

  source_class->browse = grl_bliptv_source_browse;
  source_class->search = grl_bliptv_source_search;
  source_class->cancel = grl_bliptv_source_cancel;

  metadata_class->supported_keys = grl_bliptv_source_supported_keys;
  metadata_class->get_caps = grl_bliptv_source_get_caps;
}

static void
grl_bliptv_source_init (GrlBliptvSource *self)
{
  self->priv = BLIPTV_SOURCE_PRIVATE (self);
}

/**/

static void
bliptv_insert_mapping (GrlKeyID grl_key, const gchar *exp)
{
  BliptvAssoc *assoc = g_new (BliptvAssoc, 1);

  assoc->grl_key = grl_key;
  assoc->exp     = exp;

  bliptv_mappings = g_list_append (bliptv_mappings, assoc);
}

static void
bliptv_setup_mapping (void)
{
  bliptv_insert_mapping (GRL_METADATA_KEY_ID,
                         "blip:item_id");

  bliptv_insert_mapping (GRL_METADATA_KEY_PUBLICATION_DATE,
                         "blip:datestamp");

  bliptv_insert_mapping (GRL_METADATA_KEY_TITLE,
                         "title");

  bliptv_insert_mapping (GRL_METADATA_KEY_MIME,
                         "enclosure/@type");

  bliptv_insert_mapping (GRL_METADATA_KEY_URL,
                         "enclosure/@url");

  bliptv_insert_mapping (GRL_METADATA_KEY_THUMBNAIL,
                         "media:thumbnail/@url");
}

static void
bliptv_operation_free (BliptvOperation *op)
{
  if (op->call)
    g_object_unref (op->call);
  if (op->proxy)
    g_object_unref (op->proxy);
  if (op->source)
    g_object_unref (op->source);
  g_slice_free (BliptvOperation, op);
}

static void
proxy_call_raw_async_cb (RestProxyCall *call,
                         const GError  *error,
                         GObject       *weak_object,
                         gpointer       data)
{
  BliptvOperation    *op = (BliptvOperation *) data;
  xmlDocPtr           doc = NULL;
  xmlXPathContextPtr  xpath = NULL;
  xmlXPathObjectPtr   obj = NULL;
  gint i, nb_items = 0;

  GRL_DEBUG ("Response id=%u", op->operation_id);

  doc = xmlParseMemory (rest_proxy_call_get_payload (call),
                        rest_proxy_call_get_payload_length (call));

  g_object_unref (op->call);
  op->call = NULL;

  if (!doc)
    goto finalize;

  xpath = xmlXPathNewContext (doc);
  if (!xpath)
    goto finalize;

  xmlXPathRegisterNs (xpath,
                      (xmlChar *) "blip",
                      (xmlChar *) "http://blip.tv/dtd/blip/1.0");
  xmlXPathRegisterNs (xpath,
                      (xmlChar *) "media",
                      (xmlChar *) "http://search.yahoo.com/mrss/");

  obj = xmlXPathEvalExpression ((xmlChar *) "/rss/channel/item", xpath);
  if (obj)
    {
      nb_items = xmlXPathNodeSetGetLength (obj->nodesetval);
      xmlXPathFreeObject (obj);
    }

  if (nb_items < op->count)
    op->count = nb_items;

  for (i = 0; i < nb_items; i++)
    {
      GList *mapping = bliptv_mappings;
      GrlMedia *media = grl_media_video_new ();

      while (mapping)
        {
          BliptvAssoc *assoc = (BliptvAssoc *) mapping->data;
          gchar *str;

          str = g_strdup_printf ("string(/rss/channel/item[%i]/%s)",
                                 i + 1, assoc->exp);

          obj = xmlXPathEvalExpression ((xmlChar *) str, xpath);
          if (obj)
            {
              if (obj->stringval && obj->stringval[0] != '\0')
                {
                  GType _type;
                  GRL_DEBUG ("\t%s -> %s", str, obj->stringval);
                  _type = grl_metadata_key_get_type (assoc->grl_key);
                  switch (_type)
                    {
                    case G_TYPE_STRING:
                      grl_data_set_string (GRL_DATA (media),
                                           assoc->grl_key,
                                           (gchar *) obj->stringval);
                      break;

                    case G_TYPE_INT:
                      grl_data_set_int (GRL_DATA (media),
                                        assoc->grl_key,
                                        (gint) atoi ((gchar *) obj->stringval));
                      break;

                    case G_TYPE_FLOAT:
                      grl_data_set_float (GRL_DATA (media),
                                          assoc->grl_key,
                                          (gfloat) atof ((gchar *) obj->stringval));
                      break;

                    default:
                      /* G_TYPE_DATE_TIME is not a constant, so this has to be
                       * in "default:" */
                      if (_type == G_TYPE_DATE_TIME) {
                        GDateTime *date =
                            grl_date_time_from_iso8601 ((gchar *) obj->stringval);
                        GRL_DEBUG ("Setting %s to %s",
                                   grl_metadata_key_get_name (assoc->grl_key),
                                   g_date_time_format (date, "%F %H:%M:%S"));
                        grl_data_set_boxed (GRL_DATA (media),
                                            assoc->grl_key, date);
                        g_date_time_unref (date);
                      } else {
                        GRL_DEBUG ("\tUnexpected data type: %s",
                                   g_type_name (_type));
                      }
                      break;
                    }
                }
              xmlXPathFreeObject (obj);
            }

          g_free (str);

          mapping = mapping->next;
        }

      op->callback (op->source,
                    op->operation_id,
                    media,
                    --op->count,
                    op->user_data,
                    NULL);

      if (op->count == 0)
        break;
    }

 finalize:
  /* Signal the last element if it was not already signaled */
  if (nb_items == 0) {
    op->callback (op->source,
                  op->operation_id,
                  NULL,
                  0,
                  op->user_data,
                  NULL);
  }

  if (xpath)
    xmlXPathFreeContext (xpath);
  if (doc)
    xmlFreeDoc (doc);

  bliptv_operation_free (op);
}

/* ================== API Implementation ================ */

static const GList *
grl_bliptv_source_supported_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_PUBLICATION_DATE,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_MIME,
                                      GRL_METADATA_KEY_URL,
                                      GRL_METADATA_KEY_THUMBNAIL,
                                      NULL);
  }
  return keys;
}

static void
grl_bliptv_source_browse (GrlMediaSource *source,
                          GrlMediaSourceBrowseSpec *bs)
{
  BliptvOperation *op = g_slice_new0 (BliptvOperation);
  GError *error = NULL;
  gchar *length;
  gint count = grl_operation_options_get_count (bs->options);

  op->source       = g_object_ref (source);
  op->count        = count;
  op->operation_id = bs->browse_id;
  op->callback     = bs->callback;
  op->user_data    = bs->user_data;

  grl_operation_set_data (bs->browse_id, op);

  op->proxy = rest_proxy_new ("http://blip.tv/posts/", FALSE);
  op->call = rest_proxy_new_call (op->proxy);
  rest_proxy_call_add_param (op->call, "skin", "rss");
  length = g_strdup_printf ("%u", count);
  rest_proxy_call_add_param (op->call, "pagelen", length);
  g_free (length);

  GRL_DEBUG ("Starting browse request for id=%u", bs->browse_id);

  if (!rest_proxy_call_async (op->call,
                              proxy_call_raw_async_cb,
                              NULL,
                              op,
                              &error))
    {
      if (error)
        {
          GRL_WARNING ("Could not start search request : %s", error->message);
          g_error_free (error);
        }
      bs->callback (source, bs->browse_id, NULL, 0, bs->user_data, NULL);
      bliptv_operation_free (op);
    }
}

static void
grl_bliptv_source_search (GrlMediaSource *source,
                          GrlMediaSourceSearchSpec *ss)
{
  BliptvOperation *op = g_slice_new0 (BliptvOperation);
  GError *error = NULL;
  GError *grl_error;
  gchar *length;
  gint count = grl_operation_options_get_count (ss->options);

  op->source       = g_object_ref (source);
  op->count        = count;
  op->operation_id = ss->search_id;
  op->callback     = ss->callback;
  op->user_data    = ss->user_data;

  grl_operation_set_data (ss->search_id, op);

  op->proxy = rest_proxy_new ("http://blip.tv/posts/", FALSE);
  op->call = rest_proxy_new_call (op->proxy);
  rest_proxy_call_add_param (op->call, "skin", "rss");
  rest_proxy_call_add_param (op->call, "search", ss->text);
  length = g_strdup_printf ("%u", count);
  rest_proxy_call_add_param (op->call, "pagelen", length);
  g_free (length);

  GRL_DEBUG ("Starting search request for id=%u : '%s'",
             ss->search_id, ss->text);

  if (!rest_proxy_call_async (op->call,
                              proxy_call_raw_async_cb,
                              NULL,
                              op,
                              &error))
    {
      if (error)
        {
          GRL_WARNING ("Could not start search request : %s", error->message);
          g_error_free (error);
        }
      grl_error = g_error_new (GRL_CORE_ERROR,
                               GRL_CORE_ERROR_SEARCH_FAILED,
                               "Unable to search '%s'",
                               ss->text? ss->text: "");
      ss->callback (source, ss->search_id, NULL, 0, ss->user_data, grl_error);
      g_error_free (grl_error);
      bliptv_operation_free (op);
    }
}

static void
grl_bliptv_source_cancel (GrlMediaSource *source, guint operation_id)
{
  BliptvOperation *op = grl_operation_get_data (operation_id);

  GRL_WARNING ("Cancelling id=%u", operation_id);

  if (!op)
    {
      GRL_WARNING ("\tNo such operation id=%u", operation_id);
    }

  if (op->call)
    {
      if (!rest_proxy_call_cancel (op->call))
        {
          GRL_WARNING ("\tCannot cancel rest call id=%u", operation_id);
        }
    }

  grl_operation_set_data (operation_id, NULL);
  bliptv_operation_free (op);
}

static GrlCaps *
grl_bliptv_source_get_caps (GrlMetadataSource *source,
                            GrlSupportedOps operation)
{
  static GrlCaps *caps = NULL;

  if (caps == NULL)
    caps = grl_caps_new ();

  return caps;
}
