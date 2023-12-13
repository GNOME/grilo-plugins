/*
 * SPDX-FileCopyrightText: 2011-2013 Juan A. Suarez Romero <jasuarez@igalia.com>
 * SPDX-FileCopyrightText: 2013 Marek Chalupa <mchalupa@redhat.com>
 * SPDX-FileCopyrightText: 2016 Rafael Fonseca <r4f4rfs@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "gflickr.h"
#include "flickr-oauth.h"
#include "grl-flickr.h"       /* log domain */

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <gio/gio.h>
#include <string.h>

#include <grilo.h>
#include <net/grl-net.h>

#define GRL_LOG_DOMAIN_DEFAULT flickr_log_domain

#define FLICKR_PHOTO_ORIG_URL                           \
  "https://farm%s.static.flickr.com/%s/%s_%s_o.%s"

#define FLICKR_PHOTO_SMALL_URL                          \
  "https://farm%s.static.flickr.com/%s/%s_%s_n.jpg"

#define FLICKR_PHOTO_THUMB_URL                          \
  "https://farm%s.static.flickr.com/%s/%s_%s_t.jpg"

#define FLICKR_PHOTO_LARGEST_URL                        \
  "https://farm%s.static.flickr.com/%s/%s_%s_b.jpg"


#define FLICKR_PHOTOS_SEARCH_METHOD       "flickr.photos.search"
#define FLICKR_PHOTOS_GETEXIF_METHOD      "flickr.photos.getExif"
#define FLICKR_PHOTOS_GETINFO_METHOD      "flickr.photos.getInfo"
#define FLICKR_PHOTOS_GETRECENT_METHOD    "flickr.photos.getRecent"
#define FLICKR_PHOTOSETS_GETLIST_METHOD   "flickr.photosets.getList"
#define FLICKR_PHOTOSETS_GETPHOTOS_METHOD "flickr.photosets.getPhotos"
#define FLICKR_TAGS_GETHOTLIST_METHOD     "flickr.tags.getHotList"
#define FLICKR_OAUTH_CHECKTOKEN_METHOD    "flickr.auth.oauth.checkToken"


typedef void (*ParseXML) (const gchar *xml_result, gpointer user_data);

typedef struct {
  GFlickr *flickr;
  ParseXML parse_xml;
  GFlickrHashTableCb hashtable_cb;
  GFlickrListCb list_cb;
  gpointer user_data;
} GFlickrData;

struct _GFlickrPrivate {
  gchar *consumer_key;
  gchar *consumer_secret;
  gchar *oauth_token;
  gchar *oauth_token_secret;

  gint per_page;

  GrlNetWc *wc;
};

static void g_flickr_finalize (GObject *object);

/* -------------------- GOBJECT -------------------- */

G_DEFINE_TYPE_WITH_PRIVATE (GFlickr, g_flickr, G_TYPE_OBJECT)

static void
g_flickr_class_init (GFlickrClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = g_flickr_finalize;
}

static void
g_flickr_init (GFlickr *f)
{
  f->priv = g_flickr_get_instance_private (f);

  f->priv->consumer_key = NULL;
  f->priv->consumer_secret = NULL;
  f->priv->oauth_token = NULL;
  f->priv->oauth_token_secret = NULL;

  f->priv->per_page = 100;
}

static void
g_flickr_finalize (GObject *object)
{
  GFlickr *f = G_FLICKR (object);
  g_free (f->priv->consumer_key);
  g_free (f->priv->consumer_secret);
  g_free (f->priv->oauth_token);
  g_free (f->priv->oauth_token_secret);

  if (f->priv->wc)
    g_object_unref (f->priv->wc);

  G_OBJECT_CLASS (g_flickr_parent_class)->finalize (object);
}

GFlickr *
g_flickr_new (const gchar *consumer_key,
              const gchar *consumer_secret,
              const gchar *oauth_token,
              const gchar *oauth_token_secret)
{
  GFlickr *f;

  g_return_val_if_fail (consumer_key && consumer_secret, NULL);

  f = g_object_new (G_FLICKR_TYPE, NULL);
  f->priv->consumer_key = g_strdup (consumer_key);
  f->priv->consumer_secret = g_strdup (consumer_secret);

  if (oauth_token != NULL) {
    if (oauth_token_secret == NULL)
      GRL_WARNING ("No token secret given.");

    f->priv->oauth_token = g_strdup (oauth_token);
    f->priv->oauth_token_secret = g_strdup (oauth_token_secret);

   }

  return f;
}

/* -------------------- PRIVATE API -------------------- */

inline static gchar *
create_url (GFlickr *f, gchar **params, const guint params_no)
{
  return flickroauth_create_api_url (f->priv->consumer_key,
                                     f->priv->consumer_secret,
                                     f->priv->oauth_token,
                                     f->priv->oauth_token_secret,
                                     params, params_no);
}

inline static void
free_params (gchar **params, gint no)
{
  gint i = 0;
  for (; i < no; i++)
    g_free (params[i]);
}

static gboolean
result_is_correct (xmlNodePtr node)
{
  gboolean correct = FALSE;
  xmlChar *stat;

  if (xmlStrcmp (node->name, (const xmlChar *) "rsp") == 0) {
    stat = xmlGetProp (node, (const xmlChar *) "stat");
    if (stat) {
      if (xmlStrcmp (stat, (const xmlChar *) "ok") == 0) {
        correct = TRUE;
      }
      xmlFree (stat);
    }
  }

  return correct;
}

static void
add_node (xmlNodePtr node, GHashTable *photo)
{
  xmlAttrPtr attr;

  for (attr = node->properties; attr != NULL; attr = attr->next) {
    gchar *key;
    xmlChar *prop = xmlGetProp (node, attr->name);
    if (!prop)
      continue;

    key = g_strconcat ((const gchar *) node->name,
                       "_",
                       (const gchar *) attr->name,
                       NULL);
    g_hash_table_insert (photo, key, prop);
  }
}

static void
parse_exif_node (xmlNodePtr node, GHashTable *photo)
{
  xmlChar *tag;
  xmlChar *tagspace;
  xmlChar *content;
  xmlNodePtr child;

  tagspace = xmlGetProp (node, (const xmlChar *) "tagspace");
  if (!tagspace)
    return;

  /* We don't care for most of the tagspaces returned by Flickr, except the
   * two below. That way we don't polute the hash table with key/values which
   * will never be used
   */
  if (xmlStrcmp (tagspace, (const xmlChar *) "IFD0") != 0 &&
      xmlStrcmp (tagspace, (const xmlChar *) "ExifIFD") != 0) {
      xmlFree (tagspace);
      return;
  }
  xmlFree (tagspace);

  tag = xmlGetProp (node, (const xmlChar *) "tag");
  if (!tag)
    return;

  child = node->xmlChildrenNode;
  /* Empty tag? */
  if (!child)
    goto end;

  /* Exif nodes have at least one child containing "raw" exif data */
  while (child != NULL && xmlStrcmp (child->name, (const xmlChar *) "raw") != 0)
    child = child->next;

  if (!child)
    goto end;

  content = xmlNodeGetContent (child);
  if (content)
    g_hash_table_insert (photo, g_strdup ((const gchar *) tag), content);

end:
  xmlFree (tag);
}

static GHashTable *
get_photo (xmlNodePtr node)
{
  GHashTable *photo = g_hash_table_new_full (g_str_hash,
                                             g_str_equal,
                                             g_free,
                                             xmlFree);

  /* Add photo node */
  add_node (node, photo);

  /* Add children nodes with their properties */

  node = node->xmlChildrenNode;

  while (node) {
    if (xmlStrcmp (node->name, (const xmlChar *) "owner") == 0 ||
        xmlStrcmp (node->name, (const xmlChar *) "dates") == 0) {
      add_node (node, photo);
    } else if (xmlStrcmp (node->name, (const xmlChar *) "title") == 0 ||
               xmlStrcmp (node->name, (const xmlChar *) "description") == 0) {
      xmlChar *content = xmlNodeGetContent (node);

      if (content) {
        g_hash_table_insert (photo,
                             g_strdup ((const gchar *) node->name),
                             content);

      }
    } else if (xmlStrcmp (node->name, (const xmlChar *) "exif") == 0) {
      parse_exif_node (node, photo);
    }

    node = node->next;
  }

  return photo;
}

static GHashTable *
get_photoset (xmlNodePtr node)
{
  GHashTable *photoset = g_hash_table_new_full (g_str_hash,
                                             g_str_equal,
                                             g_free,
                                             g_free);

  /* Add photoset node */
  add_node (node, photoset);

  /* Add children nodes with their properties */
  node = node->xmlChildrenNode;

  while (node) {
    xmlChar *content = xmlNodeGetContent (node);

    if (content) {
      g_hash_table_insert (photoset,
                           g_strdup ((const gchar *) node->name),
                           content);

    }

    node = node->next;
  }

  return photoset;
}

static gchar *
get_tag (xmlNodePtr node)
{
  gchar *tag = NULL;

  if (xmlStrcmp (node->name, (const xmlChar *) "tag") == 0) {
    xmlChar *content = xmlNodeGetContent (node);
    if (content) {
      tag = g_strdup ((const gchar *)content);
      xmlFree (content);
    }
  }
  return tag;
}

static GHashTable *
get_token_info (xmlNodePtr node)
{
  GHashTable *token = g_hash_table_new_full (g_str_hash,
                                             g_str_equal,
                                             g_free,
                                             g_free);
  node = node->xmlChildrenNode;

  while (node) {
    xmlChar *content = xmlNodeGetContent (node);
    if (content) {
      g_hash_table_insert (token,
                           g_strdup ((const gchar *) node->name),
                           content);

      add_node (node, token);
    }
    node = node->next;
  }

  return token;
}

static void
process_photo_result (const gchar *xml_result, gpointer user_data)
{
  xmlDocPtr doc;
  xmlNodePtr node;
  GFlickrData *data = (GFlickrData *) user_data;
  GHashTable *photo = NULL;

  doc = xmlReadMemory (xml_result, xmlStrlen ((xmlChar*) xml_result), NULL,
                       NULL, XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  if (doc) {
    node = xmlDocGetRootElement (doc);

    /* Check result is ok */
    if (node && result_is_correct (node)) {
      node = node->xmlChildrenNode;

      photo = get_photo (node);
    }
    xmlFreeDoc (doc);
  }

  data->hashtable_cb (data->flickr, photo, data->user_data);
  if (photo) {
    g_hash_table_unref (photo);
  }
  g_object_unref (data->flickr);
  g_slice_free (GFlickrData, data);
}

static void
process_exif_result (const gchar *xml_result, gpointer user_data)
{
  xmlDocPtr doc;
  GHashTable *photo = NULL;
  GFlickrData *data = (GFlickrData *) user_data;

  doc = xmlReadMemory (xml_result, xmlStrlen ((xmlChar *) xml_result), NULL,
                       NULL, XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  if (doc) {
    xmlNodePtr node = xmlDocGetRootElement (doc);

    /* Check result is ok */
    if (node && result_is_correct (node)) {
      node = node->xmlChildrenNode;
      photo = get_photo (node);
    }
    xmlFreeDoc (doc);
  }

  data->hashtable_cb (data->flickr, photo, data->user_data);
  if (photo)
    g_hash_table_unref (photo);
  g_object_unref (data->flickr);
  g_slice_free (GFlickrData, data);
}

static void
process_photolist_result (const gchar *xml_result, gpointer user_data)
{
  GFlickrData *data = (GFlickrData *) user_data;
  GList *photolist = NULL;
  xmlDocPtr doc;
  xmlNodePtr node;

  doc = xmlReadMemory (xml_result, xmlStrlen ((xmlChar*) xml_result), NULL,
                       NULL, XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  if (doc) {
    node = xmlDocGetRootElement (doc);

    /* Check result is ok */
    if (node && result_is_correct (node)) {
      node = node->xmlChildrenNode;

      /* Now we're at "photo pages" node */
      node = node->xmlChildrenNode;
      while (node) {
        photolist = g_list_prepend (photolist, get_photo (node));
        node = node->next;
      }
      xmlFreeDoc (doc);
    }
  }

  if (photolist) {
    data->list_cb (data->flickr, g_list_reverse (photolist), data->user_data);
    g_list_free_full (photolist, (GDestroyNotify) g_hash_table_unref);
  } else {
    data->list_cb (data->flickr, NULL, data->user_data);
  }

  g_object_unref (data->flickr);
  g_slice_free (GFlickrData, data);
}

static void
process_taglist_result (const gchar *xml_result, gpointer user_data)
{
  GFlickrData *data = (GFlickrData *) user_data;
  GList *taglist = NULL;
  xmlDocPtr doc;
  xmlNodePtr node;

  doc = xmlReadMemory (xml_result, xmlStrlen ((xmlChar*) xml_result), NULL,
                       NULL, XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  if (doc) {
    node = xmlDocGetRootElement (doc);

    /* Check if result is OK */
    if (node && result_is_correct (node)) {
      node = node->xmlChildrenNode;

      /* Now we're at "hot tags" node */
      node = node->xmlChildrenNode;
      while (node) {
        taglist = g_list_prepend (taglist, get_tag (node));
        node = node->next;
      }

    }
    xmlFreeDoc (doc);
  }

  if (taglist) {
    data->list_cb (data->flickr, g_list_reverse (taglist), data->user_data);
    g_list_free_full (taglist, g_free);
  } else {
    data->list_cb (data->flickr, NULL, data->user_data);
  }

  g_object_unref (data->flickr);
  g_slice_free (GFlickrData, data);
}

static void
process_photosetslist_result (const gchar *xml_result, gpointer user_data)
{
  GFlickrData *data = (GFlickrData *) user_data;
  GList *photosets = NULL;
  xmlDocPtr doc;
  xmlNodePtr node;

  doc = xmlReadMemory (xml_result, xmlStrlen ((xmlChar*) xml_result), NULL,
                       NULL, XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  if (doc) {
    node = xmlDocGetRootElement (doc);

    /* Check if result is OK */
    if (node && result_is_correct (node)) {
      node = node->xmlChildrenNode;

      /* Now we're at "photosets" node */
      node = node->xmlChildrenNode;
      while (node) {
        photosets = g_list_prepend (photosets, get_photoset (node));
        node = node->next;
      }
    }
    xmlFreeDoc (doc);
  }

  if (photosets) {
    data->list_cb (data->flickr, g_list_reverse (photosets), data->user_data);
    g_list_free_full (photosets, (GDestroyNotify) g_hash_table_unref);
  } else {
    data->list_cb (data->flickr, NULL, data->user_data);
  }
  g_object_unref (data->flickr);
  g_slice_free (GFlickrData, data);
}

static void
process_photosetsphotos_result (const gchar *xml_result, gpointer user_data)
{
  GFlickrData *data = (GFlickrData *) user_data;
  GList *list = NULL;
  xmlDocPtr doc;
  xmlNodePtr node;

  doc = xmlReadMemory (xml_result, xmlStrlen ((xmlChar*) xml_result), NULL,
                       NULL, XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  if (doc) {
    node = xmlDocGetRootElement (doc);

    /* Check result is ok */
    if (node && result_is_correct (node)) {
      node = node->xmlChildrenNode;

      /* Now we're at "photoset page" node */
      node = node->xmlChildrenNode;
      while (node) {
        list = g_list_prepend (list, get_photo (node));
        node = node->next;
      }
    }
    xmlFreeDoc (doc);
  }

  if (list) {
    data->list_cb (data->flickr, g_list_reverse (list), data->user_data);
    g_list_free_full (list, (GDestroyNotify) g_hash_table_unref);
  } else {
    data->list_cb (data->flickr, NULL, data->user_data);
  }
  g_object_unref (data->flickr);
  g_slice_free (GFlickrData, data);
}

static void
process_token_result (const gchar *xml_result, gpointer user_data)
{
  xmlDocPtr doc;
  xmlNodePtr node;
  GFlickrData *data = (GFlickrData *) user_data;
  GHashTable *token = NULL;

  doc = xmlReadMemory (xml_result, xmlStrlen ((xmlChar*) xml_result), NULL,
                       NULL, XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  if (doc) {
    node = xmlDocGetRootElement (doc);

    /* Check if result is OK */
    if (node && result_is_correct (node)) {
      node = node->xmlChildrenNode;
      token = get_token_info (node);
    }
    xmlFreeDoc (doc);
  }

  data->hashtable_cb (data->flickr, token, data->user_data);
  if (token)
    g_hash_table_unref (token);
  g_object_unref (data->flickr);
  g_slice_free (GFlickrData, data);
}

inline static GrlNetWc *
get_wc (GFlickr *f)
{
  if (!f->priv->wc)
    f->priv->wc = grl_net_wc_new ();

  return f->priv->wc;
}

static void
read_done_cb (GObject *source_object,
              GAsyncResult *res,
              gpointer user_data)
{
  gchar *content = NULL;
  GError *wc_error = NULL;
  GFlickrData *data = (GFlickrData *) user_data;

  grl_net_wc_request_finish (GRL_NET_WC (source_object),
                         res,
                         &content,
                         NULL,
                         &wc_error);

  data->parse_xml (content, user_data);
}

static void
read_url_async (GFlickr *f,
                const gchar *url,
                gpointer user_data)
{
  GRL_DEBUG ("Opening '%s'", url);
  grl_net_wc_request_async (get_wc (f),
                        url,
                        NULL,
                        read_done_cb,
                        user_data);
}

/* -------------------- PUBLIC API -------------------- */

void
g_flickr_set_per_page (GFlickr *f, gint per_page)
{
  g_return_if_fail (G_IS_FLICKR (f));

  f->priv->per_page = per_page;
}

void
g_flickr_photos_getExif (GFlickr *f,
                         const gchar *photo_id,
                         GFlickrHashTableCb callback,
                         gpointer user_data)
{
  gchar *params[2];
  gchar *request;
  GFlickrData *gfd;

  g_return_if_fail (G_IS_FLICKR (f));

  params[0] = g_strdup_printf ("photo_id=%s", photo_id);
  params[1] = g_strdup_printf ("method=%s", FLICKR_PHOTOS_GETEXIF_METHOD);

  request = create_url (f, params, 2);
  free_params (params, 2);

  gfd = g_slice_new (GFlickrData);
  gfd->flickr = g_object_ref (f);
  gfd->parse_xml = process_exif_result;
  gfd->hashtable_cb = callback;
  gfd->user_data = user_data;

  read_url_async (f, request, gfd);
  g_free (request);
}

void
g_flickr_photos_getInfo (GFlickr *f,
                         const gchar *photo_id,
                         GFlickrHashTableCb callback,
                         gpointer user_data)
{
  gchar *params[2];
  gchar *request;
  GFlickrData *gfd;

  g_return_if_fail (G_IS_FLICKR (f));

  params[0] = g_strdup_printf ("photo_id=%s", photo_id);
  params[1] = g_strdup_printf ("method=%s", FLICKR_PHOTOS_GETINFO_METHOD);

  request = create_url (f, params, 2);

  free_params (params, 2);

  gfd = g_slice_new (GFlickrData);
  gfd->flickr = g_object_ref (f);
  gfd->parse_xml = process_photo_result;
  gfd->hashtable_cb = callback;
  gfd->user_data = user_data;

  read_url_async (f, request, gfd);
  g_free (request);
}

void
g_flickr_photos_search (GFlickr *f,
                        const gchar *user_id,
                        const gchar *text,
                        const gchar *tags,
                        gint page,
                        GFlickrListCb callback,
                        gpointer user_data)
{
  gchar *params[8];
  gchar *request;
  GFlickrData *gfd;

  g_return_if_fail (G_IS_FLICKR (f));

  if (user_id == NULL) {
    user_id = "";
  }

  if (text == NULL) {
    text = "";
  }

  if (tags == NULL) {
    tags = "";
  }

  params[0] = g_strdup ("extras=date_taken,owner_name,url_0,url_t");
  params[1] = g_strdup ("media=photos");
  params[2] = g_strdup_printf ("user_id=%s", user_id);
  params[3] = g_strdup_printf ("page=%d", page);
  params[4] = g_strdup_printf ("per_page=%d", f->priv->per_page);
  params[5] = g_strdup_printf ("tags=%s", tags);
  params[6] = g_strdup_printf ("text=%s", text);
  params[7] = g_strdup_printf ("method=%s", FLICKR_PHOTOS_SEARCH_METHOD);


  /* Build the request */

  request = create_url (f, params, 8);

  free_params (params, 8);

  gfd = g_slice_new (GFlickrData);
  gfd->flickr = g_object_ref (f);
  gfd->parse_xml = process_photolist_result;
  gfd->list_cb = callback;
  gfd->user_data = user_data;

  read_url_async (f, request, gfd);
  g_free (request);
}

void
g_flickr_photos_getRecent (GFlickr *f,
                           gint page,
                           GFlickrListCb callback,
                           gpointer user_data)
{
  gchar *params[5];
  gchar *request;
  GFlickrData *gfd;

  g_return_if_fail (G_IS_FLICKR (f));

  params[0] = g_strdup ("extras=date_taken,owner_name,url_o,url_t");
  params[1] = g_strdup ("media=photos");
  params[2] = g_strdup_printf ("method=%s", FLICKR_PHOTOS_GETRECENT_METHOD);
  params[3] = g_strdup_printf ("page=%d", page);
  params[4] = g_strdup_printf ("per_page=%d", f->priv->per_page);

  request = create_url (f, params, 5);

  free_params (params, 5);

  gfd = g_slice_new (GFlickrData);
  gfd->flickr = g_object_ref (f);
  gfd->parse_xml = process_photolist_result;
  gfd->list_cb = callback;
  gfd->user_data = user_data;

  read_url_async (f, request, gfd);
  g_free (request);
}

gchar *
g_flickr_photo_url_original (GFlickr *f, GHashTable *photo)
{
  gchar *extension;
  gchar *farm_id;
  gchar *o_secret;
  gchar *photo_id;
  gchar *server_id;

  if (!photo) {
    return NULL;
  }

  extension = g_hash_table_lookup (photo, "photo_originalformat");
  farm_id = g_hash_table_lookup (photo, "photo_farm");
  o_secret = g_hash_table_lookup (photo, "photo_originalsecret");
  photo_id = g_hash_table_lookup (photo, "photo_id");
  server_id = g_hash_table_lookup (photo, "photo_server");

  if (!extension || !farm_id || !o_secret || !photo_id || !server_id) {
    return NULL;
  } else {
    return g_strdup_printf (FLICKR_PHOTO_ORIG_URL,
                            farm_id,
                            server_id,
                            photo_id,
                            o_secret,
                            extension);
  }
}

gchar *
g_flickr_photo_url_small (GFlickr *f, GHashTable *photo)
{
  gchar *farm_id;
  gchar *secret;
  gchar *photo_id;
  gchar *server_id;

  if (!photo) {
    return NULL;
  }

  farm_id = g_hash_table_lookup (photo, "photo_farm");
  secret = g_hash_table_lookup (photo, "photo_secret");
  photo_id = g_hash_table_lookup (photo, "photo_id");
  server_id = g_hash_table_lookup (photo, "photo_server");

  if (!farm_id || !secret || !photo_id || !server_id) {
    return NULL;
  } else {
    return g_strdup_printf (FLICKR_PHOTO_SMALL_URL,
                            farm_id,
                            server_id,
                            photo_id,
                            secret);
  }
}

gchar *
g_flickr_photo_url_thumbnail (GFlickr *f, GHashTable *photo)
{
  gchar *farm_id;
  gchar *secret;
  gchar *photo_id;
  gchar *server_id;

  if (!photo) {
    return NULL;
  }

  farm_id = g_hash_table_lookup (photo, "photo_farm");
  secret = g_hash_table_lookup (photo, "photo_secret");
  photo_id = g_hash_table_lookup (photo, "photo_id");
  server_id = g_hash_table_lookup (photo, "photo_server");

  if (!farm_id || !secret || !photo_id || !server_id) {
    return NULL;
  } else {
    return g_strdup_printf (FLICKR_PHOTO_THUMB_URL,
                            farm_id,
                            server_id,
                            photo_id,
                            secret);
  }
}

gchar *
g_flickr_photo_url_largest (GFlickr *f, GHashTable *photo)
{
  gchar *farm_id;
  gchar *secret;
  gchar *photo_id;
  gchar *server_id;

  if (!photo) {
    return NULL;
  }

  farm_id = g_hash_table_lookup (photo, "photo_farm");
  secret = g_hash_table_lookup (photo, "photo_secret");
  photo_id = g_hash_table_lookup (photo, "photo_id");
  server_id = g_hash_table_lookup (photo, "photo_server");

  if (!farm_id || !secret || !photo_id || !server_id) {
    return NULL;
  } else {
    return g_strdup_printf (FLICKR_PHOTO_LARGEST_URL,
                            farm_id,
                            server_id,
                            photo_id,
                            secret);
  }
}

void
g_flickr_tags_getHotList (GFlickr *f,
                          gint count,
                          GFlickrListCb callback,
                          gpointer user_data)
{
  gchar *params[2];
  gchar *request;
  GFlickrData *gfd;

  g_return_if_fail (G_IS_FLICKR (f));

  params[0] = g_strdup_printf ("count=%d", count);
  params[1] = g_strdup_printf ("method=%s", FLICKR_TAGS_GETHOTLIST_METHOD);

  request = create_url (f, params, 2);

  free_params (params, 2);

  gfd = g_slice_new (GFlickrData);
  gfd->flickr = g_object_ref (f);
  gfd->parse_xml = process_taglist_result;
  gfd->list_cb = callback;
  gfd->user_data = user_data;

  read_url_async (f, request, gfd);
  g_free (request);
}

void
g_flickr_photosets_getList (GFlickr *f,
                           const gchar *user_id,
                           GFlickrListCb callback,
                           gpointer user_data)
{
  /* Either we insert user_id or not */
  gint params_no = (user_id == NULL) ? 1 : 2;

  gchar *params[2];
  gchar *request;
  GFlickrData *gfd;

  params[0] = g_strdup_printf ("method=%s", FLICKR_PHOTOSETS_GETLIST_METHOD);

  if (user_id != NULL)
    params[1] = g_strdup_printf ("user_id=%s", user_id);

  request = create_url (f, params, params_no);

  free_params (params, params_no);

  gfd = g_slice_new (GFlickrData);
  gfd->flickr = g_object_ref (f);
  gfd->parse_xml = process_photosetslist_result;
  gfd->list_cb = callback;
  gfd->user_data = user_data;

  read_url_async (f, request, gfd);
  g_free (request);
}

void
g_flickr_photosets_getPhotos (GFlickr *f,
                              const gchar *photoset_id,
                              gint page,
                              GFlickrListCb callback,
                              gpointer user_data)
{
  gchar *params[6];
  gchar *request;
  GFlickrData *gfd;

  g_return_if_fail (G_IS_FLICKR (f));
  g_return_if_fail (photoset_id);

  params[0] = g_strdup_printf ("photoset_id=%s", photoset_id);
  params[1] = g_strdup ("extras=date_taken,owner_name,url_o,url_t,media");
  params[2] = g_strdup ("media=photos");
  params[3] = g_strdup_printf ("page=%d", page);
  params[4] = g_strdup_printf ("per_page=%d", f->priv->per_page);
  params[5] = g_strdup_printf ("method=%s", FLICKR_PHOTOSETS_GETPHOTOS_METHOD);

  request = create_url (f, params, 6);

  free_params (params, 6);

  gfd = g_slice_new (GFlickrData);
  gfd->flickr = g_object_ref (f);
  gfd->parse_xml = process_photosetsphotos_result;
  gfd->list_cb = callback;
  gfd->user_data = user_data;

  read_url_async (f, request, gfd);
  g_free (request);
}

/* ---------- Helper authorization functions ---------- */

void
g_flickr_auth_checkToken (GFlickr *f,
                          const gchar *token,
                          GFlickrHashTableCb callback,
                          gpointer user_data)
{
  gchar *request;
  gchar *params[1];
  GFlickrData *gfd;

  g_return_if_fail (G_IS_FLICKR (f));
  g_return_if_fail (token);
  g_return_if_fail (callback);

  params[0] = g_strdup_printf ("method=%s", FLICKR_OAUTH_CHECKTOKEN_METHOD);

  request = create_url (f, params, 1);

  free_params (params, 1);

  gfd = g_slice_new (GFlickrData);
  gfd->flickr = g_object_ref (f);
  gfd->parse_xml = process_token_result;
  gfd->hashtable_cb = callback;
  gfd->user_data = user_data;

  read_url_async (f, request, gfd);
  g_free (request);
}

GDateTime *
g_flickr_parse_date (const gchar *date)
{
  /* See http://www.flickr.com/services/api/misc.dates.html */
  guint year, month, day, hours, minutes;
  gdouble seconds;

  sscanf (date, "%u-%u-%u %u:%u:%lf",
          &year, &month, &day, &hours, &minutes, &seconds);

  /* The date we get from flickr is expressed in the timezone of the camera,
   * which we cannot know, so we just go with utc */
  return g_date_time_new_utc (year, month, day, hours, minutes, seconds);
}
