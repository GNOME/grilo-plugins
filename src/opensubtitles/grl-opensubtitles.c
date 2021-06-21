/*
 * Copyright (C) 2014 Bastien Nocera <hadess@hadess.net>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>

#include <grilo.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <libsoup/soup.h>

#include "grl-opensubtitles.h"

#define GRL_LOG_DOMAIN_DEFAULT opensubtitles_log_domain
GRL_LOG_DOMAIN_STATIC(opensubtitles_log_domain);

#define SOURCE_ID   "grl-opensubtitles"
#define SOURCE_NAME _("OpenSubtitles Provider")
#define SOURCE_DESC _("A source providing a list of subtitles for a video")

#define GRL_OPENSUBTITLES_SOURCE_GET_PRIVATE(object)           \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                        \
                               GRL_OPENSUBTITLES_SOURCE_TYPE,	\
                               GrlOpenSubtitlesSourcePriv))

struct _GrlOpenSubtitlesSourcePriv {
  char *token;
  gboolean token_requested;

  GrlKeyID hash_keyid;
  SoupSession *session;
  GAsyncQueue *queue;
};

typedef struct {
  char *url;
  guint downloads;
  guint score;
} SubtitleData;

static GrlKeyID GRL_OPENSUBTITLES_METADATA_KEY_SUBTITLES_URL = GRL_METADATA_KEY_INVALID;
static GrlKeyID GRL_OPENSUBTITLES_METADATA_KEY_SUBTITLES_LANG = GRL_METADATA_KEY_INVALID;

/**/

static GrlOpenSubtitlesSource *grl_opensubtitles_source_new (void);
static void grl_opensubtitles_source_finalize (GObject *object);

static void grl_opensubtitles_source_resolve (GrlSource            *source,
                                              GrlSourceResolveSpec *rs);

static const GList *grl_opensubtitles_source_supported_keys (GrlSource *source);

static void grl_opensubtitles_source_cancel (GrlSource *source,
                                             guint      operation_id);

static gboolean grl_opensubtitles_source_may_resolve (GrlSource  *source,
                                                      GrlMedia   *media,
                                                      GrlKeyID    key_id,
                                                      GList     **missing_keys);

gboolean grl_opensubtitles_source_plugin_init (GrlRegistry *registry,
                                               GrlPlugin   *plugin,
                                               GList       *configs);

/* =================== GrlOpenSubtitles Plugin  =============== */

static GrlKeyID
register_metadata_key (GrlRegistry *registry,
                       GrlKeyID bind_key,
                       const char *name,
                       const char *nick,
                       const char *blurb)
{
  GParamSpec *spec;
  GrlKeyID key;

  spec = g_param_spec_string (name,
                              nick,
                              blurb,
                              NULL,
                              G_PARAM_READWRITE
                              | G_PARAM_STATIC_STRINGS);

  key = grl_registry_register_metadata_key (registry, spec, bind_key, NULL);

  if (key == GRL_METADATA_KEY_INVALID) {
    key = grl_registry_lookup_metadata_key (registry, name);
    if (grl_metadata_key_get_type (key) != G_TYPE_STRING) {
      key = GRL_METADATA_KEY_INVALID;
    }
  }

  return key;
}

gboolean
grl_opensubtitles_source_plugin_init (GrlRegistry *registry,
                                      GrlPlugin   *plugin,
                                      GList       *configs)
{
  GRL_LOG_DOMAIN_INIT (opensubtitles_log_domain, "opensubtitles");

  GRL_DEBUG ("grl_opensubtitles_source_plugin_init");

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  GRL_OPENSUBTITLES_METADATA_KEY_SUBTITLES_URL =
    register_metadata_key (registry,
                           GRL_METADATA_KEY_INVALID,
                           "subtitles-url",
                           "subtitles-url",
                           "Subtitles URL");

  GRL_OPENSUBTITLES_METADATA_KEY_SUBTITLES_LANG =
    register_metadata_key (registry,
                           GRL_METADATA_KEY_INVALID,
                           "subtitles-lang",
                           "subtitles-lang",
                           "Subtitles Language");

  GrlOpenSubtitlesSource *source = grl_opensubtitles_source_new ();
  grl_registry_register_source (registry,
                                plugin,
                                GRL_SOURCE (source),
                                NULL);
  return TRUE;
}

GRL_PLUGIN_DEFINE (GRL_MAJOR,
                   GRL_MINOR,
                   OPENSUBTITLES_PLUGIN_ID,
                   "OpenSubtitles Provider",
                   "A plugin that gets a list of subtitles for a video",
                   "Bastien Nocera",
                   VERSION,
                   "LGPL-2.1-or-later",
                   "http://www.hadess.net",
                   grl_opensubtitles_source_plugin_init,
                   NULL,
                   NULL);

/* ================== GrlOpenSubtitles GObject ================ */

G_DEFINE_TYPE_WITH_PRIVATE (GrlOpenSubtitlesSource, grl_opensubtitles_source, GRL_TYPE_SOURCE)

static GrlOpenSubtitlesSource *
grl_opensubtitles_source_new (void)
{
  GRL_DEBUG ("grl_opensubtitles_source_new");
  return g_object_new (GRL_OPENSUBTITLES_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       NULL);
}

static void
grl_opensubtitles_source_class_init (GrlOpenSubtitlesSourceClass * klass)
{
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  source_class->supported_keys = grl_opensubtitles_source_supported_keys;
  source_class->cancel = grl_opensubtitles_source_cancel;
  source_class->may_resolve = grl_opensubtitles_source_may_resolve;
  source_class->resolve = grl_opensubtitles_source_resolve;

  gobject_class->finalize = grl_opensubtitles_source_finalize;
}

static void
grl_opensubtitles_source_init (GrlOpenSubtitlesSource *source)
{
  GrlOpenSubtitlesSourcePrivate *priv = grl_opensubtitles_source_get_instance_private (source);

  source->priv = priv;
  priv->session = soup_session_new ();
  priv->queue = g_async_queue_new ();
}

static void
grl_opensubtitles_source_finalize (GObject *object)
{
  GrlOpenSubtitlesSource *source = GRL_OPENSUBTITLES_SOURCE (object);
  GrlOpenSubtitlesSourcePrivate *priv = source->priv;

  GRL_DEBUG ("%s", G_STRFUNC);

  g_clear_object (&priv->session);
  g_async_queue_unref (priv->queue);

  G_OBJECT_CLASS (grl_opensubtitles_source_parent_class)->finalize (object);
}


/* ======================= Utilities ==================== */

static void
ensure_hash_keyid (GrlOpenSubtitlesSourcePrivate *priv)
{
  if (priv->hash_keyid == GRL_METADATA_KEY_INVALID) {
    GrlRegistry *registry = grl_registry_get_default ();
    priv->hash_keyid = grl_registry_lookup_metadata_key (registry, "gibest-hash");
  }
}

static SoupMessage *
new_login_message (void)
{
  SoupMessage *msg;

  msg = soup_xmlrpc_request_new ("http://api.opensubtitles.org/xml-rpc",
				 "LogIn",
				 G_TYPE_STRING, "",
				 G_TYPE_STRING, "",
				 G_TYPE_STRING, "en",
				 G_TYPE_STRING, "Totem",
				 G_TYPE_INVALID);

  return msg;
}

static SoupMessage *
new_search_message (const char *token,
		    const char *hash,
		    gint64      size)
{
  SoupMessage *msg;
  GValueArray *array;
  GHashTable *fields;
  char *size_str;
  GValue sublanguageid = G_VALUE_INIT;
  GValue moviehash = G_VALUE_INIT;
  GValue moviebytesize = G_VALUE_INIT;
  GValue fieldsval = G_VALUE_INIT;

  size_str = g_strdup_printf ("%" G_GINT64_FORMAT, size);

  fields = g_hash_table_new (g_str_hash, g_str_equal);
  g_value_init (&sublanguageid, G_TYPE_STRING);
  g_value_set_string (&sublanguageid, "all");
  g_hash_table_insert (fields, "sublanguageid", &sublanguageid);
  g_value_init (&moviehash, G_TYPE_STRING);
  g_value_set_string (&moviehash, hash);
  g_hash_table_insert (fields, "moviehash", &moviehash);
  g_value_init (&moviebytesize, G_TYPE_STRING);
  g_value_set_string (&moviebytesize, size_str);
  g_hash_table_insert (fields, "moviebytesize", &moviebytesize);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

  array = g_value_array_new (1);
  g_value_init (&fieldsval, G_TYPE_HASH_TABLE);
  g_value_set_boxed (&fieldsval, fields);
  g_value_array_append (array, &fieldsval);

  msg = soup_xmlrpc_request_new ("http://api.opensubtitles.org/xml-rpc",
				 "SearchSubtitles",
				 G_TYPE_STRING, token,
				 G_TYPE_VALUE_ARRAY, array,
				 G_TYPE_INVALID);

  g_value_array_free (array);
  g_hash_table_unref (fields);

G_GNUC_END_IGNORE_DEPRECATIONS

  return msg;
}

static const char *
lookup_string (GHashTable *ht,
	       const char *key)
{
  GValue *val;
  val = g_hash_table_lookup (ht, key);
  if (!val)
    return NULL;
  return g_value_get_string (val);
}

static void
subtitle_data_free (SubtitleData *sub)
{
  g_free (sub->url);
  g_free (sub);
}

static int
lookup_int (GHashTable *ht,
	    const char *key)
{
  GValue *val;
  val = g_hash_table_lookup (ht, key);
  if (!val)
    return 0;
  return atoi (g_value_get_string (val));
}

static void
subs_foreach (gpointer key,
	      gpointer value,
	      gpointer user_data)
{
  const char *lang = key;
  SubtitleData *sub = value;
  GrlMedia *media = user_data;
  GrlRelatedKeys *relkeys;

  relkeys = grl_related_keys_new ();
  grl_related_keys_set_string (relkeys,
			       GRL_OPENSUBTITLES_METADATA_KEY_SUBTITLES_LANG,
			       lang);
  grl_related_keys_set_string (relkeys,
			       GRL_OPENSUBTITLES_METADATA_KEY_SUBTITLES_URL,
			       sub->url);
  grl_data_add_related_keys (GRL_DATA (media), relkeys);
}

static void
maybe_add_sub (GHashTable   *subs,
	       const char   *lang,
	       SubtitleData *sub)
{
  SubtitleData *old;

  old = g_hash_table_lookup (subs, lang);
  if (!old) {
    g_hash_table_insert (subs, g_strdup (lang), sub);
    return;
  }

  if (sub->score > old->score ||
      (sub->score == old->score && sub->downloads > old->downloads)) {
    g_hash_table_insert (subs, g_strdup (lang), sub);
    return;
  }

  subtitle_data_free (sub);
}

static char *
fixup_sub_url (const char *url)
{
  GString *str;

  if (!g_str_has_suffix (url, ".gz"))
    return g_strdup (url);

  str = g_string_new (NULL);
  g_string_insert_len (str, -1, url, strlen (url) - strlen (".gz"));
  g_string_insert (str, -1, ".srt");
  return g_string_free (str, FALSE);
}

static void
parse_results (GrlMedia    *media,
	       SoupMessage *msg)
{
  SoupBuffer *body;
  GError *error = NULL;
  GHashTable *response;
  GValue *data_val;
  GValueArray *data;
  guint i;
  GHashTable *subs;

  body = soup_message_body_flatten (msg->response_body);
  if (!soup_xmlrpc_extract_method_response (body->data, body->length, &error,
					    G_TYPE_HASH_TABLE, &response)) {
    GRL_WARNING ("Parsing search response failed: %s", error->message);
    g_error_free (error);
    soup_buffer_free (body);
    return;
  }

  data_val = g_hash_table_lookup (response, "data");
  if (!data_val)
    goto out;
  if (!G_VALUE_HOLDS_BOXED (data_val)) {
    GRL_DEBUG ("No matching subtitles in response");
    goto out;
  }
  data = g_value_get_boxed (data_val);
  subs = g_hash_table_new_full (g_str_hash, g_str_equal,
				g_free, (GDestroyNotify) subtitle_data_free);

  for (i = 0; i < data->n_values; i++) {
    GValue *val;
    GHashTable *ht;
    const char *lang;
    SubtitleData *sub;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

    val = g_value_array_get_nth (data, i);
    ht = g_value_get_boxed (val);

    /* Verify that the season/episode matches the media
     * before using that data */
    if (grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_SHOW)) {
      int episode, season;

      episode = lookup_int (ht, "SeriesSeason");
      season = lookup_int (ht, "SeriesEpisode");
      if (season != grl_media_get_episode (media) ||
          episode != grl_media_get_season (media)) {
        continue;
      }
    }

    lang = lookup_string (ht, "ISO639");

    sub = g_new0 (SubtitleData, 1);
    sub->url = fixup_sub_url (lookup_string (ht, "SubDownloadLink"));
    sub->downloads = lookup_int (ht, "SubDownloadsCnt");
    sub->score = 0;

    /* Scoring system from popcorn-opensubtitles */
    if (g_strcmp0 (lookup_string (ht, "MatchedBy"), "moviehash") == 0)
      sub->score += 100;
    if (g_strcmp0 (lookup_string (ht, "MatchedBy"), "tag") == 0)
      sub->score += 50;
    if (g_strcmp0 (lookup_string (ht, "UserRank"), "trusted") == 0)
      sub->score += 100;

    maybe_add_sub (subs, lang, sub);

G_GNUC_END_IGNORE_DEPRECATIONS
  }

  g_hash_table_foreach (subs, subs_foreach, media);
  g_hash_table_unref (subs);

out:
  g_hash_table_unref (response);
  soup_buffer_free (body);
}

static void
search_done_cb (SoupSession *session,
		SoupMessage *msg,
		gpointer     user_data)
{
  GrlSourceResolveSpec *rs = user_data;

  if (msg->status_code != 200) {
    GError *error = NULL;

    GRL_DEBUG ("Failed to login: HTTP code %d", msg->status_code);
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_RESOLVE_FAILED,
                         "Failed to login to OpenSubtitles.org (HTTP code %d)",
                         msg->status_code);
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);
    g_clear_error (&error);
  } else {
    parse_results (rs->media, msg);
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
  }
}

static char *
parse_token (SoupMessage *msg)
{
  SoupBuffer *body;
  char *token = NULL;
  GHashTable *ht;
  GError *error = NULL;

  body = soup_message_body_flatten (msg->response_body);
  if (!soup_xmlrpc_extract_method_response (body->data, body->length, &error,
                                            G_TYPE_HASH_TABLE, &ht)) {
    GRL_WARNING ("Parsing token response failed: %s", error->message);
    g_error_free (error);
    soup_buffer_free (body);
    return NULL;
  }
  token = g_value_dup_string (g_hash_table_lookup (ht, "token"));
  g_hash_table_unref (ht);
  soup_buffer_free (body);
  return token;
}

static void
login_done_cb (SoupSession *session,
	       SoupMessage *msg,
	       gpointer     user_data)
{
  gboolean failed = FALSE;
  GrlSourceResolveSpec *rs;
  GError *error = NULL;
  GrlOpenSubtitlesSourcePrivate *priv = GRL_OPENSUBTITLES_SOURCE(user_data)->priv;

  if (msg->status_code != 200) {
    GRL_DEBUG ("Failed to login: HTTP code %d", msg->status_code);
    failed = TRUE;
  } else {
    priv->token = parse_token (msg);
    if (!priv->token) {
      failed = TRUE;
      msg->status_code = 666;
    }
  }

  if (failed) {
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_RESOLVE_FAILED,
                         "Failed to fetch subtitles from OpenSubtitles.org (HTTP code %d)",
                         msg->status_code);
  }

  while ((rs = g_async_queue_try_pop (priv->queue))) {
    if (failed) {
      rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, error);
    } else {
       SoupMessage *search;
       search = new_search_message (priv->token,
                                    grl_data_get_string (GRL_DATA (rs->media), priv->hash_keyid),
                                    grl_media_get_size (rs->media));
       grl_operation_set_data (rs->operation_id, search);
       soup_session_queue_message (session, search, search_done_cb, rs);
    }
  }

  g_clear_error (&error);
}

/* ================== API Implementation ================ */

static const GList *
grl_opensubtitles_source_supported_keys (GrlSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_OPENSUBTITLES_METADATA_KEY_SUBTITLES_URL,
                                      GRL_OPENSUBTITLES_METADATA_KEY_SUBTITLES_LANG,
                                      NULL);
  }
  return keys;
}

static gboolean
grl_opensubtitles_source_may_resolve (GrlSource *source,
                                      GrlMedia  *media,
                                      GrlKeyID   key_id,
                                      GList    **missing_keys)
{
  GrlOpenSubtitlesSourcePrivate *priv = GRL_OPENSUBTITLES_SOURCE(source)->priv;

  ensure_hash_keyid (priv);
  if (priv->hash_keyid == GRL_METADATA_KEY_INVALID)
    return FALSE;

  if (!media ||
      !grl_data_has_key (GRL_DATA (media), priv->hash_keyid) ||
      !grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_SIZE)) {
    if (missing_keys) {
      GList *keys = NULL;
      if (!media || !grl_data_has_key (GRL_DATA (media), priv->hash_keyid))
        keys = g_list_prepend (keys, GRLKEYID_TO_POINTER (priv->hash_keyid));
      if (!media || !grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_SIZE))
        keys = g_list_prepend (keys, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_SIZE));
      *missing_keys = keys;
    }
    return FALSE;
  }

  if (!grl_media_is_video (media))
    return FALSE;

  if (grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_SHOW)) {
    gboolean has_episode, has_season;

    has_episode = grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_EPISODE);
    has_season = grl_data_has_key (GRL_DATA (media), GRL_METADATA_KEY_SEASON);

    if (!has_episode || !has_season) {
      if (missing_keys) {
        GList *keys = NULL;
        if (!has_episode)
          keys = g_list_prepend (keys, GRLKEYID_TO_POINTER(GRL_METADATA_KEY_EPISODE));
        if (!has_season)
          keys = g_list_prepend (keys, GRLKEYID_TO_POINTER(GRL_METADATA_KEY_SEASON));
        *missing_keys = keys;
      }
      return FALSE;
    }
  }

  return TRUE;
}

static void
grl_opensubtitles_source_resolve (GrlSource            *source,
                                  GrlSourceResolveSpec *rs)
{
  GrlOpenSubtitlesSourcePrivate *priv = GRL_OPENSUBTITLES_SOURCE(source)->priv;
  SoupMessage *msg;

  GRL_DEBUG (__FUNCTION__);

  ensure_hash_keyid (priv);

  if (!priv->token) {
    if (!priv->token_requested) {
      msg = new_login_message ();
      grl_operation_set_data (rs->operation_id, msg);
      soup_session_queue_message (priv->session, msg, login_done_cb, source);
    }
    g_async_queue_push (priv->queue, rs);
    return;
  }

  msg = new_search_message (priv->token,
                            grl_data_get_string (GRL_DATA (rs->media), priv->hash_keyid),
                            grl_media_get_size (rs->media));
  grl_operation_set_data (rs->operation_id, msg);
  soup_session_queue_message (priv->session, msg, search_done_cb, rs);
}

static void
grl_opensubtitles_source_cancel (GrlSource *source,
                                 guint operation_id)
{
  GrlOpenSubtitlesSourcePrivate *priv = GRL_OPENSUBTITLES_SOURCE (source)->priv;
  SoupMessage *msg = grl_operation_get_data (operation_id);

  if (msg)
    soup_session_cancel_message (priv->session, msg, SOUP_STATUS_CANCELLED);
}
