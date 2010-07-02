#include "grl-flickr-auth.h"

#include <gio/gio.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>

#define FLICKR_ENTRYPOINT "http://api.flickr.com/services/rest/?"

gchar *
grl_flickr_get_frob (const gchar *api_key,
                     const gchar *secret)
{
  gchar *to_sign;
  gchar *api_sig;
  gchar *url;
  GVfs *vfs;
  GFile *uri;
  gchar *contents;
  xmlDocPtr xmldoc = NULL;
  xmlXPathContextPtr xpath_ctx = NULL;
  xmlXPathObjectPtr xpath_res = NULL;
  GError *error = NULL;
  gchar *frob = NULL;

  /* Get api sig */
  to_sign = g_strdup_printf ("%s"
                             "api_key%s"
                             "method" "flickr.auth.getFrob",
                             secret,
                             api_key);
  api_sig = g_compute_checksum_for_string (G_CHECKSUM_MD5, to_sign, -1);
  g_free (to_sign);

  /* Build url */
  url = g_strdup_printf (FLICKR_ENTRYPOINT
                         "method=flickr.auth.getFrob&"
                         "api_key=%s&"
                         "api_sig=%s",
                         api_key,
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
  xmldoc = xmlReadMemory (contents, xmlStrlen ((xmlChar *)contents), NULL, NULL,
                          XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  g_free (contents);

  if (xmldoc) {
    xpath_ctx = xmlXPathNewContext (xmldoc);
    if (xpath_ctx) {
      xpath_res = xmlXPathEvalExpression ((xmlChar *) "/rsp/frob", xpath_ctx);
      if (xpath_res && xpath_res->nodesetval->nodeTab) {
        frob = (gchar *) xmlNodeListGetString (xmldoc,
                                               xpath_res->nodesetval->nodeTab[0]->xmlChildrenNode,
                                               1);
      } else {
        g_warning ("Flick's frob not found");
      }
    } else {
      g_warning ("Unable to create Flickr's XPath");
    }
  } else {
    g_warning ("Unable to parse Flickr XML reply");
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

  return frob;
}
