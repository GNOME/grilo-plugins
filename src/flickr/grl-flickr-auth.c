#include "grl-flickr-auth.h"
#include "gflickr.h"

static void
check_token_cb (GFlickr *f,
                GHashTable *result,
                gpointer user_data)
{
  gint *token_is_valid = (gint *) user_data;

  *token_is_valid = (result != NULL);
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

gboolean
grl_flickr_check_token (const gchar *api_key,
                        const gchar *secret,
                        const gchar *token)
{
  GFlickr *f;
  GMainContext *mainloop_ctx;
  GMainLoop *mainloop;
  gint token_is_valid = -1;

  f = g_flickr_new (api_key, secret, NULL);
  if (!f) {
    return FALSE;
  }

  g_flickr_auth_checkToken (f, token, check_token_cb, &token_is_valid);

  mainloop = g_main_loop_new (NULL, TRUE);
  mainloop_ctx = g_main_loop_get_context (mainloop);

  while (token_is_valid == -1) {
    g_main_context_iteration (mainloop_ctx, TRUE);
  }

  g_main_loop_unref (mainloop);
  g_object_unref (f);

  return token_is_valid;
}
