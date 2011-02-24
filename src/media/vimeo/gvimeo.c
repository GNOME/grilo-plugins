/*
 * Copyright (C) 2010 Igalia S.L.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Joaquim Rocha <jrocha@igalia.com>
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

#include <gcrypt.h>
#include <libsoup/soup.h>
#include "gvimeo.h"
#include <libxml/parser.h>
#include <libxml/xpath.h>

#define G_VIMEO_GET_PRIVATE(object)             \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),        \
                               G_VIMEO_TYPE,    \
                               GVimeoPrivate))

#define PLUGIN_USER_AGENT             "Grilo Vimeo Plugin"

#define VIMEO_ENDPOINT                "http://vimeo.com/api/rest/v2"
#define VIMEO_VIDEO_LOAD_URL          "http://vimeo.com/moogaloop/load/clip:"
#define VIMEO_VIDEO_PLAY_URL          "http://vimeo.com/moogaloop/play/clip:"

#define VIMEO_VIDEO_SEARCH_METHOD     "vimeo.videos.search"
#define VIMEO_API_OAUTH_SIGN_METHOD   "HMAC-SHA1"
#define VIMEO_API_OAUTH_SIGNATURE_PARAM "&oauth_signature=%s"

#define VIMEO_VIDEO_SEARCH					\
  "full_response=yes"						\
  "&method=%s"							\
  "&oauth_consumer_key=%s"					\
  "&oauth_nonce=%s"						\
  "&oauth_signature_method=" VIMEO_API_OAUTH_SIGN_METHOD	\
  "&oauth_timestamp=%s"						\
  "&oauth_token="						\
  "&page=%d"							\
  "&per_page=%d"						\
  "&query=%s"

typedef struct {
  GVimeo *vimeo;
  GVimeoVideoSearchCb search_cb;
  gpointer user_data;
} GVimeoVideoSearchData;

typedef struct {
  GVimeo *vimeo;
  gint video_id;
  GVimeoURLCb callback;
  gpointer user_data;
} GVimeoVideoURLData;

struct _GVimeoPrivate {
  gchar *api_key;
  gchar *auth_token;
  gchar *auth_secret;
  gint per_page;
  SoupSession *async_session;
};

enum InfoType {SIMPLE, EXTENDED};

typedef struct {
  enum InfoType type;
  gchar *name;
} VideoInfo;

static VideoInfo video_info[] = {{SIMPLE, VIMEO_VIDEO_TITLE},
				 {SIMPLE, VIMEO_VIDEO_DESCRIPTION},
				 {SIMPLE, VIMEO_VIDEO_UPLOAD_DATE},
				 {SIMPLE, VIMEO_VIDEO_WIDTH},
				 {SIMPLE, VIMEO_VIDEO_HEIGHT},
				 {SIMPLE, VIMEO_VIDEO_OWNER},
				 {SIMPLE, VIMEO_VIDEO_URL},
				 {SIMPLE, VIMEO_VIDEO_THUMBNAIL},
				 {SIMPLE, VIMEO_VIDEO_DURATION},
				 {EXTENDED, VIMEO_VIDEO_OWNER}};

static void g_vimeo_finalize (GObject *object);
static gchar * encode_uri (const gchar *uri);

/* -------------------- GOBJECT -------------------- */

G_DEFINE_TYPE (GVimeo, g_vimeo, G_TYPE_OBJECT);

static void
g_vimeo_class_init (GVimeoClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = g_vimeo_finalize;

  g_type_class_add_private (klass, sizeof (GVimeoPrivate));
}

static void
g_vimeo_init (GVimeo *vimeo)
{
  vimeo->priv = G_VIMEO_GET_PRIVATE (vimeo);
  vimeo->priv->per_page = 50;
  vimeo->priv->async_session = soup_session_async_new ();
}

static void
g_vimeo_finalize (GObject *object)
{
  GVimeo *vimeo = G_VIMEO (object);
  g_free (vimeo->priv->api_key);
  g_free (vimeo->priv->auth_secret);
  g_object_unref (vimeo->priv->async_session);

  G_OBJECT_CLASS (g_vimeo_parent_class)->finalize (object);
}

GVimeo *
g_vimeo_new (const gchar *api_key, const gchar *auth_secret)
{
  GVimeo *vimeo = g_object_new (G_VIMEO_TYPE, NULL);
  vimeo->priv->api_key = g_strdup (api_key);
  vimeo->priv->auth_secret = g_strdup (auth_secret);

  return vimeo;
}

/* -------------------- PRIVATE API -------------------- */

static gchar *
get_timestamp (void)
{
  time_t t = time (NULL);
  return g_strdup_printf ("%d", (gint) t);
}

static gchar *
get_nonce (void)
{
  gchar *timestamp = get_timestamp();
  guint rnd_number = g_random_int ();
  gchar *rnd_str = g_strdup_printf ("%d_%s", rnd_number, timestamp);
  gchar *nonce = g_compute_checksum_for_string (G_CHECKSUM_MD5, rnd_str, -1);
  g_free (timestamp);
  g_free (rnd_str);

  return nonce;
}

static gchar *
get_videos_search_params (GVimeo *vimeo, const gchar *text, gint page) {
  gchar *encoded_text = encode_uri (text);
  gchar *timestamp = get_timestamp ();
  gchar *nonce = get_nonce ();
  gchar *params = g_strdup_printf (VIMEO_VIDEO_SEARCH,
				   VIMEO_VIDEO_SEARCH_METHOD,
				   vimeo->priv->api_key,
				   nonce,
				   timestamp,
				   page,
				   vimeo->priv->per_page,
				   encoded_text);
  g_free (timestamp);
  g_free (nonce);
  g_free (encoded_text);

  return params;
}

static gchar *
sign_string (gchar *message, gchar *key)
{
  gchar *signed_message = NULL;
  gcry_md_hd_t digest_obj;
  unsigned char *hmac_digest;
  guint digest_len;

  gcry_md_open(&digest_obj,
	       GCRY_MD_SHA1,
	       GCRY_MD_FLAG_SECURE | GCRY_MD_FLAG_HMAC);
  gcry_md_setkey(digest_obj, key, strlen (key));
  gcry_md_write (digest_obj, message, strlen (message));
  gcry_md_final (digest_obj);
  hmac_digest = gcry_md_read (digest_obj, 0);

  digest_len = gcry_md_get_algo_dlen (GCRY_MD_SHA1);
  signed_message = g_base64_encode (hmac_digest, digest_len);

  gcry_md_close (digest_obj);

  return signed_message;
}

static gboolean
result_is_correct (xmlNodePtr node)
{
  gboolean correct = FALSE;
  xmlChar *stat;

  if (xmlStrcmp (node->name, (const xmlChar *) "rsp") == 0)
  {
    stat = xmlGetProp (node, (const xmlChar *) "stat");
    if (stat && xmlStrcmp (stat, (const xmlChar *) "ok") == 0)
    {
      correct = TRUE;
      xmlFree (stat);
    }
  }

  return correct;
}

static void
add_node (xmlNodePtr node, GHashTable *video)
{
  xmlAttrPtr attr;

  for (attr = node->properties; attr != NULL; attr = attr->next)
  {
    g_hash_table_insert (video,
                         g_strconcat ((const gchar *) node->name,
                                      "_",
                                      (const gchar *) attr->name,
                                      NULL),
                         (gchar *) xmlGetProp (node, attr->name));
  }
}

static xmlNodePtr
xpath_get_node (xmlXPathContextPtr context, gchar *xpath_expr)
{
  xmlNodePtr node = NULL;
  xmlXPathObjectPtr xpath_obj;
  xpath_obj = xmlXPathEvalExpression ((xmlChar *) xpath_expr, context);

  if (xpath_obj && xpath_obj->nodesetval->nodeTab)
  {
    node = xpath_obj->nodesetval->nodeTab[0];
  }
  xmlXPathFreeObject (xpath_obj);

  return node;
}

static gchar *
get_node_text (xmlXPathContextPtr context, gchar *xpath_expr)
{
  xmlNodePtr node;
  gchar *node_text = NULL;

  node = xpath_get_node (context, xpath_expr);
  if (node)
  {
    node_text = (gchar *) xmlNodeGetContent (node);
  }

  return node_text;
}

static GHashTable *
get_video (xmlNodePtr node)
{
  gint i;
  gint array_length;
  xmlXPathContextPtr context;
  gchar *video_id;
  GHashTable *video = g_hash_table_new_full (g_str_hash,
                                             g_str_equal,
                                             g_free,
                                             g_free);

  /* Adds the video node's properties */
  add_node (node, video);

  context = xmlXPathNewContext (node->doc);
  video_id = (gchar *) xmlGetProp (node, (xmlChar *) "id");

  array_length = G_N_ELEMENTS (video_info);
  for (i = 0; i < array_length; i++)
  {
    /* Look for the wanted information only under the current video */
    gchar *xpath_name = g_strdup_printf ("//video[@id=%s]//%s",
					 video_id,
					 video_info[i].name);
    xmlNodePtr info_node = xpath_get_node (context, xpath_name);
    if (info_node)
    {
      if (video_info[i].type == EXTENDED) {
	add_node (info_node, video);
      }
      else
      {
	g_hash_table_insert (video,
			     g_strdup ((const gchar *) info_node->name),
			     (gchar *) xmlNodeGetContent (info_node));
      }
    }
    g_free (xpath_name);
  }
  g_free (video_id);

  xmlXPathFreeContext (context);

  return video;
}


static void
process_video_search_result (const gchar *xml_result, gpointer user_data)
{
  xmlDocPtr doc;
  xmlNodePtr node;
  GList *video_list = NULL;
  GVimeoVideoSearchData *data = (GVimeoVideoSearchData *) user_data;

  doc = xmlReadMemory (xml_result,
		       xmlStrlen ((xmlChar *) xml_result),
		       NULL,
		       NULL,
		       XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  node = xmlDocGetRootElement (doc);

  /* Check result is ok */
  if (!node || !result_is_correct (node))
  {
    data->search_cb (data->vimeo, NULL, data->user_data);
  }
  else
  {
    node = node->xmlChildrenNode;

    /* Now we're at "video pages" node */
    node = node->xmlChildrenNode;
    while (node)
    {
      video_list = g_list_prepend (video_list, get_video (node));
      node = node->next;
    }

    data->search_cb (data->vimeo, g_list_reverse (video_list), data->user_data);
    g_list_foreach (video_list, (GFunc) g_hash_table_unref, NULL);
    g_list_free (video_list);
  }
  g_slice_free (GVimeoVideoSearchData, data);
  xmlFreeDoc (doc);
}

static void
search_videos_complete_cb (SoupSession *session,
			   SoupMessage *message,
			   gpointer *data)
{
  GVimeoVideoSearchCb *search_data = (GVimeoVideoSearchCb *) data;
  process_video_search_result (message->response_body->data, search_data);
}

static gchar *
get_play_url_from_vimeo_xml (const gchar *xml, gint video_id)
{
  xmlDocPtr doc = xmlRecoverDoc ((xmlChar *) xml);
  xmlXPathContextPtr context = xmlXPathNewContext (doc);
  gchar *request_signature = get_node_text (context,
					    "/xml/request_signature[1]");
  gchar *signature_expires = get_node_text (context,
					    "/xml/request_signature_expires[1]");

  gchar *url = g_strdup_printf ("%s%d/%s/%s/?q=sd",
				VIMEO_VIDEO_PLAY_URL,
			        video_id,
				request_signature,
				signature_expires);

  g_free (request_signature);
  g_free (signature_expires);
  xmlXPathFreeContext (context);
  xmlFreeDoc (doc);

  return url;
}

static void
get_video_play_url_complete_cb (SoupSession *session,
				SoupMessage *message,
				gpointer *data)
{
  GVimeoVideoURLData *url_data;
  gchar *url;

  if (message->response_body == NULL)
  {
    return;
  }

  url_data = (GVimeoVideoURLData *) data;
  url =  get_play_url_from_vimeo_xml (message->response_body->data,
				      url_data->video_id);
  url_data->callback (url, url_data->user_data);
  g_slice_free (GVimeoVideoURLData, url_data);
}

static gchar *
encode_uri (const gchar *uri)
{
  return soup_uri_encode (uri, "%!*'();:@&=+$,/?#[] ");
}

static gchar *
build_request (GVimeo *vimeo, const gchar *query, gint page)
{
  gchar *params;
  gchar *endpoint_encoded;
  gchar *key;
  gchar *escaped_str;
  gchar *tmp_str;
  gchar *signature;

  g_return_val_if_fail (G_IS_VIMEO (vimeo), NULL);

  params = get_videos_search_params (vimeo, query, page);
  endpoint_encoded = encode_uri (VIMEO_ENDPOINT);
  key = g_strdup_printf ("%s&", vimeo->priv->auth_secret);
  escaped_str = encode_uri (params);
  tmp_str = g_strdup_printf ("GET&%s&%s", endpoint_encoded, escaped_str);
  signature = sign_string (tmp_str, key);
  g_free (escaped_str);
  g_free (tmp_str);
  escaped_str = encode_uri (signature);
  tmp_str = g_strdup_printf ("%s?%s" VIMEO_API_OAUTH_SIGNATURE_PARAM,
			     VIMEO_ENDPOINT,
			     params,
			     escaped_str);

  g_free (endpoint_encoded);
  g_free (params);
  g_free (key);
  g_free (escaped_str);
  g_free (signature);

  return tmp_str;
}

/* -------------------- PUBLIC API -------------------- */

void
g_vimeo_set_per_page (GVimeo *vimeo, gint per_page)
{
  g_return_if_fail (G_IS_VIMEO (vimeo));
  vimeo->priv->per_page = per_page;
}

void
g_vimeo_videos_search (GVimeo *vimeo,
		       const gchar *text,
		       gint page,
		       GVimeoVideoSearchCb callback,
		       gpointer user_data)
{
  SoupMessage *message;
  GVimeoVideoSearchData *search_data;
  gchar *request;

  g_return_if_fail (G_IS_VIMEO (vimeo));

  request = build_request (vimeo, text, page);
  search_data = g_slice_new (GVimeoVideoSearchData);
  search_data->vimeo = vimeo;
  search_data->search_cb = callback;
  search_data->user_data = user_data;

  message = soup_message_new ("GET", request);
  soup_session_queue_message (vimeo->priv->async_session,
			      message,
			      (SoupSessionCallback) search_videos_complete_cb,
			      search_data);
  g_free (request);
}

void
g_vimeo_video_get_play_url (GVimeo *vimeo,
			    gint id,
			    GVimeoURLCb callback,
			    gpointer user_data)
{
  GVimeoVideoURLData *data;
  gchar *url = g_strdup_printf ("%s%d",
				VIMEO_VIDEO_LOAD_URL,
				id);
  SoupMessage *message = soup_message_new ("GET", url);
  SoupMessageHeaders *headers = message->request_headers;
  soup_message_headers_append (headers, "User-Agent", PLUGIN_USER_AGENT);

  data = g_slice_new (GVimeoVideoURLData);
  data->video_id = id;
  data->vimeo = vimeo;
  data->callback = callback;
  data->user_data = user_data;

  soup_session_queue_message (vimeo->priv->async_session,
			      message,
			      (SoupSessionCallback) get_video_play_url_complete_cb,
			      data);
  g_free (url);
}
