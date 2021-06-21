/*
 * Copyright (C) 2010, 2011 Igalia S.L.
 * Copyright (C) 2012 Canonical Ltd.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Juan A. Suarez Romero <jasuarez@igalia.com>
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

#include "grl-gravatar.h"

#include <glib/gi18n-lib.h>

/* ---------- Logging ---------- */

#define GRL_LOG_DOMAIN_DEFAULT gravatar_log_domain
GRL_LOG_DOMAIN_STATIC(gravatar_log_domain);

/* -------- Gravatar API -------- */

#define GRAVATAR_URL "https://www.gravatar.com/avatar/%s.jpg"

/* ------- Pluging Info -------- */

#define SOURCE_ID   GRAVATAR_PLUGIN_ID
#define SOURCE_NAME _("Avatar provider from Gravatar")
#define SOURCE_DESC _("A plugin to get avatars for artist and author fields")

static GrlGravatarSource *grl_gravatar_source_new (void);

static void grl_gravatar_source_resolve (GrlSource *source,
                                         GrlSourceResolveSpec *rs);

static const GList *grl_gravatar_source_supported_keys (GrlSource *source);

static gboolean grl_gravatar_source_may_resolve (GrlSource *source,
                                                 GrlMedia *media,
                                                 GrlKeyID key_id,
                                                 GList **missing_keys);

static GrlKeyID register_gravatar_key (GrlRegistry *registry,
                                       GrlKeyID bind_key,
                                       const gchar *name,
                                       const gchar *nick,
                                       const gchar *blurb);

gboolean grl_gravatar_source_plugin_init (GrlRegistry *registry,
                                          GrlPlugin *plugin,
                                          GList *configs);

GrlKeyID GRL_METADATA_KEY_ARTIST_AVATAR = 0;
GrlKeyID GRL_METADATA_KEY_AUTHOR_AVATAR = 0;

/* =================== Gravatar Plugin  =============== */

gboolean
grl_gravatar_source_plugin_init (GrlRegistry *registry,
                                 GrlPlugin *plugin,
                                 GList *configs)
{
  GRL_LOG_DOMAIN_INIT (gravatar_log_domain, "gravatar");

  GRL_DEBUG ("grl_gravatar_source_plugin_init");

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  if (!GRL_METADATA_KEY_ARTIST_AVATAR &&
      !GRL_METADATA_KEY_AUTHOR_AVATAR) {
    GRL_WARNING ("Unable to register \"author-avatar\" nor \"artist-avatar\"");
    return FALSE;
  }

  GrlGravatarSource *source = grl_gravatar_source_new ();
  grl_registry_register_source (registry,
                                plugin,
                                GRL_SOURCE (source),
                                NULL);
  return TRUE;
}

static void
grl_gravatar_source_plugin_register_keys (GrlRegistry *registry,
                                          GrlPlugin   *plugin)
{
  GRL_METADATA_KEY_ARTIST_AVATAR =
    register_gravatar_key (registry,
                           GRL_METADATA_KEY_ARTIST,
                           "artist-avatar",
                           "ArtistAvatar",
                           "Avatar for the artist");

  GRL_METADATA_KEY_AUTHOR_AVATAR =
    register_gravatar_key (registry,
                           GRL_METADATA_KEY_AUTHOR,
                           "author-avatar",
                            "AuthorAvatar",
                            "Avatar for the author");
}

GRL_PLUGIN_DEFINE (GRL_MAJOR,
                   GRL_MINOR,
                   GRAVATAR_PLUGIN_ID,
                   "Avatar provider from Gravatar",
                   "A plugin to get avatars for artist and author fields",
                   "Igalia S.L.",
                   VERSION,
                   "LGPL-2.1-or-later",
                   "http://www.igalia.com",
                   grl_gravatar_source_plugin_init,
                   NULL,
                   grl_gravatar_source_plugin_register_keys);

/* ================== Gravatar GObject ================ */

static GrlGravatarSource *
grl_gravatar_source_new (void)
{
  GRL_DEBUG ("grl_gravatar_source_new");
  return g_object_new (GRL_GRAVATAR_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       NULL);
}

static void
grl_gravatar_source_class_init (GrlGravatarSourceClass * klass)
{
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

  source_class->supported_keys = grl_gravatar_source_supported_keys;
  source_class->may_resolve = grl_gravatar_source_may_resolve;
  source_class->resolve = grl_gravatar_source_resolve;
}

static void
grl_gravatar_source_init (GrlGravatarSource *source)
{
}

G_DEFINE_TYPE (GrlGravatarSource,
               grl_gravatar_source,
               GRL_TYPE_SOURCE);

/* ======================= Utilities ==================== */

static GrlKeyID
register_gravatar_key (GrlRegistry *registry,
                       GrlKeyID bind_key,
                       const gchar *name,
                       const gchar *nick,
                       const gchar *blurb)
{
  GParamSpec *spec;
  GrlKeyID key;

  spec = g_param_spec_string (name,
                              nick,
                              blurb,
                              NULL,
                              G_PARAM_READWRITE);

  key = grl_registry_register_metadata_key (registry, spec, bind_key, NULL);

  /* If key was not registered, could be that it is already registered. If so,
     check if type is the expected one, and reuse it */
  if (key == GRL_METADATA_KEY_INVALID) {
    key = grl_registry_lookup_metadata_key (registry, name);
    if (grl_metadata_key_get_type (key) != G_TYPE_STRING) {
      key = GRL_METADATA_KEY_INVALID;
    }
  }

  return key;
}

static gchar *
get_avatar (const gchar *field) {
  GMatchInfo *match_info = NULL;
  gchar *avatar = NULL;
  gchar *email;
  gchar *email_hash;
  gchar *lowercased_field;
  static GRegex *email_regex = NULL;

  if (!field) {
    return NULL;
  }

  lowercased_field = g_utf8_strdown (field, -1);

  if (!email_regex) {
    email_regex = g_regex_new ("[\\w-]+@([\\w-]+\\.)+[\\w-]+", G_REGEX_OPTIMIZE, 0, NULL);
  }

  if (g_regex_match (email_regex, lowercased_field, 0, &match_info)) {
    email = g_match_info_fetch (match_info, 0);
    email_hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, email, -1);
    avatar = g_strdup_printf (GRAVATAR_URL, email_hash);
    g_free (email);
    g_free (email_hash);
  }
  g_match_info_free (match_info);

  return avatar;
}

/**
 * Returns: TRUE if @dependency is in @media, FALSE else.
 * When returning FALSE, if @missing_keys is not NULL it is populated with a
 * list containing @dependency as only element.
 */
static gboolean
has_dependency (GrlMedia *media, GrlKeyID dependency, GList **missing_keys)
{
  if (media && grl_data_has_key (GRL_DATA (media), dependency))
    return TRUE;

  if (missing_keys)
    *missing_keys = grl_metadata_key_list_new (dependency,
                                               NULL);
  return FALSE;
}

static void
set_avatar (GrlData *data,
            GrlKeyID key)
{
  gint length, i;
  GrlRelatedKeys *relkeys;
  gchar *avatar_url;

  length = grl_data_length (data, key);

  for (i = 0; i < length; i++) {
    relkeys = grl_data_get_related_keys (data, key, i);
    avatar_url = get_avatar (grl_related_keys_get_string (relkeys, key));
    if (avatar_url) {
      grl_related_keys_set_string (relkeys, key, avatar_url);
      g_free (avatar_url);
    }
  }
}

/* ================== API Implementation ================ */

static const GList *
grl_gravatar_source_supported_keys (GrlSource *source)
{
  static GList *keys = NULL;

  if (!keys) {
    if (GRL_METADATA_KEY_ARTIST_AVATAR) {
      keys =
          g_list_prepend (keys,
                          GRLKEYID_TO_POINTER (GRL_METADATA_KEY_ARTIST_AVATAR));
    }
    if (GRL_METADATA_KEY_AUTHOR_AVATAR) {
      keys =
          g_list_prepend (keys,
                          GRLKEYID_TO_POINTER (GRL_METADATA_KEY_AUTHOR_AVATAR));
    }
  }

 return keys;
}

static gboolean
grl_gravatar_source_may_resolve (GrlSource *source,
                                 GrlMedia *media,
                                 GrlKeyID key_id,
                                 GList **missing_keys)
{
  /* FIXME: we should check whether the artist/author in @media is in an email
   * format */

  if (key_id == GRL_METADATA_KEY_ARTIST_AVATAR)
    return has_dependency (media, GRL_METADATA_KEY_ARTIST, missing_keys);
  else if (key_id == GRL_METADATA_KEY_AUTHOR_AVATAR)
    return has_dependency (media, GRL_METADATA_KEY_AUTHOR, missing_keys);

  return FALSE;
}

static void
grl_gravatar_source_resolve (GrlSource *source,
                             GrlSourceResolveSpec *rs)
{
  gboolean artist_avatar_required = FALSE;
  gboolean author_avatar_required = FALSE;

  GRL_DEBUG (__FUNCTION__);

  GList *iter;

  /* Check that albumart is requested */
  iter = rs->keys;
  while (iter && (!artist_avatar_required || !author_avatar_required)) {
    GrlKeyID key = GRLPOINTER_TO_KEYID (iter->data);
    if (key == GRL_METADATA_KEY_ARTIST_AVATAR) {
      artist_avatar_required = TRUE;
    } else if (key == GRL_METADATA_KEY_AUTHOR_AVATAR) {
      author_avatar_required = TRUE;
    }
    iter = g_list_next (iter);
  }

  if (artist_avatar_required) {
    set_avatar (GRL_DATA (rs->media), GRL_METADATA_KEY_ARTIST);
  }

  if (author_avatar_required) {
    set_avatar (GRL_DATA (rs->media), GRL_METADATA_KEY_AUTHOR);
  }

  rs->callback (source, rs->operation_id, rs->media, rs->user_data, NULL);
}
