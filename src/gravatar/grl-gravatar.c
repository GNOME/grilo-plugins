/*
 * Copyright (C) 2010 Igalia S.L.
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

/* ---------- Logging ---------- */

#define GRL_LOG_DOMAIN_DEFAULT gravatar_log_domain
GRL_LOG_DOMAIN_STATIC(gravatar_log_domain);

/* -------- Gravatar API -------- */

#define GRAVATAR_URL "http://www.gravatar.com/avatar/%s.jpg"

/* ------- Pluging Info -------- */

#define PLUGIN_ID   GRAVATAR_PLUGIN_ID

#define SOURCE_ID   PLUGIN_ID
#define SOURCE_NAME "Avatar provider from Gravatar"
#define SOURCE_DESC "A plugin to get avatars for artist and author fields"

#define AUTHOR      "Igalia S.L."
#define LICENSE     "LGPL"
#define SITE        "http://www.igalia.com"


static GrlGravatarSource *grl_gravatar_source_new (void);

static void grl_gravatar_source_resolve (GrlMetadataSource *source,
                                         GrlMetadataSourceResolveSpec *rs);

static const GList *grl_gravatar_source_supported_keys (GrlMetadataSource *source);

static const GList *grl_gravatar_source_key_depends (GrlMetadataSource *source,
                                                     GrlKeyID key_id);

static GrlKeyID register_gravatar_key (GrlPluginRegistry *registry,
                                       const gchar *name,
                                       const gchar *nick,
                                       const gchar *blurb);

gboolean grl_gravatar_source_plugin_init (GrlPluginRegistry *registry,
                                          const GrlPluginInfo *plugin,
                                          GList *configs);

GrlKeyID GRL_METADATA_KEY_ARTIST_AVATAR = NULL;
GrlKeyID GRL_METADATA_KEY_AUTHOR_AVATAR = NULL;

/* =================== Gravatar Plugin  =============== */

gboolean
grl_gravatar_source_plugin_init (GrlPluginRegistry *registry,
                                 const GrlPluginInfo *plugin,
                                 GList *configs)
{
  GRL_LOG_DOMAIN_INIT (gravatar_log_domain, "gravatar");

  GRL_DEBUG ("grl_gravatar_source_plugin_init");

  /* Register keys */
  GRL_METADATA_KEY_ARTIST_AVATAR =
    register_gravatar_key (registry,
                           "artist-avatar",
                           "ArtistAvatar",
                           "Avatar for the artist");

  GRL_METADATA_KEY_AUTHOR_AVATAR =
    register_gravatar_key (registry,
                           "author-avatar",
                            "AuthorAvatar",
                            "Avatar for the author");
  if (!GRL_METADATA_KEY_ARTIST_AVATAR &&
      !GRL_METADATA_KEY_AUTHOR_AVATAR) {
    GRL_WARNING ("Unable to register \"autor-avatar\" nor \"artist-avatar\"");
    return FALSE;
  }

  GrlGravatarSource *source = grl_gravatar_source_new ();
  grl_plugin_registry_register_source (registry,
                                       plugin,
                                       GRL_MEDIA_PLUGIN (source),
                                       NULL);
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_gravatar_source_plugin_init,
                     NULL,
                     PLUGIN_ID);

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
  GrlMetadataSourceClass *metadata_class = GRL_METADATA_SOURCE_CLASS (klass);
  metadata_class->supported_keys = grl_gravatar_source_supported_keys;
  metadata_class->key_depends = grl_gravatar_source_key_depends;
  metadata_class->resolve = grl_gravatar_source_resolve;
}

static void
grl_gravatar_source_init (GrlGravatarSource *source)
{
}

G_DEFINE_TYPE (GrlGravatarSource,
               grl_gravatar_source,
               GRL_TYPE_METADATA_SOURCE);

/* ======================= Utilities ==================== */

static GrlKeyID
register_gravatar_key (GrlPluginRegistry *registry,
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

  key = grl_plugin_registry_register_metadata_key (registry, spec, NULL);

  /* If key was not registered, could be that it is already registered. If so,
     check if type is the expected one, and reuse it */
  if (!key) {
    g_param_spec_unref (spec);
    key = grl_plugin_registry_lookup_metadata_key (registry, name);
    if (!key || GRL_METADATA_KEY_GET_TYPE (key) != G_TYPE_STRING) {
      key = NULL;
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
    g_match_info_free (match_info);
    email_hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, email, -1);
    avatar = g_strdup_printf (GRAVATAR_URL, email_hash);
    g_free (email);
    g_free (email_hash);
  }

  return avatar;
}

/* ================== API Implementation ================ */

static const GList *
grl_gravatar_source_supported_keys (GrlMetadataSource *source)
{
  static GList *keys = NULL;

  if (!keys) {
    if (GRL_METADATA_KEY_ARTIST_AVATAR) {
      keys = g_list_prepend (keys, GRL_METADATA_KEY_ARTIST_AVATAR);
    }
    if (GRL_METADATA_KEY_AUTHOR_AVATAR) {
      keys =g_list_prepend (keys, GRL_METADATA_KEY_AUTHOR_AVATAR);
    }
  }

 return keys;
}

static const GList *
grl_gravatar_source_key_depends (GrlMetadataSource *source,
                                 GrlKeyID key_id)
{
  static GList *artist_avatar_deps = NULL;
  static GList *author_avatar_deps = NULL;

  if (!artist_avatar_deps) {
    artist_avatar_deps = grl_metadata_key_list_new (GRL_METADATA_KEY_ARTIST,
                                                    NULL);
  }

  if (!author_avatar_deps) {
    author_avatar_deps = grl_metadata_key_list_new (GRL_METADATA_KEY_AUTHOR,
                                                    NULL);
  }

  if (key_id == GRL_METADATA_KEY_ARTIST_AVATAR) {
    return artist_avatar_deps;
  } else if (key_id == GRL_METADATA_KEY_AUTHOR_AVATAR) {
    return author_avatar_deps;
  } else {
    return  NULL;
  }
}

static void
grl_gravatar_source_resolve (GrlMetadataSource *source,
                             GrlMetadataSourceResolveSpec *rs)
{
  gboolean artist_avatar_required = FALSE;
  gboolean author_avatar_required = FALSE;
  gchar *avatar_url;

  GRL_DEBUG ("grl_gravatar_source_resolve");

  GList *iter;

  /* Check that albumart is requested */
  iter = rs->keys;
  while (iter && (!artist_avatar_required || !author_avatar_required)) {
    if (iter->data == GRL_METADATA_KEY_ARTIST_AVATAR) {
      artist_avatar_required = TRUE;
    } else if (iter->data == GRL_METADATA_KEY_AUTHOR_AVATAR) {
      author_avatar_required = TRUE;
    }
    iter = g_list_next (iter);
  }

  if (artist_avatar_required) {
    avatar_url = get_avatar (grl_data_get_string (GRL_DATA (rs->media),
                                                  GRL_METADATA_KEY_ARTIST));
    if (avatar_url) {
      grl_data_set_string (GRL_DATA (rs->media),
                           GRL_METADATA_KEY_ARTIST_AVATAR,
                           avatar_url);
      g_free (avatar_url);
    }
  }

  if (author_avatar_required) {
    avatar_url = get_avatar (grl_data_get_string (GRL_DATA (rs->media),
                                                  GRL_METADATA_KEY_AUTHOR));
    if (avatar_url) {
      grl_data_set_string (GRL_DATA (rs->media),
                           GRL_METADATA_KEY_AUTHOR_AVATAR,
                           avatar_url);
      g_free (avatar_url);
    }
  }

  rs->callback (source, rs->media, rs->user_data, NULL);
}
