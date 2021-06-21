/*
 * SPDX-FileCopyrightText: 2021 Marek Chalupa <mchalupa@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "flickr-oauth.h"

#include <glib.h>
#include <stdlib.h>

#include <string.h>
#include <oauth.h>

/* ----------------- private functions declarations ----------------- */

static gchar *
get_timestamp (void);

static void
free_params (gchar **params, gint params_no);

/* -------------------------- public API  -------------------------- */

gchar *
flickroauth_get_signature (const gchar *consumer_secret,
                           const gchar *token_secret,
                           const gchar *url,
                           gchar **params,
                           gint params_no)
{
  gchar *params_string;
  gchar *base_string;
  gchar *encryption_key;
  gchar *signature;

  qsort (params, params_no, sizeof (gchar *), oauth_cmpstringp);

  params_string = oauth_serialize_url (params_no, 0, params);

  base_string = oauth_catenc (3, FLICKR_OAUTH_HTTP_METHOD,
                                url, params_string);

  g_free (params_string);

  if (token_secret == NULL)
    encryption_key = g_strdup_printf ("%s&", consumer_secret);
  else
    encryption_key = g_strdup_printf ("%s&%s",
                                      consumer_secret,
                                      token_secret);

  signature = oauth_sign_hmac_sha1 (base_string, encryption_key);

  g_free (encryption_key);
  g_free (base_string);

  return signature;
}


gchar *
flickroauth_create_api_url (const gchar *consumer_key,
                            const gchar *consumer_secret,
                            const gchar *oauth_token,
                            const gchar *oauth_token_secret,
                            gchar **params,
                            const guint params_no)
{
  guint i;
  gchar *nonce;
  gchar *timestamp;
  gchar *signature;
  gchar *url;
  gchar *params_string;

  g_return_val_if_fail (consumer_key, NULL);

  /* handle Non-authorised call */
  if (oauth_token == NULL)
  {
    params_string = oauth_serialize_url (params_no, 0, params);

    url = g_strdup_printf ("%s?api_key=%s&%s", FLICKR_API_URL,
                                               consumer_key,
                                               params_string);

    g_free (params_string);

    return url;
  }

  /* there are 7 pre-filled parameters  in authorize call*/
  guint params_all_no = params_no + 7;
  gchar **params_all = g_malloc ((params_all_no) * sizeof (gchar *));

  if (params_all == NULL)
    return NULL;

  nonce = oauth_gen_nonce ();
  timestamp = get_timestamp ();

  params_all[0] = g_strdup_printf ("oauth_nonce=%s", nonce);
  params_all[1] = g_strdup_printf ("oauth_timestamp=%s", timestamp);
  params_all[2] = g_strdup_printf ("oauth_consumer_key=%s",
                                   consumer_key);
  params_all[3] = g_strdup_printf ("oauth_signature_method=%s",
                                   FLICKR_OAUTH_SIGNATURE_METHOD);
  params_all[4] = g_strdup_printf ("oauth_version=%s",
                                   FLICKR_OAUTH_VERSION);
  params_all[5] = g_strdup_printf ("oauth_token=%s", oauth_token);

  /* copy user parameters to the params_all */
  for (i = 0; i < params_no; i++)
    params_all[7 + i - 1] = g_strdup (params[i]);

  g_free (nonce);
  g_free (timestamp);

  signature = flickroauth_get_signature (consumer_secret,
                                         oauth_token_secret,
                                         FLICKR_API_URL, params_all,
                                         params_all_no - 1);

  params_all[params_all_no - 1] = g_strdup_printf ("oauth_signature=%s",
                                                   signature);
  g_free (signature);

  params_string = oauth_serialize_url (params_all_no, 0, params_all);

  free_params (params_all, params_all_no);
  g_free (params_all);

  url = g_strdup_printf ("%s?%s", FLICKR_API_URL, params_string);

  return url;
}

inline gchar *
flickroauth_authorization_url (const gchar *oauth_token,
                               const gchar *perms)
{
  gchar *url;
  if (perms == NULL)
    url = g_strdup_printf ("%s?oauth_token=%s", FLICKR_OAUTH_AUTHPOINT,
                                              oauth_token);
  else
    url = g_strdup_printf ("%s?oauth_token=%s&perms=%s",
                           FLICKR_OAUTH_AUTHPOINT,
                           oauth_token, perms);

  return url;
}

/* ----------------------- private functions ----------------------- */

inline static gchar *
get_timestamp (void)
{
  GTimeVal tm;
  g_get_current_time (&tm);

  return g_strdup_printf ("%lu", tm.tv_sec);
}

static void
free_params (gchar **params, gint params_no)
{
  gint i;
  for (i = 0; i < params_no; i++)
    g_free (params[i]);
}
