#include "grl-flickr-auth.h"
#include "gflickr.h"

#include <gio/gio.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <string.h>

#define FLICKR_ENTRYPOINT "http://api.flickr.com/services/rest/?"
#define FLICKR_AUTH       "http://flickr.com/services/auth/?"

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

gchar *
grl_flickr_get_frob (const gchar *api_key,
                     const gchar *secret)
{
  GFlickr *f;
  gchar *frob;

  f = g_flickr_new (api_key, secret, NULL);
  if (!f) {
    return NULL;
  }

  frob = g_flickr_auth_getFrob (f);
  g_object_unref (f);

  return frob;
}

gchar *
grl_flickr_get_login_link (const gchar *api_key,
                           const gchar *secret,
                           const gchar *frob,
                           const gchar *perm)
{
  GFlickr *f;
  gchar *url;

  f = g_flickr_new (api_key, secret, NULL);
  if (!f) {
    return NULL;
  }

  url = g_flickr_auth_loginLink (f, frob, perm);
  g_object_unref (f);

  return url;
}

gchar *
grl_flickr_get_token (const gchar *api_key,
                      const gchar *secret,
                      const gchar *frob)
{
  GFlickr *f;
  gchar *token;

  f = g_flickr_new (api_key, secret, NULL);
  if (!f) {
    return NULL;
  }

  token = g_flickr_auth_getToken (f, frob);
  g_object_unref (f);

  return token;
}
