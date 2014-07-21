/*
 * Copyright (C) 2010, 2011 Igalia S.L.
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

#include <grilo.h>
#include <glib/gi18n-lib.h>
#include <net/grl-net.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <string.h>
#include <errno.h>

#include "grl-jamendo.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT jamendo_log_domain
GRL_LOG_DOMAIN_STATIC(jamendo_log_domain);

#define GRL_TRACE(x) GRL_DEBUG(__PRETTY_FUNCTION__)

#define JAMENDO_ID_SEP    "/"
#define JAMENDO_ROOT_NAME "Jamendo"

#define MAX_ELEMENTS 100

/* ------- Categories ------- */

#define JAMENDO_ARTIST "artist"
#define JAMENDO_ALBUM  "album"
#define JAMENDO_TRACK  "track"

/* ---- Jamendo Web API  ---- */

#define JAMENDO_BASE_ENTRY "http://api.jamendo.com/get2"
#define JAMENDO_FORMAT     "xml"
#define JAMENDO_RANGE      "n=%u&pn=%u"

#define JAMENDO_ARTIST_ENTRY JAMENDO_BASE_ENTRY "/%s/" JAMENDO_ARTIST "/" JAMENDO_FORMAT

#define JAMENDO_ALBUM_ENTRY  JAMENDO_BASE_ENTRY "/%s/" JAMENDO_ALBUM  "/" JAMENDO_FORMAT \
  "/" JAMENDO_ALBUM "_" JAMENDO_ARTIST

#define JAMENDO_TRACK_ENTRY  JAMENDO_BASE_ENTRY "/%s/" JAMENDO_TRACK  "/" JAMENDO_FORMAT \
  "/" JAMENDO_ALBUM "_" JAMENDO_ARTIST "+" JAMENDO_TRACK "_" JAMENDO_ALBUM

#define JAMENDO_GET_ARTISTS JAMENDO_ARTIST_ENTRY "/?" JAMENDO_RANGE
#define JAMENDO_GET_ALBUMS  JAMENDO_ALBUM_ENTRY  "/?" JAMENDO_RANGE
#define JAMENDO_GET_TRACKS  JAMENDO_TRACK_ENTRY  "/?" JAMENDO_RANGE

#define JAMENDO_GET_ALBUMS_FROM_ARTIST JAMENDO_ALBUM_ENTRY "/?" JAMENDO_RANGE "&artist_id=%s"
#define JAMENDO_GET_TRACKS_FROM_ALBUM JAMENDO_TRACK_ENTRY  "/?" JAMENDO_RANGE "&album_id=%s"
#define JAMENDO_GET_ARTIST JAMENDO_ARTIST_ENTRY "/?id=%s"

#define JAMENDO_GET_ALBUM  JAMENDO_ALBUM_ENTRY  "/?id=%s"
#define JAMENDO_GET_TRACK  JAMENDO_TRACK_ENTRY  "/?id=%s"

#define JAMENDO_SEARCH_ARTIST JAMENDO_ARTIST_ENTRY "/?" JAMENDO_RANGE "&searchquery=%s"
#define JAMENDO_SEARCH_ALBUM  JAMENDO_ALBUM_ENTRY  "/?" JAMENDO_RANGE "&searchquery=%s"
#define JAMENDO_SEARCH_TRACK  JAMENDO_TRACK_ENTRY  "/?" JAMENDO_RANGE "&searchquery=%s"
#define JAMENDO_SEARCH_ALL    JAMENDO_TRACK_ENTRY  "/?" JAMENDO_RANGE

/* --- Plugin information --- */

#define PLUGIN_ID   JAMENDO_PLUGIN_ID

#define SOURCE_ID   "grl-jamendo"
#define SOURCE_NAME "Jamendo"
#define SOURCE_DESC _("A source for browsing and searching Jamendo music")

enum {
  RESOLVE,
  BROWSE,
  QUERY,
  SEARCH
};

typedef enum {
  JAMENDO_ARTIST_CAT = 1,
  JAMENDO_ALBUM_CAT,
  JAMENDO_FEEDS_CAT,
  JAMENDO_TRACK_CAT,
} JamendoCategory;

typedef struct {
  JamendoCategory category;
  gchar *id;
  gchar *artist_name;
  gchar *artist_genre;
  gchar *artist_url;
  gchar *artist_image;
  gchar *album_name;
  gchar *album_genre;
  gchar *album_url;
  gchar *album_duration;
  gchar *album_image;
  gchar *track_name;
  gchar *track_url;
  gchar *track_stream;
  gchar *track_duration;
  gchar *feed_name;
} Entry;

typedef struct {
  gint type;
  union {
    GrlSourceBrowseSpec *bs;
    GrlSourceQuerySpec *qs;
    GrlSourceResolveSpec *rs;
    GrlSourceSearchSpec *ss;
  } spec;
  xmlNodePtr node;
  xmlDocPtr doc;
  guint total_results;
  guint index;
  guint offset;
  gboolean cancelled;
} XmlParseEntries;

struct Feeds {
  gchar *name;
  JamendoCategory cat;
  gchar *url;
} feeds[] = {
  { N_("Albums of the week"), JAMENDO_ALBUM_CAT,
    JAMENDO_GET_ALBUMS "&order=ratingweek_desc", },
  { N_("Tracks of the week"), JAMENDO_TRACK_CAT,
    JAMENDO_GET_TRACKS "&order=ratingweek_desc", },
  { N_("New releases"), JAMENDO_TRACK_CAT,
    JAMENDO_GET_TRACKS "&order=releasedate_desc", },
  { N_("Top artists"), JAMENDO_ARTIST_CAT,
    JAMENDO_GET_ARTISTS "&order=rating_desc", },
  { N_("Top albums"), JAMENDO_ALBUM_CAT,
    JAMENDO_GET_ALBUMS "&order=rating_desc", },
  { N_("Top tracks"), JAMENDO_TRACK_CAT,
    JAMENDO_GET_TRACKS "&order=rating_desc", },
};

struct _GrlJamendoSourcePriv {
  GrlNetWc *wc;
  GCancellable *cancellable;
};

#define GRL_JAMENDO_SOURCE_GET_PRIVATE(object)		\
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                \
                               GRL_JAMENDO_SOURCE_TYPE,	\
                               GrlJamendoSourcePriv))

static GrlJamendoSource *grl_jamendo_source_new (void);

gboolean grl_jamendo_plugin_init (GrlRegistry *registry,
                                  GrlPlugin *plugin,
                                  GList *configs);

static const GList *grl_jamendo_source_supported_keys (GrlSource *source);

static void grl_jamendo_source_resolve (GrlSource *source,
                                        GrlSourceResolveSpec *rs);

static void grl_jamendo_source_browse (GrlSource *source,
                                       GrlSourceBrowseSpec *bs);

static void grl_jamendo_source_query (GrlSource *source,
                                      GrlSourceQuerySpec *qs);

static void grl_jamendo_source_search (GrlSource *source,
                                       GrlSourceSearchSpec *ss);

static void grl_jamendo_source_cancel (GrlSource *source,
                                       guint operation_id);

/* =================== Jamendo Plugin  =============== */

gboolean
grl_jamendo_plugin_init (GrlRegistry *registry,
                         GrlPlugin *plugin,
                         GList *configs)
{
  GRL_LOG_DOMAIN_INIT (jamendo_log_domain, "jamendo");

  GRL_TRACE ();

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  GrlJamendoSource *source = grl_jamendo_source_new ();
  grl_registry_register_source (registry,
                                plugin,
                                GRL_SOURCE (source),
                                NULL);
  return TRUE;
}

GRL_PLUGIN_REGISTER (grl_jamendo_plugin_init,
                     NULL,
                     PLUGIN_ID);

/* ================== Jamendo GObject ================ */

static GrlJamendoSource *
grl_jamendo_source_new (void)
{
  GRL_TRACE();
  return g_object_new (GRL_JAMENDO_SOURCE_TYPE,
		       "source-id", SOURCE_ID,
		       "source-name", SOURCE_NAME,
		       "source-desc", SOURCE_DESC,
		       "supported-media", GRL_MEDIA_TYPE_AUDIO,
		       NULL);
}

G_DEFINE_TYPE (GrlJamendoSource, grl_jamendo_source, GRL_TYPE_SOURCE);

static void
grl_jamendo_source_finalize (GObject *object)
{
  GrlJamendoSource *self;

  self = GRL_JAMENDO_SOURCE (object);

  g_clear_object (&self->priv->wc);
  g_clear_object (&self->priv->cancellable);

  G_OBJECT_CLASS (grl_jamendo_source_parent_class)->finalize (object);
}

static void
grl_jamendo_source_class_init (GrlJamendoSourceClass * klass)
{
  GObjectClass *g_class = G_OBJECT_CLASS (klass);
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

  g_class->finalize = grl_jamendo_source_finalize;

  source_class->cancel = grl_jamendo_source_cancel;
  source_class->supported_keys = grl_jamendo_source_supported_keys;
  source_class->resolve = grl_jamendo_source_resolve;
  source_class->browse = grl_jamendo_source_browse;
  source_class->query = grl_jamendo_source_query;
  source_class->search = grl_jamendo_source_search;

  g_type_class_add_private (klass, sizeof (GrlJamendoSourcePriv));
}

static void
grl_jamendo_source_init (GrlJamendoSource *source)
{
  source->priv = GRL_JAMENDO_SOURCE_GET_PRIVATE (source);

  /* If we try to get too much elements in a single step, Jamendo might return
     nothing. So limit the maximum amount of elements in each query */
  grl_source_set_auto_split_threshold (GRL_SOURCE (source), MAX_ELEMENTS);
}

/* ======================= Utilities ==================== */

static void
free_entry (Entry *entry)
{
  g_free (entry->id);
  g_free (entry->artist_name);
  g_free (entry->artist_genre);
  g_free (entry->artist_url);
  g_free (entry->artist_image);
  g_free (entry->album_name);
  g_free (entry->album_genre);
  g_free (entry->album_url);
  g_free (entry->album_duration);
  g_free (entry->album_image);
  g_free (entry->track_name);
  g_free (entry->track_url);
  g_free (entry->track_stream);
  g_free (entry->track_duration);
  g_free (entry->feed_name);
  g_slice_free (Entry, entry);
}

static gint
xml_count_children (xmlNodePtr node)
{
#if (LIBXML2_VERSION >= 20700)
  return xmlChildElementCount (node);
#else
  gint nchildren = 0;
  xmlNodePtr i = node->xmlChildrenNode;

  while (i) {
    nchildren++;
    i = i->next;
  }

  return nchildren;
#endif
}

static void
xml_parse_result (const gchar *str, GError **error, XmlParseEntries *xpe)
{
  xmlDocPtr doc;
  xmlNodePtr node;
  gint child_nodes = 0;

  doc = xmlReadMemory (str, strlen (str), NULL, NULL,
                       XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
  if (!doc) {
    *error = g_error_new_literal (GRL_CORE_ERROR,
                                  GRL_CORE_ERROR_BROWSE_FAILED,
                                  _("Failed to parse response"));
    goto free_resources;
  }

  node = xmlDocGetRootElement (doc);
  if (!node) {
    *error = g_error_new_literal (GRL_CORE_ERROR,
                                  GRL_CORE_ERROR_BROWSE_FAILED,
                                  _("Empty response"));
    goto free_resources;
  }

  if (xmlStrcmp (node->name, (const xmlChar *) "data")) {
    *error = g_error_new_literal (GRL_CORE_ERROR,
                                  GRL_CORE_ERROR_BROWSE_FAILED,
                                  _("Empty response"));
    goto free_resources;
  }

  child_nodes = xml_count_children (node);
  node = node->xmlChildrenNode;

  /* Skip offset */
  while (node && xpe->offset > 0) {
    node = node->next;
    child_nodes--;
    xpe->offset--;
  }

  xpe->node = node;
  xpe->doc = doc;
  xpe->total_results = child_nodes;

  return;

 free_resources:
  xmlFreeDoc (doc);
}

static Entry *
xml_parse_entry (xmlDocPtr doc, xmlNodePtr entry)
{
  xmlNodePtr node;
  Entry *data = g_slice_new0 (Entry);

  if (strcmp ((gchar *) entry->name, JAMENDO_ARTIST) == 0) {
    data->category = JAMENDO_ARTIST_CAT;
  } else if (strcmp ((gchar *) entry->name, JAMENDO_ALBUM) == 0) {
    data->category = JAMENDO_ALBUM_CAT;
  } else if (strcmp ((gchar *) entry->name, JAMENDO_TRACK) == 0) {
    data->category = JAMENDO_TRACK_CAT;
  } else {
    g_return_val_if_reached (NULL);
  }

  node = entry->xmlChildrenNode;

  while (node) {
    if (!xmlStrcmp (node->name, (const xmlChar *) "id")) {
      data->id =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "artist_name")) {
      data->artist_name =
	(gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "album_name")) {
      data->album_name =
	(gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "artist_genre")) {
      data->artist_genre =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "artist_url")) {
      data->artist_url =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "artist_image")) {
      data->artist_image =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "album_genre")) {
      data->album_genre =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "album_url")) {
      data->album_url =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "album_duration")) {
      data->album_duration =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "album_image")) {
      data->album_image =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "track_name")) {
      data->track_name =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "track_url")) {
      data->track_url =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "track_stream")) {
      data->track_stream =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);

    } else if (!xmlStrcmp (node->name, (const xmlChar *) "track_duration")) {
      data->track_duration =
        (gchar *) xmlNodeListGetString (doc, node->xmlChildrenNode, 1);
    }

    node = node->next;
  }

  return data;
}

static void
update_media_from_entry (GrlMedia *media, const Entry *entry)
{
  gchar *id;

  if (entry->id) {
    id = g_strdup_printf ("%d/%s", entry->category, entry->id);
  } else {
    id = g_strdup_printf ("%d", entry->category);
  }

  /* Common fields */
  grl_media_set_id (media, id);
  g_free (id);

  if (entry->artist_name) {
    grl_data_set_string (GRL_DATA (media),
                         GRL_METADATA_KEY_ARTIST,
                         entry->artist_name);
  }

  if (entry->album_name) {
    grl_data_set_string (GRL_DATA (media),
                         GRL_METADATA_KEY_ALBUM,
                         entry->album_name);
  }

  /* Fields for artist */
  if (entry->category == JAMENDO_ARTIST_CAT) {
    if (entry->artist_name) {
      grl_media_set_title (media, entry->artist_name);
    }

    if (entry->artist_genre) {
      grl_data_set_string (GRL_DATA (media),
                           GRL_METADATA_KEY_GENRE,
                           entry->artist_genre);
    }

    if (entry->artist_url) {
      grl_media_set_site (media, entry->artist_url);
    }

    if (entry->artist_image) {
      grl_media_set_thumbnail (media, entry->artist_image);
    }

    /* Fields for album */
  } else if (entry->category == JAMENDO_ALBUM_CAT) {
    if (entry->album_name) {
      grl_media_set_title (media, entry->album_name);
    }

    if (entry->album_genre) {
      grl_data_set_string (GRL_DATA (media),
                           GRL_METADATA_KEY_GENRE,
                           entry->album_genre);
    }

    if (entry->album_url) {
      grl_media_set_site (media, entry->album_url);
    }

    if (entry->album_image) {
      grl_media_set_thumbnail (media, entry->album_image);
    }

    if (entry->album_duration) {
      grl_media_set_duration (media, atoi (entry->album_duration));
    }

    /* Fields for track */
  } else if (entry->category == JAMENDO_TRACK_CAT) {
    if (entry->track_name) {
      grl_media_set_title (media, entry->track_name);
    }

    if (entry->album_genre) {
      grl_media_audio_set_genre (GRL_MEDIA_AUDIO (media),
                                 entry->album_genre);
    }

    if (entry->track_url) {
      grl_media_set_site (media, entry->track_url);
    }

    if (entry->album_image) {
      grl_media_set_thumbnail (media, entry->album_image);
    }

    if (entry->track_stream) {
      grl_media_set_url (media, entry->track_stream);
    }

    if (entry->track_duration) {
      grl_media_set_duration (media, atoi (entry->track_duration));
    }
  } else if (entry->category == JAMENDO_FEEDS_CAT) {
    if (entry->feed_name) {
      grl_media_set_title (media, entry->feed_name);
    }
  }
}

static gboolean
xml_parse_entries_idle (gpointer user_data)
{
  XmlParseEntries *xpe = (XmlParseEntries *) user_data;
  gboolean parse_more;
  GrlMedia *media = NULL;
  Entry *entry;
  gint remaining = 0;

  GRL_TRACE ();

  parse_more = (xpe->cancelled == FALSE && xpe->node);

  if (parse_more) {
    entry = xml_parse_entry (xpe->doc, xpe->node);
    if (entry->category == JAMENDO_TRACK_CAT) {
      media = grl_media_audio_new ();
    } else {
      media = grl_media_box_new ();
    }

    update_media_from_entry (media, entry);
    free_entry (entry);

    xpe->index++;
    xpe->node = xpe->node->next;
    remaining = xpe->total_results - xpe->index;
  }

  if (parse_more || xpe->cancelled) {
    switch (xpe->type) {
    case BROWSE:
      xpe->spec.bs->callback (xpe->spec.bs->source,
                              xpe->spec.bs->operation_id,
                              media,
                              remaining,
                              xpe->spec.bs->user_data,
                              NULL);
      break;
    case QUERY:
      xpe->spec.qs->callback (xpe->spec.qs->source,
                              xpe->spec.qs->operation_id,
                              media,
                              remaining,
                              xpe->spec.qs->user_data,
                              NULL);
      break;
    case SEARCH:
      xpe->spec.ss->callback (xpe->spec.ss->source,
                              xpe->spec.ss->operation_id,
                              media,
                              remaining,
                              xpe->spec.ss->user_data,
                              NULL);
      break;
    }
  }

  if (!parse_more) {
    xmlFreeDoc (xpe->doc);
    g_slice_free (XmlParseEntries, xpe);
  }

  return parse_more;
}

static void
read_done_cb (GObject *source_object,
              GAsyncResult *res,
              gpointer user_data)
{
  XmlParseEntries *xpe = (XmlParseEntries *) user_data;
  gint error_code = -1;
  GError *wc_error = NULL;
  GError *error = NULL;
  gchar *content = NULL;
  Entry *entry = NULL;

  /* Check if operation was cancelled */
  if (xpe->cancelled) {
    goto invoke_cb;
  }

  if (!grl_net_wc_request_finish (GRL_NET_WC (source_object),
                              res,
                              &content,
                              NULL,
                              &wc_error)) {
    switch (xpe->type) {
    case RESOLVE:
      error_code = GRL_CORE_ERROR_RESOLVE_FAILED;
      break;
    case BROWSE:
      error_code = GRL_CORE_ERROR_BROWSE_FAILED;
      break;
    case QUERY:
      error_code = GRL_CORE_ERROR_QUERY_FAILED;
      break;
    case SEARCH:
      error_code = GRL_CORE_ERROR_SEARCH_FAILED;
      break;
    }

    error = g_error_new (GRL_CORE_ERROR,
                         error_code,
                         _("Failed to connect: %s"),
                         wc_error->message);
    g_error_free (wc_error);
    goto invoke_cb;
  }

  if (content) {
    xml_parse_result (content, &error, xpe);
  } else {
    goto invoke_cb;
  }

  if (error) {
    goto invoke_cb;
  }

  if (xpe->node) {
    if (xpe->type == RESOLVE) {
      entry = xml_parse_entry (xpe->doc, xpe->node);
      xmlFreeDoc (xpe->doc);
      update_media_from_entry (xpe->spec.rs->media, entry);
      free_entry (entry);
      goto invoke_cb;
    } else {
      guint id = g_idle_add (xml_parse_entries_idle, xpe);
      g_source_set_name_by_id (id, "[jamendo] xml_parse_entries_idle");
    }
  } else {
    if (xpe->type == RESOLVE) {
      error = g_error_new_literal (GRL_CORE_ERROR,
                                   GRL_CORE_ERROR_RESOLVE_FAILED,
                                   _("Failed to parse response"));
    }
    goto invoke_cb;
  }

  return;

 invoke_cb:
  switch (xpe->type) {
  case RESOLVE:
    xpe->spec.rs->callback (xpe->spec.rs->source,
                            xpe->spec.rs->operation_id,
                            xpe->spec.rs->media,
                            xpe->spec.rs->user_data,
                            error);
    break;
  case BROWSE:
    xpe->spec.bs->callback (xpe->spec.bs->source,
                            xpe->spec.bs->operation_id,
                            NULL,
                            0,
                            xpe->spec.bs->user_data,
                            error);
    break;
  case QUERY:
    xpe->spec.qs->callback (xpe->spec.qs->source,
                            xpe->spec.qs->operation_id,
                            NULL,
                            0,
                            xpe->spec.qs->user_data,
                            error);
    break;
  case SEARCH:
    xpe->spec.ss->callback (xpe->spec.ss->source,
                            xpe->spec.ss->operation_id,
                            NULL,
                            0,
                            xpe->spec.ss->user_data,
                            error);
    break;
  }

  g_slice_free (XmlParseEntries, xpe);
  g_clear_error (&error);
}

static void
read_url_async (GrlJamendoSource *source,
                const gchar *url,
                gpointer user_data)
{
  if (!source->priv->wc)
    source->priv->wc = g_object_new (GRL_TYPE_NET_WC, "throttling", 1, NULL);

  source->priv->cancellable = g_cancellable_new ();

  GRL_DEBUG ("Opening '%s'", url);
  grl_net_wc_request_async (source->priv->wc,
                        url,
                        source->priv->cancellable,
                        read_done_cb,
                        user_data);
}

static void
update_media_from_root (GrlMedia *media)
{
  grl_media_set_title (media, JAMENDO_ROOT_NAME);
  grl_media_box_set_childcount (GRL_MEDIA_BOX (media), 3);
}

static void
update_media_from_artists (GrlMedia *media)
{
  Entry entry = {
    .category = JAMENDO_ARTIST_CAT,
    .artist_name = _("Artists"),
  };

  update_media_from_entry (media, &entry);
}

static void
update_media_from_albums (GrlMedia *media)
{
  Entry entry = {
    .category = JAMENDO_ALBUM_CAT,
    .album_name = _("Albums"),
  };

  update_media_from_entry (media, &entry);
}

static void
update_media_from_feeds (GrlMedia *media)
{
  Entry entry = {
    .category = JAMENDO_FEEDS_CAT,
    .feed_name = _("Feeds"),
  };

  update_media_from_entry (media, &entry);
  grl_media_box_set_childcount (GRL_MEDIA_BOX (media), G_N_ELEMENTS(feeds));
}

static void
send_toplevel_categories (GrlSourceBrowseSpec *bs)
{
  GrlMedia *media;
  gint remaining;
  guint skip = grl_operation_options_get_skip (bs->options);
  gint count = grl_operation_options_get_count (bs->options);

  /* Check if all elements must be skipped */
  if (skip > 2 || count == 0) {
    bs->callback (bs->source, bs->operation_id, NULL, 0, bs->user_data, NULL);
    return;
  }

  count = MIN (count, 3);
  remaining = MIN (count, 3 - skip);

  while (remaining > 0) {
    media = grl_media_box_new ();
    switch (skip) {
    case 0:
      update_media_from_artists (media);
      break;
    case 1:
      update_media_from_albums (media);
      break;
    default:
      update_media_from_feeds (media);
    }
    remaining--;
    skip++;
    bs->callback (bs->source, bs->operation_id, media, remaining, bs->user_data, NULL);
  }
}

static void
update_media_from_feed (GrlMedia *media, int i)
{
  char *id;

  id = g_strdup_printf("%d" JAMENDO_ID_SEP "%d", JAMENDO_FEEDS_CAT, i);
  grl_media_set_id (media, id);
  g_free (id);

  grl_media_set_title (media, g_dgettext (GETTEXT_PACKAGE, feeds[i].name));

}

static void
send_feeds (GrlSourceBrowseSpec *bs)
{
  gint count = grl_operation_options_get_count (bs->options);
  guint skip = grl_operation_options_get_skip (bs->options);

  if( skip >= G_N_ELEMENTS (feeds) )
  {
    //Signal "no results"
    bs->callback (bs->source, bs->operation_id,
                  NULL, 0, bs->user_data, NULL);
  } else {
    int remaining = MIN (count, G_N_ELEMENTS (feeds));
    int i;

    for (i = skip; remaining > 0 && i < G_N_ELEMENTS (feeds); i++) {
      GrlMedia *media;

      media = grl_media_box_new ();
      update_media_from_feed (media, i);
      remaining--;
      bs->callback (bs->source,
                    bs->operation_id,
                    media,
                    remaining,
                    bs->user_data,
                    NULL);
    }
  }
}

static gchar *
get_jamendo_keys (JamendoCategory category)
{
  gchar *jamendo_keys = NULL;
  gchar *keys_for_artist = "artist_name+artist_genre+artist_image+artist_url";
  gchar *keys_for_album  = "album_name+album_genre+album_image+album_url+album_duration";
  gchar *keys_for_track  = "track_name+track_stream+track_url+track_duration";

  if (category == JAMENDO_ARTIST_CAT) {
    jamendo_keys = g_strconcat ("id+", keys_for_artist, NULL);
  } else if (category == JAMENDO_ALBUM_CAT) {
    jamendo_keys = g_strconcat ("id+", keys_for_artist,
                                "+", keys_for_album,
                                NULL);
  } else if (category == JAMENDO_TRACK_CAT) {
    jamendo_keys = g_strconcat ("id+", keys_for_artist,
                                "+", keys_for_album,
                                "+", keys_for_track,
                                NULL);
  }

  return jamendo_keys;
}

static gboolean
parse_query (const gchar *query, JamendoCategory *category, gchar **term)
{
  if (!query) {
    return FALSE;
  }

  if (g_str_has_prefix (query, JAMENDO_ARTIST "=")) {
    *category = JAMENDO_ARTIST_CAT;
    query += 7;
  } else if (g_str_has_prefix (query, JAMENDO_ALBUM "=")) {
    *category = JAMENDO_ALBUM_CAT;
    query += 6;
  } else if (g_str_has_prefix (query, JAMENDO_TRACK "=")) {
    *category = JAMENDO_TRACK_CAT;
    query += 6;
  } else {
    return FALSE;
  }

  *term = g_uri_escape_string (query, NULL, TRUE);
  return TRUE;
}

/* ================== API Implementation ================ */

static const GList *
grl_jamendo_source_supported_keys (GrlSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_ARTIST,
                                      GRL_METADATA_KEY_ALBUM,
                                      GRL_METADATA_KEY_GENRE,
                                      GRL_METADATA_KEY_URL,
                                      GRL_METADATA_KEY_DURATION,
                                      GRL_METADATA_KEY_THUMBNAIL,
                                      GRL_METADATA_KEY_SITE,
                                      NULL);
  }
  return keys;
}

static void
grl_jamendo_source_resolve (GrlSource *source,
                            GrlSourceResolveSpec *rs)
{
  gchar *url = NULL;
  gchar *jamendo_keys = NULL;
  const gchar *id;
  gchar **id_split = NULL;
  XmlParseEntries *xpe = NULL;
  GError *error = NULL;
  JamendoCategory category;

  GRL_TRACE ();

  if (!rs->media ||
      !grl_data_has_key (GRL_DATA (rs->media),
                         GRL_METADATA_KEY_ID)) {
    /* Get info from root */
    if (!rs->media) {
      rs->media = grl_media_box_new ();
    }
    update_media_from_root (rs->media);
  } else {
    id = grl_media_get_id (rs->media);
    id_split = g_strsplit (id, JAMENDO_ID_SEP, 0);

    if (g_strv_length (id_split) == 0) {
      error = g_error_new (GRL_CORE_ERROR,
                           GRL_CORE_ERROR_RESOLVE_FAILED,
                           _("Invalid identifier %s"),
                           id);
      goto send_error;
    }

    category = atoi (id_split[0]);

    if (category == JAMENDO_ARTIST_CAT) {
      if (id_split[1]) {
        /* Requesting information from a specific artist */
        jamendo_keys = get_jamendo_keys (JAMENDO_ARTIST_CAT);
        url =
          g_strdup_printf (JAMENDO_GET_ARTIST,
                           jamendo_keys,
                           id_split[1]);
        g_free (jamendo_keys);
      } else {
        /* Requesting information from artist category */
        update_media_from_artists (rs->media);
      }
    } else if (category == JAMENDO_ALBUM_CAT) {
      if (id_split[1]) {
        /* Requesting information from a specific album */
        jamendo_keys = get_jamendo_keys (JAMENDO_ALBUM_CAT);
        url =
          g_strdup_printf (JAMENDO_GET_ALBUM,
                           jamendo_keys,
                           id_split[1]);
        g_free (jamendo_keys);
      } else {
        /* Requesting information from album category */
        update_media_from_albums (rs->media);
      }
    } else if (category == JAMENDO_TRACK_CAT) {
      if (id_split[1]) {
        /* Requesting information from a specific song */
        jamendo_keys = get_jamendo_keys (JAMENDO_TRACK_CAT);
        url =
          g_strdup_printf (JAMENDO_GET_TRACK,
                           jamendo_keys,
                           id_split[1]);
        g_free (jamendo_keys);
      } else {
        error = g_error_new (GRL_CORE_ERROR,
                             GRL_CORE_ERROR_RESOLVE_FAILED,
                             _("Invalid identifier %s"),
                             id);
        g_strfreev (id_split);
        goto send_error;
      }
    } else if (category == JAMENDO_FEEDS_CAT) {
      if (id_split[1]) {
        int i;

        errno = 0;
        i = strtol (id_split[1], NULL, 0);
        if (errno != 0 || (i <= 0 && i > G_N_ELEMENTS (feeds))) {
          error = g_error_new (GRL_CORE_ERROR,
                               GRL_CORE_ERROR_RESOLVE_FAILED,
                               _("Invalid category identifier %s"),
                               id_split[1]);
          g_strfreev (id_split);
          goto send_error;
        }

        update_media_from_feed (rs->media, i);
      } else {
        update_media_from_feeds (rs->media);
      }
    } else {
      error = g_error_new (GRL_CORE_ERROR,
                           GRL_CORE_ERROR_RESOLVE_FAILED,
                           _("Invalid identifier %s"),
                           id);
      g_strfreev (id_split);
      goto send_error;
    }
  }

  g_clear_pointer (&id_split, g_strfreev);

  if (url) {
    xpe = g_slice_new0 (XmlParseEntries);
    xpe->type = RESOLVE;
    xpe->spec.rs = rs;
    read_url_async (GRL_JAMENDO_SOURCE (source), url, xpe);
    g_free (url);
  } else {
    if (rs->media) {
      rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
    }
  }

  return;

 send_error:
  rs->callback (rs->source, rs->operation_id, NULL, rs->user_data, error);
  g_error_free (error);
}

static void
grl_jamendo_source_browse (GrlSource *source,
                           GrlSourceBrowseSpec *bs)
{
  gchar *url = NULL;
  gchar *jamendo_keys;
  gchar **container_split = NULL;
  JamendoCategory category;
  XmlParseEntries *xpe = NULL;
  const gchar *container_id;
  GError *error = NULL;
  guint page_size;
  guint page_number;
  guint page_offset = 0;
  gint count = grl_operation_options_get_count (bs->options);
  guint skip = grl_operation_options_get_skip (bs->options);

  GRL_TRACE ();

  container_id = grl_media_get_id (bs->container);

  if (!container_id) {
    /* Root category: return top-level predefined categories */
    send_toplevel_categories (bs);
    return;
  }

  container_split = g_strsplit (container_id, JAMENDO_ID_SEP, 0);

  if (g_strv_length (container_split) == 0) {
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_BROWSE_FAILED,
                         _("Invalid container identifier %s"),
                         container_id);
  } else {
    category = atoi (container_split[0]);
    grl_paging_translate (skip,
                          count,
                          0,
                          &page_size,
                          &page_number,
                          &page_offset);

    if (category == JAMENDO_ARTIST_CAT) {
      if (container_split[1]) {
        jamendo_keys = get_jamendo_keys (JAMENDO_ALBUM_CAT);
        /* Requesting information from a specific artist */
        url =
          g_strdup_printf (JAMENDO_GET_ALBUMS_FROM_ARTIST,
                           jamendo_keys,
                           page_size,
                           page_number,
                           container_split[1]);
      } else {
        /* Browsing through artists */
        jamendo_keys = get_jamendo_keys (JAMENDO_ARTIST_CAT);
        url = g_strdup_printf (JAMENDO_GET_ARTISTS,
                               jamendo_keys,
                               page_size,
                               page_number);
      }
      g_free (jamendo_keys);

    } else if (category == JAMENDO_ALBUM_CAT) {
      if (container_split[1]) {
        /* Requesting information from a specific album */
        jamendo_keys = get_jamendo_keys (JAMENDO_TRACK_CAT);
        url =
          g_strdup_printf (JAMENDO_GET_TRACKS_FROM_ALBUM,
                           jamendo_keys,
                           page_size,
                           page_number,
                           container_split[1]);
      } else {
        /* Browsing through albums */
        jamendo_keys = get_jamendo_keys (JAMENDO_ALBUM_CAT);
        url = g_strdup_printf (JAMENDO_GET_ALBUMS,
                               jamendo_keys,
                               page_size,
                               page_number);
      }
      g_free (jamendo_keys);

    } else if (category == JAMENDO_FEEDS_CAT) {
      if (container_split[1]) {
        int feed_id;

        feed_id = atoi (container_split[1]);
        jamendo_keys = get_jamendo_keys (feeds[feed_id].cat);
        url = g_strdup_printf (feeds[feed_id].url,
                               jamendo_keys,
                               page_size,
                               page_number);
        g_free (jamendo_keys);
      } else {
        send_feeds (bs);
        return;
      }

    } else if (category == JAMENDO_TRACK_CAT) {
      error = g_error_new (GRL_CORE_ERROR,
                           GRL_CORE_ERROR_BROWSE_FAILED,
                           _("Failed to browse: %s is a track"),
                           container_id);
    } else {
      error = g_error_new (GRL_CORE_ERROR,
                           GRL_CORE_ERROR_BROWSE_FAILED,
                           _("Invalid container identifier %s"),
                           container_id);
    }
  }

  if (error) {
    bs->callback (source, bs->operation_id, NULL, 0, bs->user_data, error);
    g_error_free (error);
    return;
  }

  xpe = g_slice_new0 (XmlParseEntries);
  xpe->type = BROWSE;
  xpe->offset = page_offset;
  xpe->spec.bs = bs;

  grl_operation_set_data (bs->operation_id, xpe);

  read_url_async (GRL_JAMENDO_SOURCE (source), url, xpe);
  g_free (url);
  g_clear_pointer (&container_split, g_strfreev);
}

/*
 * Query format is "<type>=<text>", where <type> can be either 'artist', 'album'
 * or 'track' and 'text' is the term to search.
 *
 * The result will be also a <type>.
 *
 * Example: search for artists that have the "Shake" in their name or
 * description: "artist=Shake"
 *
 */
static void
grl_jamendo_source_query (GrlSource *source,
                          GrlSourceQuerySpec *qs)
{
  GError *error = NULL;
  JamendoCategory category;
  gchar *term = NULL;
  gchar *url;
  gchar *jamendo_keys = NULL;
  gchar *query = NULL;
  XmlParseEntries *xpe = NULL;
  guint page_size;
  guint page_number;
  guint page_offset;
  gint count = grl_operation_options_get_count (qs->options);
  guint skip = grl_operation_options_get_skip (qs->options);

  GRL_TRACE ();

  if (!parse_query (qs->query, &category, &term)) {
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_QUERY_FAILED,
                         _("Malformed query \"%s\""),
                         qs->query);
    goto send_error;
  }

  jamendo_keys = get_jamendo_keys (category);
  switch (category) {
  case JAMENDO_ARTIST_CAT:
    query = JAMENDO_SEARCH_ARTIST;
    break;
  case JAMENDO_ALBUM_CAT:
    query = JAMENDO_SEARCH_ALBUM;
    break;
  case JAMENDO_TRACK_CAT:
    query = JAMENDO_SEARCH_TRACK;
    break;
  default:
    g_return_if_reached ();
  }

  grl_paging_translate (skip,
                        count,
                        0,
                        &page_size,
                        &page_number,
                        &page_offset);

  url = g_strdup_printf (query,
                         jamendo_keys,
                         page_size,
                         page_number,
                         term);
  g_free (term);
  g_free (jamendo_keys);

  xpe = g_slice_new0 (XmlParseEntries);
  xpe->type = QUERY;
  xpe->offset = page_offset;
  xpe->spec.qs = qs;

  grl_operation_set_data (qs->operation_id, xpe);

  read_url_async (GRL_JAMENDO_SOURCE (source), url, xpe);
  g_free (url);

  return;

 send_error:
  qs->callback (qs->source, qs->operation_id, NULL, 0, qs->user_data, error);
  g_error_free (error);
}


static void
grl_jamendo_source_search (GrlSource *source,
                           GrlSourceSearchSpec *ss)
{
  XmlParseEntries *xpe;
  gchar *jamendo_keys;
  gchar *url;
  guint page_size;
  guint page_number;
  guint page_offset;
  gint count = grl_operation_options_get_count (ss->options);
  guint skip = grl_operation_options_get_skip (ss->options);

  GRL_TRACE ();

  jamendo_keys = get_jamendo_keys (JAMENDO_TRACK_CAT);

  grl_paging_translate (skip,
                        count,
                        0,
                        &page_size,
                        &page_number,
                        &page_offset);

  if (ss->text) {
    url = g_strdup_printf (JAMENDO_SEARCH_TRACK,
                           jamendo_keys,
                           page_size,
                           page_number,
                           ss->text);
  } else {
    url = g_strdup_printf (JAMENDO_SEARCH_ALL,
                           jamendo_keys,
                           page_size,
                           page_number);
  }

  xpe = g_slice_new0 (XmlParseEntries);
  xpe->type = SEARCH;
  xpe->offset = page_offset;
  xpe->spec.ss = ss;

  grl_operation_set_data (ss->operation_id, xpe);

  read_url_async (GRL_JAMENDO_SOURCE (source), url, xpe);
  g_free (jamendo_keys);
  g_free (url);
}

static void
grl_jamendo_source_cancel (GrlSource *source, guint operation_id)
{
  XmlParseEntries *xpe;
  GrlJamendoSourcePriv *priv;

  g_return_if_fail (GRL_IS_JAMENDO_SOURCE (source));

  priv = GRL_JAMENDO_SOURCE_GET_PRIVATE (source);

  if (priv->cancellable && G_IS_CANCELLABLE (priv->cancellable))
    g_cancellable_cancel (priv->cancellable);
  priv->cancellable = NULL;

  if (priv->wc)
    grl_net_wc_flush_delayed_requests (priv->wc);

  GRL_DEBUG ("grl_jamendo_source_cancel");

  xpe = (XmlParseEntries *) grl_operation_get_data (operation_id);

  if (xpe) {
    xpe->cancelled = TRUE;
  }
}
