#include "gflickr.h"

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <gio/gio.h>
#include <string.h>

#define G_FLICKR_GET_PRIVATE(object)            \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),        \
                               G_FLICKR_TYPE,   \
                               GFlickrPrivate))

#define FLICKR_PHOTO_ORIG_URL                           \
  "http://farm%s.static.flickr.com/%s/%s_%s_o.%s"

#define FLICKR_PHOTO_THUMB_URL                          \
  "http://farm%s.static.flickr.com/%s/%s_%s_t.jpg"

#define FLICKR_ENDPOINT "http://api.flickr.com/services/rest/?"
#define FLICKR_AUTHPOINT "http://flickr.com/services/auth/?"

#define FLICKR_PHOTOS_SEARCH_METHOD "flickr.photos.search"
#define FLICKR_PHOTOS_GETINFO_METHOD "flickr.photos.getInfo"
#define FLICKR_TAGS_GETHOTLIST_METHOD "flickr.tags.getHotList"
#define FLICKR_AUTH_GETFROB_METHOD "flickr.auth.getFrob"
#define FLICKR_AUTH_GETTOKEN_METHOD "flickr.auth.getToken"
#define FLICKR_AUTH_CHECKTOKEN_METHOD "flickr.auth.checkToken"

#define FLICKR_PHOTOS_SEARCH                            \
  FLICKR_ENDPOINT                                       \
  "api_key=%s"                                          \
  "&api_sig=%s"                                         \
  "&method=" FLICKR_PHOTOS_SEARCH_METHOD                \
  "&extras=media,date_taken,owner_name,url_o,url_t"     \
  "&per_page=%d"                                        \
  "&page=%d"                                            \
  "&tags=%s"                                            \
  "&text=%s"                                            \
  "%s"

#define FLICKR_TAGS_GETHOTLIST                          \
  FLICKR_ENDPOINT                                       \
  "api_key=%s"                                          \
  "&api_sig=%s"                                         \
  "&method=" FLICKR_TAGS_GETHOTLIST_METHOD              \
  "&count=%d"                                           \
  "%s"

#define FLICKR_PHOTOS_GETINFO                   \
  FLICKR_ENDPOINT                               \
  "api_key=%s"                                  \
  "&api_sig=%s"                                 \
  "&method=" FLICKR_PHOTOS_GETINFO_METHOD       \
  "&photo_id=%ld"                               \
  "%s"

#define FLICKR_AUTH_GETFROB                     \
  FLICKR_ENDPOINT                               \
  "api_key=%s"                                  \
  "&api_sig=%s"                                 \
  "&method=" FLICKR_AUTH_GETFROB_METHOD

#define FLICKR_AUTH_GETTOKEN                    \
  FLICKR_ENDPOINT                               \
  "api_key=%s"                                  \
  "&api_sig=%s"                                 \
  "&method=" FLICKR_AUTH_GETTOKEN_METHOD        \
  "&frob=%s"

#define FLICKR_AUTH_CHECKTOKEN                  \
  FLICKR_ENDPOINT                               \
  "api_key=%s"                                  \
  "&api_sig=%s"                                 \
  "&method=" FLICKR_AUTH_CHECKTOKEN_METHOD      \
  "&auth_token=%s"

#define FLICKR_AUTH_LOGINLINK                   \
  FLICKR_AUTHPOINT                              \
  "api_key=%s"                                  \
  "&api_sig=%s"                                 \
  "&frob=%s"                                    \
  "&perms=%s"

typedef void (*ParseXML) (const gchar *xml_result, gpointer user_data);

typedef struct {
  ParseXML parse_xml;
  GFlickrHashTableCb hashtable_cb;
  GFlickrListCb list_cb;
  gpointer user_data;
} GFlickrData;

struct _GFlickrPrivate {
  gchar *api_key;
  gchar *auth_secret;
  gchar *auth_token;
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
g_flickr_new (const gchar *api_key, const gchar *auth_secret, const gchar *auth_token)
{
  g_return_val_if_fail (api_key && auth_secret, NULL);

  GFlickr *f = g_object_new (G_FLICKR_TYPE, NULL);
  f->priv->api_key = g_strdup (api_key);
  f->priv->auth_secret = g_strdup (auth_secret);
  f->priv->auth_token = g_strdup (auth_token);

  return f;
}

/* -------------------- PRIVATE API -------------------- */

static gchar *
get_api_sig (const gchar *secret, ...)
{
  GHashTable *hash;
  GList *key_iter;
  GList *keys;
  GString *to_sign;
  gchar *api_sig;
  gchar *key;
  gchar *value;
  gint text_size = strlen (secret);
  va_list va_params;

  hash = g_hash_table_new (g_str_hash, g_str_equal);

  va_start (va_params, secret);
  while ((key = va_arg (va_params, gchar *))) {
    text_size += strlen (key);
    value = va_arg (va_params, gchar *);
    text_size += strlen (value);
    g_hash_table_insert (hash, key, value);
  }
  va_end (va_params);

  to_sign = g_string_sized_new (text_size);
  g_string_append (to_sign, secret);

  keys = g_hash_table_get_keys (hash);
  keys = g_list_sort (keys, (GCompareFunc) g_strcmp0);
  for (key_iter = keys; key_iter; key_iter = g_list_next (key_iter)) {
    g_string_append (to_sign, key_iter->data);
    g_string_append (to_sign, g_hash_table_lookup (hash, key_iter->data));
  }

  api_sig = g_compute_checksum_for_string (G_CHECKSUM_MD5, to_sign->str, -1);
  g_hash_table_unref (hash);
  g_list_free (keys);
  g_string_free (to_sign, TRUE);

  return api_sig;
}

static gchar *
get_xpath_element (const gchar *content,
                   const gchar *xpath_element)
{
  gchar *element = NULL;
  xmlDocPtr xmldoc = NULL;
  xmlXPathContextPtr xpath_ctx = NULL;
  xmlXPathObjectPtr xpath_res = NULL;

  xmldoc = xmlReadMemory (content, xmlStrlen ((xmlChar *) content), NULL, NULL,
                          XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  if (xmldoc) {
    xpath_ctx = xmlXPathNewContext (xmldoc);
    if (xpath_ctx) {
      xpath_res = xmlXPathEvalExpression ((xmlChar *) xpath_element, xpath_ctx);
      if (xpath_res && xpath_res->nodesetval->nodeTab) {
        element =
          (gchar *) xmlNodeListGetString (xmldoc,
                                          xpath_res->nodesetval->nodeTab[0]->xmlChildrenNode,
                                          1);
      }
    }
  }

  /* Free data */
  if (xmldoc) {
    xmlFreeDoc (xmldoc);
  }

  if (xpath_ctx) {
    xmlXPathFreeContext (xpath_ctx);
  }

  if (xpath_res) {
    xmlXPathFreeObject (xpath_res);
  }

  return element;
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

static gchar *
get_tag (xmlNodePtr node)
{
  if (xmlStrcmp (node->name, (const xmlChar *) "tag") == 0) {
    return (gchar *) xmlNodeGetContent (node);
  } else {
    return NULL;
  }
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
    g_hash_table_insert (token,
                         g_strdup ((const gchar *) node->name),
                         (gchar *) xmlNodeGetContent (node));
    add_node (node, token);
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
  GHashTable *photo;

  doc = xmlReadMemory (xml_result, xmlStrlen ((xmlChar*) xml_result), NULL,
                       NULL, XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  node = xmlDocGetRootElement (doc);

  /* Check result is ok */
  if (!node || !result_is_correct (node)) {
    data->hashtable_cb (NULL, NULL, data->user_data);
  } else {
    node = node->xmlChildrenNode;

    photo = get_photo (node);
    data->hashtable_cb (NULL, photo, data->user_data);
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
    data->list_cb (NULL, NULL, data->user_data);
  } else {
    node = node->xmlChildrenNode;

    /* Now we're at "photo pages" node */
    node = node->xmlChildrenNode;
    while (node) {
      photolist = g_list_prepend (photolist, get_photo (node));
      node = node->next;
    }

    data->list_cb (NULL, g_list_reverse (photolist), data->user_data);
    g_list_foreach (photolist, (GFunc) g_hash_table_unref, NULL);
    g_list_free (photolist);
  }
  g_slice_free (GFlickrData, data);
  xmlFreeDoc (doc);
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
  node = xmlDocGetRootElement (doc);

  /* Check if result is OK */
  if (!node || !result_is_correct (node)) {
    data->list_cb (NULL, NULL, data->user_data);
  } else {
    node = node->xmlChildrenNode;

    /* Now we're at "hot tags" node */
    node = node->xmlChildrenNode;
    while (node) {
      taglist = g_list_prepend (taglist, get_tag (node));
      node = node->next;
    }

    data->list_cb (NULL, g_list_reverse (taglist), data->user_data);
    g_list_foreach (taglist, (GFunc) g_free, NULL);
    g_list_free (taglist);
  }
  g_slice_free (GFlickrData, data);
  xmlFreeDoc (doc);
}

static void
process_token_result (const gchar *xml_result, gpointer user_data)
{
  xmlDocPtr doc;
  xmlNodePtr node;
  GFlickrData *data = (GFlickrData *) user_data;
  GHashTable *token;

  doc = xmlReadMemory (xml_result, xmlStrlen ((xmlChar*) xml_result), NULL,
                       NULL, XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  node = xmlDocGetRootElement (doc);

  /* Check if result is OK */
  if (!node || !result_is_correct (node)) {
    data->hashtable_cb (NULL, NULL, data->user_data);
  } else {
    node = node->xmlChildrenNode;
    token = get_token_info (node);
    data->hashtable_cb (NULL, token, data->user_data);
    g_hash_table_unref (token);
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
  GFlickrData *data = (GFlickrData *) user_data;

  g_file_load_contents_finish (G_FILE (source_object),
                               res,
                               &content,
                               NULL,
                               NULL,
                               NULL);

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
                         GFlickrHashTableCb callback,
                         gpointer user_data)
{
  gchar *auth;

  g_return_if_fail (G_IS_FLICKR (f));

  gchar *str_photo_id = g_strdup_printf ("%ld", photo_id);
  gchar *api_sig = get_api_sig (f->priv->auth_secret,
                                "api_key", f->priv->api_key,
                                "method", FLICKR_PHOTOS_GETINFO_METHOD,
                                "photo_id", str_photo_id,
                                f->priv->auth_token? "auth_token": "",
                                f->priv->auth_token? f->priv->auth_token: "",
                                NULL);
  g_free (str_photo_id);

  /* Build the request */
  if (f->priv->auth_token) {
    auth = g_strdup_printf ("&auth_token=%s", f->priv->auth_token);
  } else {
    auth = g_strdup ("");
  }

  gchar *request = g_strdup_printf (FLICKR_PHOTOS_GETINFO,
                                    f->priv->api_key,
                                    api_sig,
                                    photo_id,
                                    auth);
  g_free (api_sig);
  g_free (auth);

  GFlickrData *gfd = g_slice_new (GFlickrData);
  gfd->parse_xml = process_photo_result;
  gfd->hashtable_cb = callback;
  gfd->user_data = user_data;

  read_url_async (request, gfd);
  g_free (request);
}

void
g_flickr_photos_search (GFlickr *f,
                        const gchar *text,
                        const gchar *tags,
                        gint page,
                        GFlickrListCb callback,
                        gpointer user_data)
{
  gchar *auth;
  g_return_if_fail (G_IS_FLICKR (f));

  if (!text) {
    text = "";
  }

  if (!tags) {
    tags = "";
  }

  gchar *strpage = g_strdup_printf ("%d", page);
  gchar *strperpage = g_strdup_printf ("%d", f->priv->per_page);

  gchar *api_sig =
    get_api_sig (f->priv->auth_secret,
                 "api_key", f->priv->api_key,
                 "extras", "media,date_taken,owner_name,url_o,url_t",
                 "method", FLICKR_PHOTOS_SEARCH_METHOD,
                 "page", strpage,
                 "per_page", strperpage,
                 "tags", tags,
                 "text", text,
                 f->priv->auth_token? "auth_token": "",
                 f->priv->auth_token? f->priv->auth_token: "",
                 NULL);
  g_free (strpage);
  g_free (strperpage);

  /* Build the request */
  if (f->priv->auth_token) {
    auth = g_strdup_printf ("&auth_token=%s", f->priv->auth_token);
  } else {
    auth = g_strdup ("");
  }

  gchar *request = g_strdup_printf (FLICKR_PHOTOS_SEARCH,
                                    f->priv->api_key,
                                    api_sig,
                                    f->priv->per_page,
                                    page,
                                    tags,
                                    text,
                                    auth);
  g_free (api_sig);
  g_free (auth);

  GFlickrData *gfd = g_slice_new (GFlickrData);
  gfd->parse_xml = process_photolist_result;
  gfd->list_cb = callback;
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

void
g_flickr_tags_getHotList (GFlickr *f,
                          gint count,
                          GFlickrListCb callback,
                          gpointer user_data)
{
  gchar *auth;

  g_return_if_fail (G_IS_FLICKR (f));

  gchar *strcount = g_strdup_printf ("%d", count);

  gchar *api_sig = get_api_sig (f->priv->auth_secret,
                                "api_key", f->priv->api_key,
                                "count", strcount,
                                "method", FLICKR_TAGS_GETHOTLIST_METHOD,
                                f->priv->auth_token? "auth_token": "",
                                f->priv->auth_token? f->priv->auth_token: "",
                                NULL);
  g_free (strcount);

  /* Build the request */
  if (f->priv->auth_token) {
    auth = g_strdup_printf ("&auth_token=%s", f->priv->auth_token);
  } else {
    auth = g_strdup ("");
  }
  gchar *request = g_strdup_printf (FLICKR_TAGS_GETHOTLIST,
                                    f->priv->api_key,
                                    api_sig,
                                    count,
                                    auth);
  g_free (api_sig);
  g_free (auth);

  GFlickrData *gfd = g_slice_new (GFlickrData);
  gfd->parse_xml = process_taglist_result;
  gfd->list_cb = callback;
  gfd->user_data = user_data;

  read_url_async (request, gfd);
  g_free (request);
}

gchar *
g_flickr_auth_getFrob (GFlickr *f)
{
  gchar *api_sig;
  gchar *url;
  GVfs *vfs;
  GFile *uri;
  gchar *contents;
  GError *error = NULL;
  gchar *frob = NULL;

  g_return_val_if_fail (G_IS_FLICKR (f), NULL);

  api_sig = get_api_sig (f->priv->auth_secret,
                         "api_key", f->priv->api_key,
                         "method", "flickr.auth.getFrob",
                         NULL);

  /* Build url */
  url = g_strdup_printf (FLICKR_AUTH_GETFROB,
                         f->priv->api_key,
                         api_sig);
  g_free (api_sig);

  /* Load content */
  vfs = g_vfs_get_default ();
  uri = g_vfs_get_file_for_uri (vfs, url);
  g_free (url);
  if (!g_file_load_contents (uri, NULL, &contents, NULL, NULL, &error)) {
    g_warning ("Unable to get Flickr's frob: %s", error->message);
    return NULL;
  }

  /* Get frob */
  frob = get_xpath_element (contents, "/rsp/frob");
  g_free (contents);
  if (!frob) {
    g_warning ("Can not get Flickr's frob");
  }

  return frob;
}

gchar *
g_flickr_auth_loginLink (GFlickr *f,
                         const gchar *frob,
                         const gchar *perm)
{
  gchar *api_sig;
  gchar *url;

  g_return_val_if_fail (G_IS_FLICKR (f), NULL);
  g_return_val_if_fail (frob, NULL);
  g_return_val_if_fail (perm, NULL);

  api_sig = get_api_sig (f->priv->auth_secret,
                         "api_key", f->priv->api_key,
                         "frob", frob,
                         "perms", perm,
                         NULL);

  url = g_strdup_printf (FLICKR_AUTH_LOGINLINK,
                         f->priv->api_key,
                         api_sig,
                         frob,
                         perm);
  g_free (api_sig);

  return url;
}

gchar *
g_flickr_auth_getToken (GFlickr *f,
                        const gchar *frob)
{
  GError *error = NULL;
  GFile *uri;
  GVfs *vfs;
  gchar *api_sig;
  gchar *contents;
  gchar *token;
  gchar *url;

  g_return_val_if_fail (G_IS_FLICKR (f), NULL);
  g_return_val_if_fail (frob, NULL);

  api_sig = get_api_sig (f->priv->auth_secret,
                         "method", "flickr.auth.getToken",
                         "api_key", f->priv->api_key,
                         "frob", frob,
                         NULL);

  /* Build url */
  url = g_strdup_printf (FLICKR_AUTH_GETTOKEN,
                         f->priv->api_key,
                         api_sig,
                         frob);
  g_free (api_sig);

  /* Load content */
  vfs = g_vfs_get_default ();
  uri = g_vfs_get_file_for_uri (vfs, url);
  g_free (url);
  if (!g_file_load_contents (uri, NULL, &contents, NULL, NULL, &error)) {
    g_warning ("Unable to get Flickr's token: %s", error->message);
    return NULL;
  }

  /* Get token */
  token = get_xpath_element (contents, "/rsp/auth/token");
  g_free (contents);
  if (!token) {
    g_warning ("Can not get Flickr's token");
  }

  return token;
}

void
g_flickr_auth_checkToken (GFlickr *f,
                          const gchar *token,
                          GFlickrHashTableCb callback,
                          gpointer user_data)
{
  gchar *api_sig;
  gchar *request;

  g_return_if_fail (G_IS_FLICKR (f));
  g_return_if_fail (token);
  g_return_if_fail (callback);

  api_sig = get_api_sig (f->priv->auth_secret,
                         "method", FLICKR_AUTH_CHECKTOKEN_METHOD,
                         "api_key", f->priv->api_key,
                         "auth_token", token,
                         NULL);

  /* Build request */
  request  = g_strdup_printf (FLICKR_AUTH_CHECKTOKEN,
                              f->priv->api_key,
                              api_sig,
                              token);
  g_free (api_sig);

  GFlickrData *gfd = g_slice_new (GFlickrData);
  gfd->parse_xml = process_token_result;
  gfd->hashtable_cb = callback;
  gfd->user_data = user_data;

  read_url_async (request, gfd);
  g_free (request);
}
