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

#include "grl-bliptv.h"

#include <net/grl-net.h>

#include <glib/gi18n-lib.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>


/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT bliptv_log_domain
GRL_LOG_DOMAIN_STATIC(bliptv_log_domain);


/* ----------- API ---------- */

#define MAX_ELEMENTS 100

#define BLIPTV_BACKEND "http://blip.tv"
#define BLIPTV_BROWSE  BLIPTV_BACKEND "/posts?skin=rss&page=%u"
#define BLIPTV_SEARCH  BLIPTV_BACKEND "/posts?search=%s&skin=rss&page=%%u"

/* --- Plugin information --- */

#define PLUGIN_ID   BLIPTV_PLUGIN_ID

#define SOURCE_ID   "grl-bliptv"
#define SOURCE_NAME "Blip.tv"
#define SOURCE_DESC _("A source for browsing and searching Blip.tv videos")


G_DEFINE_TYPE (GrlBliptvSource, grl_bliptv_source, GRL_TYPE_SOURCE)

#define BLIPTV_SOURCE_PRIVATE(o)                                        \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o),                                    \
                                GRL_TYPE_BLIPTV_SOURCE,                 \
                                GrlBliptvSourcePrivate))

struct _GrlBliptvSourcePrivate
{
  GrlNetWc *wc;
};

typedef struct
{
  GrlSource *source;
  guint      operation_id;
  guint      count;
  guint      skip;
  guint      page;
  gchar     *url;

  GrlSourceResultCb callback;
  gpointer          user_data;

  GCancellable *cancellable;
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

gboolean grl_bliptv_plugin_init (GrlRegistry *registry,
                                 GrlPlugin *plugin,
                                 GList *configs);

static const GList *grl_bliptv_source_supported_keys (GrlSource *source);

static void grl_bliptv_source_browse (GrlSource *source,
                                      GrlSourceBrowseSpec *bs);

static void grl_bliptv_source_search (GrlSource *source,
                                      GrlSourceSearchSpec *ss);

static void grl_bliptv_source_cancel (GrlSource *source,
                                      guint operation_id);

/**/

gboolean
grl_bliptv_plugin_init (GrlRegistry *registry,
                        GrlPlugin *plugin,
                        GList *configs)
{
  GRL_LOG_DOMAIN_INIT (bliptv_log_domain, "bliptv");

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  bliptv_setup_mapping ();

  GrlBliptvSource *source = grl_bliptv_source_new ();
  grl_registry_register_source (registry,
                                plugin,
                                GRL_SOURCE (source),
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
  GIcon *icon;
  GFile *file;
  GrlBliptvSource *source;
  const char *tags[] = {
    "net:internet",
    NULL
  };

  file = g_file_new_for_uri ("resource:///org/gnome/grilo/plugins/bliptv/channel-bliptv.svg");
  icon = g_file_icon_new (file);
  g_object_unref (file);
  source = g_object_new (GRL_TYPE_BLIPTV_SOURCE,
                         "source-id", SOURCE_ID,
                         "source-name", SOURCE_NAME,
                         "source-desc", SOURCE_DESC,
                         "supported-media", GRL_MEDIA_TYPE_VIDEO,
                         "source-icon", icon,
                         "source-tags", tags,
                         NULL);
  g_object_unref (icon);

  return source;
}

static void
grl_bliptv_source_dispose (GObject *object)
{
  GrlBliptvSource *self;

  self= GRL_BLIPTV_SOURCE (object);

  g_clear_object (&self->priv->wc);

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
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GrlBliptvSourcePrivate));

  object_class->dispose = grl_bliptv_source_dispose;
  object_class->finalize = grl_bliptv_source_finalize;

  source_class->supported_keys = grl_bliptv_source_supported_keys;
  source_class->cancel = grl_bliptv_source_cancel;
  source_class->browse = grl_bliptv_source_browse;
  source_class->search = grl_bliptv_source_search;
}

static void
grl_bliptv_source_init (GrlBliptvSource *self)
{
  self->priv = BLIPTV_SOURCE_PRIVATE (self);

  self->priv->wc = grl_net_wc_new ();

  grl_source_set_auto_split_threshold (GRL_SOURCE (self), MAX_ELEMENTS);
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
  g_clear_object (&op->cancellable);
  g_clear_object (&op->source);
  g_clear_pointer (&op->url, (GDestroyNotify) g_free);

  g_slice_free (BliptvOperation, op);
}

static void
call_raw_async_cb (GObject *     source_object,
                   GAsyncResult *res,
                   gpointer      data)
{
  BliptvOperation    *op = (BliptvOperation *) data;
  xmlDocPtr           doc = NULL;
  xmlXPathContextPtr  xpath = NULL;
  xmlXPathObjectPtr   obj = NULL;
  gint i, nb_items = 0;
  gchar *content = NULL;
  gchar *url;
  gsize length;

  GRL_DEBUG ("Response id=%u", op->operation_id);

  if (g_cancellable_is_cancelled (op->cancellable)) {
    goto finalize_send_last;
  }

  if (!grl_net_wc_request_finish (GRL_NET_WC (source_object),
                                  res,
                                  &content,
                                  &length,
                                  NULL)) {
    goto finalize_send_last;
  }

  doc = xmlParseMemory (content, (gint) length);

  if (!doc)
    goto finalize_send_last;

  xpath = xmlXPathNewContext (doc);
  if (!xpath)
    goto finalize_send_last;

  xmlXPathRegisterNs (xpath,
                      (xmlChar *) "blip",
                      (xmlChar *) BLIPTV_BACKEND "/dtd/blip/1.0");
  xmlXPathRegisterNs (xpath,
                      (xmlChar *) "media",
                      (xmlChar *) "http://search.yahoo.com/mrss/");

  obj = xmlXPathEvalExpression ((xmlChar *) "/rss/channel/item", xpath);
  if (obj)
    {
      nb_items = xmlXPathNodeSetGetLength (obj->nodesetval);
      xmlXPathFreeObject (obj);
    }

  if (nb_items == 0) {
    goto finalize_send_last;
  }

  /* Special case: when there are no results in a search, Blip.tv returns an
     element telling precisely that, no results found; so we need to report no
     results too */
  if (nb_items == 1) {
    obj = xmlXPathEvalExpression ((xmlChar *) "string(/rss/channel/item[0]/blip:item_id)", xpath);
    if (!obj || !obj->stringval || obj->stringval[0] == '\0') {
      g_clear_pointer (&obj, (GDestroyNotify) xmlXPathFreeObject);
      nb_items = 0;
      goto finalize_send_last;
    } else {
      xmlXPathFreeObject (obj);
    }
  }

  for (i = op->skip; i < nb_items; i++)
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

  if (op->count > 0) {
    /* Request next page */
    op->skip = 0;
    url = g_strdup_printf (op->url, ++op->page);

    GRL_DEBUG ("Operation %d: requesting page %d",
               op->operation_id,
               op->page);

    grl_net_wc_request_async (GRL_BLIPTV_SOURCE (op->source)->priv->wc,
                              url,
                              op->cancellable,
                              call_raw_async_cb,
                              op);
    g_free (url);

    goto finalize_free;
  }

 finalize_send_last:
  /* Signal the last element if it was not already signaled */
  if (nb_items == 0) {
    op->callback (op->source,
                  op->operation_id,
                  NULL,
                  0,
                  op->user_data,
                  NULL);
  }

 finalize_free:
  g_clear_pointer (&xpath, (GDestroyNotify) xmlXPathFreeContext);
  g_clear_pointer (&doc, (GDestroyNotify) xmlFreeDoc);
}

/* ================== API Implementation ================ */

static const GList *
grl_bliptv_source_supported_keys (GrlSource *source)
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
grl_bliptv_source_browse (GrlSource *source,
                          GrlSourceBrowseSpec *bs)
{
  BliptvOperation *op = g_slice_new0 (BliptvOperation);
  gchar *url;
  gint count = grl_operation_options_get_count (bs->options);
  guint page_number;
  guint page_offset;

  grl_paging_translate (grl_operation_options_get_skip (bs->options),
                        count,
                        MAX_ELEMENTS,
                        NULL,
                        &page_number,
                        &page_offset);

  op->source       = g_object_ref (source);
  op->cancellable  = g_cancellable_new ();
  op->count        = count;
  op->skip         = page_offset;
  op->page         = page_number;
  op->url          = g_strdup (BLIPTV_BROWSE);
  op->operation_id = bs->operation_id;
  op->callback     = bs->callback;
  op->user_data    = bs->user_data;

  grl_operation_set_data_full (bs->operation_id, op, (GDestroyNotify) bliptv_operation_free);

  url = g_strdup_printf (op->url, page_number);

  GRL_DEBUG ("Starting browse request for id=%u", bs->operation_id);

  grl_net_wc_request_async (GRL_BLIPTV_SOURCE (source)->priv->wc,
                            url,
                            op->cancellable,
                            call_raw_async_cb,
                            op);
  g_free (url);
}

static void
grl_bliptv_source_search (GrlSource *source,
                          GrlSourceSearchSpec *ss)
{
  BliptvOperation *op = g_slice_new0 (BliptvOperation);
  gchar *url;
  gint count = grl_operation_options_get_count (ss->options);
  guint page_number;
  guint page_offset;

  grl_paging_translate (grl_operation_options_get_skip (ss->options),
                        count,
                        MAX_ELEMENTS,
                        NULL,
                        &page_number,
                        &page_offset);

  op->source       = g_object_ref (source);
  op->cancellable  = g_cancellable_new ();
  op->count        = count;
  op->skip         = page_offset;
  op->page         = page_number;
  op->url          = g_strdup_printf (BLIPTV_SEARCH, ss->text);
  op->operation_id = ss->operation_id;
  op->callback     = ss->callback;
  op->user_data    = ss->user_data;


  grl_operation_set_data_full (ss->operation_id, op, (GDestroyNotify) bliptv_operation_free);

  url = g_strdup_printf (op->url, page_number);

  GRL_DEBUG ("Starting search request for id=%u : '%s'",
             ss->operation_id, ss->text);

  grl_net_wc_request_async (GRL_BLIPTV_SOURCE (source)->priv->wc,
                            url,
                            op->cancellable,
                            call_raw_async_cb,
                            op);
  g_free (url);
}

static void
grl_bliptv_source_cancel (GrlSource *source, guint operation_id)
{
  BliptvOperation *op = grl_operation_get_data (operation_id);

  GRL_DEBUG ("Cancelling id=%u", operation_id);

  if (!op)
    {
      GRL_WARNING ("\tNo such operation id=%u", operation_id);
      return;
    }

  if (op->cancellable) {
    g_cancellable_cancel (op->cancellable);
  }
}
