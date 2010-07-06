#include "grl-flickr-auth.h"
#include "gflickr.h"

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
