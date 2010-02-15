#include "gflickr.h"
#include <libxml/parser.h>
#include <gio/gio.h>

#define API_KEY     "fa037bee8120a921b34f8209d715a2fa"
#define AUTH_TOKEN  "72157623286932154-c90318d470e96a29"
#define AUTH_SECRET "9f6523b9c52e3317"

#define FLICKR_PHOTO_ORIG_URL                           \
  "http://farm%s.static.flickr.com/%s/%s_%s_o.%s"

#define FLICKR_ENDPOINT "http://api.flickr.com/services/rest/?"

#define FLICKR_PHOTOS_SEARCH_METHOD "flickr.photos.search"
#define FLICKR_PHOTOS_GETINFO_METHOD "flickr.photos.getInfo"

#define FLICKR_PHOTOS_SEARCH                    \
  FLICKR_ENDPOINT                               \
  "api_key=" API_KEY                            \
  "&auth_token=" AUTH_TOKEN                     \
  "&api_sig=%s"                                 \
  "&method=" FLICKR_PHOTOS_SEARCH_METHOD        \
  "&text=%s"

#define FLICKR_PHOTOS_GETINFO                   \
  FLICKR_ENDPOINT                               \
  "api_key=" API_KEY                            \
  "&auth_token=" AUTH_TOKEN                     \
  "&api_sig=%s"                                 \
  "&method=" FLICKR_PHOTOS_GETINFO_METHOD       \
  "&photo_id=%ld"

typedef void (*ParseXML) (const gchar *xml_result, gpointer user_data);

typedef struct {
  ParseXML parse_xml;
  GFlickrPhotoCb get_info_cb;
  gpointer user_data;
} GFlickrData;

/* -------------------- PRIVATE API -------------------- */

/* static gchar * */
/* get_api_sig_photos_search (const gchar *text) { */
/*   gchar *signature; */
/*   gchar *text_to_sign; */

/*   text_to_sign = g_strdup_printf (AUTH_SECRET */
/*                                   "method" */
/*                                   FLICKR_PHOTOS_SEARCH_METHOD */
/*                                   "text%s", text); */
/*   signature = g_compute_checksum_for_string (G_CHECKSUM_MD5, text_to_sign, -1); */
/*   g_free (text_to_sign); */

/*   return signature; */
/* } */

static gchar *
get_api_sig_photos_getInfo (glong photo_id)
{
  gchar *signature;
  gchar *text_to_sign;

  text_to_sign = g_strdup_printf (AUTH_SECRET
                                  "api_key"
                                  API_KEY
                                  "auth_token"
                                  AUTH_TOKEN
                                  "method"
                                  FLICKR_PHOTOS_GETINFO_METHOD
                                  "photo_id%ld", photo_id);
  signature = g_compute_checksum_for_string (G_CHECKSUM_MD5, text_to_sign, -1);
  g_free (text_to_sign);

  return signature;
}

static void
skip_garbage_nodes (xmlNodePtr *node)
{
  /* Result contains "\n" and "\t" to pretty align XML. Unfortunately, libxml
     doesn't cope very fine with them, and it creates "fakes" nodes with name
     "text" and value those characters. So we need to skip them */
  while ((*node) && xmlStrcmp ((*node)->name, (const xmlChar *) "text") == 0) {
    (*node) = (*node)->next;
  }
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
                         g_strdup ((gchar *) xmlGetProp (node, attr->name)));
  }
}

static GHashTable *
get_photo (xmlNodePtr node)
{
  GHashTable *photo = g_hash_table_new (g_str_hash, g_str_equal);

  /* Add photo node */
  add_node (node, photo);

  /* Add children nodes with their properties */

  node = node->xmlChildrenNode;
  skip_garbage_nodes (&node);

  while (node) {
    if (xmlStrcmp (node->name, (const xmlChar *) "owner") == 0 ||
        xmlStrcmp (node->name, (const xmlChar *) "dates") == 0) {
      add_node (node, photo);
    } else if (xmlStrcmp (node->name, (const xmlChar *) "title") == 0 ||
               xmlStrcmp (node->name, (const xmlChar *) "description") == 0) {
      g_hash_table_insert (photo,
                           g_strdup ((const gchar *) node->name),
                           g_strdup ((const gchar *) xmlNodeGetContent (node)));
    }

    node = node->next;
    skip_garbage_nodes (&node);
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

  doc = xmlRecoverDoc ((xmlChar *) xml_result);
  node = xmlDocGetRootElement (doc);

  /* Check result is ok */
  if (!node || !result_is_correct (node)) {
    data->get_info_cb (NULL, NULL, data->user_data);
  } else {
    node = node->xmlChildrenNode;
    skip_garbage_nodes (&node);

    photo = get_photo (node);
    data->get_info_cb (NULL, photo, data->user_data);
    g_hash_table_unref (photo);
  }
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
g_flickr_photos_getInfo (gpointer f,
                         glong photo_id,
                         GFlickrPhotoCb callback,
                         gpointer user_data)
{
  //g_return_if_fail (G_IS_FLICKR (f));

  gchar *api_sig = get_api_sig_photos_getInfo (photo_id);

  /* Build the request */
  gchar *request = g_strdup_printf (FLICKR_PHOTOS_GETINFO,
                                    api_sig,
                                    photo_id);
  g_free (api_sig);

  GFlickrData *gfd = g_new (GFlickrData, 1);
  gfd->parse_xml = process_photo_result;
  gfd->get_info_cb = callback;
  gfd->user_data = user_data;

  read_url_async (request, gfd);
  g_free (request);
}

gchar *
g_flickr_photo_url_original (gpointer f, GHashTable *photo)
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
