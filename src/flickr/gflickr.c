#include "gflickr.h"
#include <libxml/parser.h>
#include <gio/gio.h>

#define G_FLICKR_GET_PRIVATE(object)            \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),        \
                               G_FLICKR_TYPE,   \
                               GFlickrPrivate))

#define FLICKR_PHOTO_ORIG_URL                           \
  "http://farm%s.static.flickr.com/%s/%s_%s_o.%s"

#define FLICKR_PHOTO_THUMB_URL                          \
  "http://farm%s.static.flickr.com/%s/%s_%s_t.jpg"

#define FLICKR_ENDPOINT "http://api.flickr.com/services/rest/?"

#define FLICKR_PHOTOS_SEARCH_METHOD "flickr.photos.search"
#define FLICKR_PHOTOS_GETINFO_METHOD "flickr.photos.getInfo"

#define FLICKR_PHOTOS_SEARCH                            \
  FLICKR_ENDPOINT                                       \
  "api_key=%s"                                          \
  "&auth_token=%s"                                      \
  "&api_sig=%s"                                         \
  "&method=" FLICKR_PHOTOS_SEARCH_METHOD                \
  "&extras=media,date_taken,owner_name,url_o,url_t"     \
  "&per_page=%d"                                        \
  "&page=%d"                                            \
  "&tags=%s"                                            \
  "&text=%s"

#define FLICKR_PHOTOS_GETINFO                   \
  FLICKR_ENDPOINT                               \
  "api_key=%s"                                  \
  "&auth_token=%s"                              \
  "&api_sig=%s"                                 \
  "&method=" FLICKR_PHOTOS_GETINFO_METHOD       \
  "&photo_id=%ld"

typedef void (*ParseXML) (const gchar *xml_result, gpointer user_data);

typedef struct {
  ParseXML parse_xml;
  GFlickrPhotoCb get_info_cb;
  GFlickrPhotoListCb search_cb;
  gpointer user_data;
} GFlickrData;

struct _GFlickrPrivate {
  gchar *api_key;
  gchar *auth_token;
  gchar *auth_secret;
  gint per_page;
};

static void g_flickr_finalize (GObject *object);

/* -------------------- GOBJECT -------------------- */

G_DEFINE_TYPE (GFlickr, g_flickr, G_TYPE_OBJECT);

static void
g_flickr_class_init (GFlickrClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = g_flickr_finalize;

  g_type_class_add_private (klass, sizeof (GFlickrPrivate));
}

static void
g_flickr_init (GFlickr *f)
{
  f->priv = G_FLICKR_GET_PRIVATE (f);
  f->priv->per_page = 100;
}

static void
g_flickr_finalize (GObject *object)
{
  GFlickr *f = G_FLICKR (object);
  g_free (f->priv->api_key);
  g_free (f->priv->auth_token);
  g_free (f->priv->auth_secret);

  G_OBJECT_CLASS (g_flickr_parent_class)->finalize (object);
}

GFlickr *
g_flickr_new (const gchar *api_key, const gchar *auth_token, const gchar *auth_secret)
{
  GFlickr *f = g_object_new (G_FLICKR_TYPE, NULL);
  f->priv->api_key = g_strdup (api_key);
  f->priv->auth_token = g_strdup (auth_token);
  f->priv->auth_secret = g_strdup (auth_secret);

  return f;
}

/* -------------------- PRIVATE API -------------------- */

static gchar *
get_api_sig_photos_search (GFlickr *f,
                           const gchar *text,
                           const gchar *tags,
                           gint page) {
  gchar *signature;
  gchar *text_to_sign;

  text_to_sign = g_strdup_printf ("%s"
                                  "api_key%s"
                                  "auth_token%s"
                                  "extrasmedia,date_taken,owner_name,url_o,url_t"
                                  "method" FLICKR_PHOTOS_SEARCH_METHOD
                                  "page%d"
                                  "per_page%d"
                                  "tags%s"
                                  "text%s",
                                  f->priv->auth_secret,
                                  f->priv->api_key,
                                  f->priv->auth_token,
                                  page,
                                  f->priv->per_page,
                                  tags,
                                  text);
  signature = g_compute_checksum_for_string (G_CHECKSUM_MD5, text_to_sign, -1);
  g_free (text_to_sign);

  return signature;
}

static gchar *
get_api_sig_photos_getInfo (GFlickr *f, glong photo_id)
{
  gchar *signature;
  gchar *text_to_sign;

  text_to_sign = g_strdup_printf ("%s"
                                  "api_key%s"
                                  "auth_token%s"
                                  "method" FLICKR_PHOTOS_GETINFO_METHOD
                                  "photo_id%ld",
                                  f->priv->auth_secret,
                                  f->priv->api_key,
                                  f->priv->auth_token,
                                  photo_id);
  signature = g_compute_checksum_for_string (G_CHECKSUM_MD5, text_to_sign, -1);
  g_free (text_to_sign);

  return signature;
}

static gboolean
result_is_correct (xmlNodePtr node)
{
  gboolean correct = FALSE;
  xmlChar *stat;

  if (xmlStrcmp (node->name, (const xmlChar *) "rsp") == 0) {
    stat = xmlGetProp (node, (const xmlChar *) "stat");
    if (stat && xmlStrcmp (stat, (const xmlChar *) "ok") == 0) {
      correct = TRUE;
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
    g_hash_table_insert (photo,
                         g_strconcat ((const gchar *) node->name,
                                      "_",
                                      (const gchar *) attr->name,
                                      NULL),
                         (gchar *) xmlGetProp (node, attr->name));
  }
}

static GHashTable *
get_photo (xmlNodePtr node)
{
  GHashTable *photo = g_hash_table_new_full (g_str_hash,
                                             g_str_equal,
                                             g_free,
                                             g_free);

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
      g_hash_table_insert (photo,
                           g_strdup ((const gchar *) node->name),
                           (gchar *) xmlNodeGetContent (node));
    }

    node = node->next;
  }

  return photo;
}

static void
process_photo_result (const gchar *xml_result, gpointer user_data)
{
  xmlDocPtr doc;
  xmlNodePtr node;
  GFlickrData *data = (GFlickrData *) user_data;
  GHashTable *photo;

  doc = xmlReadMemory (xml_result, xmlStrlen ((xmlChar*) xml_result), NULL,
                       NULL, XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  node = xmlDocGetRootElement (doc);

  /* Check result is ok */
  if (!node || !result_is_correct (node)) {
    data->get_info_cb (NULL, NULL, data->user_data);
  } else {
    node = node->xmlChildrenNode;

    photo = get_photo (node);
    data->get_info_cb (NULL, photo, data->user_data);
    g_hash_table_unref (photo);
  }
  g_slice_free (GFlickrData, data);
  xmlFreeDoc (doc);
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
  node = xmlDocGetRootElement (doc);

  /* Check result is ok */
  if (!node || !result_is_correct (node)) {
    data->search_cb (NULL, NULL, data->user_data);
  } else {
    node = node->xmlChildrenNode;

    /* Now we're at "photo pages" node */
    node = node->xmlChildrenNode;
    while (node) {
      photolist = g_list_prepend (photolist, get_photo (node));
      node = node->next;
    }

    data->search_cb (NULL, g_list_reverse (photolist), data->user_data);
    g_list_foreach (photolist, (GFunc) g_hash_table_unref, NULL);
    g_list_free (photolist);
  }
  g_slice_free (GFlickrData, data);
  xmlFreeDoc (doc);
}

static void
read_done_cb (GObject *source_object,
              GAsyncResult *res,
              gpointer user_data)
{
  gchar *content = NULL;
  GError *vfs_error;
  GFlickrData *data = (GFlickrData *) user_data;

  g_file_load_contents_finish (G_FILE (source_object),
                               res,
                               &content,
                               NULL,
                               NULL,
                               &vfs_error);

  g_object_unref (source_object);

  data->parse_xml (content, user_data);
  g_free (content);
}

static void
read_url_async (const gchar *url, gpointer data)
{
  GVfs *vfs;
  GFile *uri;

  vfs = g_vfs_get_default ();
  g_debug ("Opening '%s'", url);
  uri = g_vfs_get_file_for_uri (vfs, url);
  g_file_load_contents_async (uri, NULL, read_done_cb, data);
}

/* -------------------- PUBLIC API -------------------- */

void
g_flickr_set_per_page (GFlickr *f, gint per_page)
{
  g_return_if_fail (G_IS_FLICKR (f));

  f->priv->per_page = per_page;
}

void
g_flickr_photos_getInfo (GFlickr *f,
                         glong photo_id,
                         GFlickrPhotoCb callback,
                         gpointer user_data)
{
  g_return_if_fail (G_IS_FLICKR (f));

  gchar *api_sig = get_api_sig_photos_getInfo (f, photo_id);

  /* Build the request */
  gchar *request = g_strdup_printf (FLICKR_PHOTOS_GETINFO,
                                    f->priv->api_key,
                                    f->priv->auth_token,
                                    api_sig,
                                    photo_id);
  g_free (api_sig);

  GFlickrData *gfd = g_slice_new (GFlickrData);
  gfd->parse_xml = process_photo_result;
  gfd->get_info_cb = callback;
  gfd->user_data = user_data;

  read_url_async (request, gfd);
  g_free (request);
}

void
g_flickr_photos_search (GFlickr *f,
                        const gchar *text,
                        const gchar *tags,
                        gint page,
                        GFlickrPhotoListCb callback,
                        gpointer user_data)
{
  g_return_if_fail (G_IS_FLICKR (f));

  if (!text) {
    text = "";
  }

  if (!tags) {
    tags = "";
  }

  gchar *api_sig = get_api_sig_photos_search (f, text, tags, page);

  /* Build the request */
  gchar *request = g_strdup_printf (FLICKR_PHOTOS_SEARCH,
                                    f->priv->api_key,
                                    f->priv->auth_token,
                                    api_sig,
                                    f->priv->per_page,
                                    page,
                                    tags,
                                    text);
  g_free (api_sig);

  GFlickrData *gfd = g_slice_new (GFlickrData);
  gfd->parse_xml = process_photolist_result;
  gfd->search_cb = callback;
  gfd->user_data = user_data;

  read_url_async (request, gfd);
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
