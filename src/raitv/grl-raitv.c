/*
 *
 * Author: Marco Piazza <mpiazza@gmail.com>
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

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/HTMLparser.h>
#include <string.h>
#include <net/grl-net.h>
#include <glib/gi18n-lib.h>

#include "grl-raitv.h"

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT raitv_log_domain
GRL_LOG_DOMAIN_STATIC(raitv_log_domain);

/* ----- Root categories ---- */

#define RAITV_ROOT_NAME       "Rai.tv"

#define ROOT_DIR_POPULARS_INDEX	0
#define ROOT_DIR_RECENTS_INDEX 	1

#define RAITV_POPULARS_ID	 "most-popular"
#define RAITV_POPULARS_NAME N_("Most Popular")

#define RAITV_RECENTS_ID	"recent"
#define RAITV_RECENTS_NAME	N_("Recent")

#define RAITV_POPULARS_THEME_ID	"theme-popular"
#define RAITV_RECENTS_THEME_ID	"theme-recent"

#define RAITV_VIDEO_SEARCH                      \
  "http://www.ricerca.rai.it/search?"           \
  "q=%s"                                        \
  "&num=50"                                     \
  "&start=%s"                                   \
  "&getfields=*"                                \
  "&site=raitv"                                 \
  "&filter=0"


#define RAITV_VIDEO_POPULAR                        \
  "http://www.rai.it//StatisticheProxy/proxy.jsp?" \
  "action=mostVisited"                             \
  "&domain=RaiTv"                                  \
  "&days=7"                                        \
  "&state=1"                                       \
  "&records=%s"                                    \
  "&type=Video"                                    \
  "&tags=%s"                                       \
  "&excludeTags=%s"

#define RAITV_VIDEO_RECENT                            \
  "http://www.rai.it/StatisticheProxy/proxyPost.jsp?" \
  "action=getLastContentByTag"                        \
  "&domain=RaiTv"                                     \
  "&numContents=%s"                                   \
  "&type=Video"                                       \
  "&tags=%s"                                          \
  "&excludeTags=%s"



/* --- Plugin information --- */

#define SOURCE_ID   "grl-raitv"
#define SOURCE_NAME "Rai.tv"
#define SOURCE_DESC _("A source for browsing and searching Rai.tv videos")

typedef enum {
  RAITV_MEDIA_TYPE_ROOT,
  RAITV_MEDIA_TYPE_POPULARS,
  RAITV_MEDIA_TYPE_RECENTS,
  RAITV_MEDIA_TYPE_POPULAR_THEME,
  RAITV_MEDIA_TYPE_RECENT_THEME,
  RAITV_MEDIA_TYPE_VIDEO,
} RaitvMediaType;

typedef struct {
  gchar *id;
  gchar *name;
  guint count;
  gchar *tags;
  gchar *excludeTags;
} CategoryInfo;


typedef struct
{
  GrlSource *source;
  guint      operation_id;
  const gchar *container_id;
  guint      count;
  guint      length;
  guint      offset;
  guint      skip;

  GrlSourceResultCb 	callback;
  GrlSourceResolveCb 	resolveCb;
  gpointer      user_data;
  gchar*	text;

  CategoryInfo 	*category_info;
  GrlMedia	*media;

  GCancellable *cancellable;
} RaitvOperation;

typedef struct
{
  GrlKeyID     grl_key;
  const gchar *exp;
} RaitvAssoc;





/* ==================== Private Data  ================= */

struct _GrlRaitvSourcePrivate
{
  GrlNetWc *wc;

  GList *raitv_search_mappings;
  GList *raitv_browse_mappings;
};

G_DEFINE_TYPE_WITH_PRIVATE (GrlRaitvSource, grl_raitv_source, GRL_TYPE_SOURCE)

guint root_dir_size = 2;
CategoryInfo root_dir[] = {
  {RAITV_POPULARS_ID,    RAITV_POPULARS_NAME,      23},
  {RAITV_RECENTS_ID, 	RAITV_RECENTS_NAME,	 23},
  {NULL, NULL, 0}
};

CategoryInfo themes_dir[] = {
  {"all",N_("All"),-1,"","Tematica:News^Tematica:ntz"},
  {"bianco_nero",N_("Black and White"),-1,"Tematica:Bianco e Nero",""},
  {"cinema",N_("Cinema"),-1,"Tematica:Cinema",""},
  {"comici",N_("Comedians"),-1,"Tematica:Comici",""},
  {"cronaca",N_("Chronicle"),-1,"Tematica:Cronaca",""},
  {"cultura",N_("Culture"),-1,"Tematica:Cultura",""},
  {"economia",N_("Economy"),-1,"Tematica:Economia",""},
  {"fiction",N_("Fiction"),-1,"Tematica:Fiction",""},
  {"junior",N_("Junior"),-1,"Tematica:Junior",""},
  {"inchieste",N_("Investigations"),-1,"Tematica:Inchieste",""},
  {"interviste",N_("Interviews"),-1,"Tematica:Interviste",""},
  {"musica",N_("Music"),-1,"Tematica:Musica",""},
  {"news",N_("News"),-1,"Tematica:News",""},
  {"salute",N_("Health"),-1,"Tematica:Salute",""},
  {"satira",N_("Satire"),-1,"Tematica:Satira",""},
  {"scienza","Science",-1,"Tematica:Scienza",""},
  {"societa",N_("Society"),-1,"Tematica:Societa",""},
  {"spettacolo",N_("Show"),-1,"Tematica:Spettacolo",""},
  {"sport",N_("Sport"),-1,"Tematica:Sport",""},
  {"storia",N_("History"),-1,"Tematica:Storia",""},
  {"politica",N_("Politics"),-1,"Tematica:Politica",""},
  {"tempo_libero",N_("Leisure"),-1,"Tematica:Tempo libero",""},
  {"viaggi",N_("Travel"),-1,"Tematica:Viaggi",""},
  {NULL, NULL, 0}
};


/**/

static GrlRaitvSource *grl_raitv_source_new (void);

gboolean grl_raitv_plugin_init (GrlRegistry *registry,
                                GrlPlugin *plugin,
                                GList *configs);

static const GList *grl_raitv_source_supported_keys (GrlSource *source);

static void grl_raitv_source_browse (GrlSource *source,
                                     GrlSourceBrowseSpec *bs);

static void grl_raitv_source_search (GrlSource *source,
                                     GrlSourceSearchSpec *ss);

static void grl_raitv_source_resolve (GrlSource *source,
                                      GrlSourceResolveSpec *ss);

static void g_raitv_videos_search(RaitvOperation *op);

static void produce_from_popular_theme (RaitvOperation *op);
static void produce_from_recent_theme (RaitvOperation *op);

static void grl_raitv_source_cancel (GrlSource *source,
                                     guint operation_id);

static RaitvMediaType classify_media_id (const gchar *media_id);


/**/

gboolean
grl_raitv_plugin_init (GrlRegistry *registry,
                       GrlPlugin *plugin,
                       GList *configs)
{
  GRL_LOG_DOMAIN_INIT (raitv_log_domain, "raitv");

  /* Initialize i18n */
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  GrlRaitvSource *source = grl_raitv_source_new ();
  grl_registry_register_source (registry,
                                plugin,
                                GRL_SOURCE (source),
                                NULL);
  return TRUE;
}

GRL_PLUGIN_DEFINE (GRL_MAJOR,
                   GRL_MINOR,
                   RAITV_PLUGIN_ID,
                   "Rai.tv",
                   "A plugin for searching multimedia content using Rai.tv",
                   "Marco Piazza",
                   VERSION,
                   "LGPL-2.1-or-later",
                   "https://wiki.gnome.org/Projects/Grilo",
                   grl_raitv_plugin_init,
                   NULL,
                   NULL);

/* ================== Rai.tv GObject ================ */

static GrlRaitvSource *
grl_raitv_source_new (void)
{
  GIcon *icon;
  GFile *file;
  GrlRaitvSource *source;
  const char *tags[] = {
    "country:it",
    "tv",
    "net:internet",
    NULL
  };

  file = g_file_new_for_uri ("resource:///org/gnome/grilo/plugins/raitv/channel-rai.svg");
  icon = g_file_icon_new (file);
  g_object_unref (file);
  source = g_object_new (GRL_TYPE_RAITV_SOURCE,
                         "source-id", SOURCE_ID,
                         "source-name", SOURCE_NAME,
                         "source-desc", SOURCE_DESC,
                         "supported-media", GRL_SUPPORTED_MEDIA_VIDEO,
                         "source-icon", icon,
                         "source-tags", tags,
                         NULL);
  g_object_unref (icon);

  return source;
}

static void
grl_raitv_source_dispose (GObject *object)
{
  G_OBJECT_CLASS (grl_raitv_source_parent_class)->dispose (object);
}

static void
grl_raitv_source_finalize (GObject *object)
{
  GrlRaitvSource *source = GRL_RAITV_SOURCE (object);

  g_clear_object (&source->priv->wc);

  if (source->priv->raitv_search_mappings != NULL) {
    g_list_free_full (source->priv->raitv_search_mappings, g_free);
    source->priv->raitv_search_mappings = NULL;
  }

  if (source->priv->raitv_browse_mappings != NULL) {
    g_list_free_full (source->priv->raitv_browse_mappings, g_free);
    source->priv->raitv_browse_mappings = NULL;
  }


  G_OBJECT_CLASS (grl_raitv_source_parent_class)->finalize (object);
}

static void
grl_raitv_source_class_init (GrlRaitvSourceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

  object_class->dispose = grl_raitv_source_dispose;
  object_class->finalize = grl_raitv_source_finalize;

  source_class->supported_keys = grl_raitv_source_supported_keys;

  source_class->cancel = grl_raitv_source_cancel;
  source_class->browse = grl_raitv_source_browse;
  source_class->search = grl_raitv_source_search;
  source_class->resolve = grl_raitv_source_resolve;

}

static RaitvAssoc *
raitv_build_mapping (GrlKeyID grl_key, const gchar *exp)
{
  RaitvAssoc *assoc = g_new (RaitvAssoc, 1);

  assoc->grl_key = grl_key;
  assoc->exp     = exp;

  return assoc;
}

static void
grl_raitv_source_init (GrlRaitvSource *self)
{
  self->priv = grl_raitv_source_get_instance_private (self);

  self->priv->wc = grl_net_wc_new ();
  grl_net_wc_set_throttling (self->priv->wc, 1);

  //Insert search mapping
  self->priv->raitv_search_mappings = g_list_append (self->priv->raitv_search_mappings,
                                                     raitv_build_mapping(GRL_METADATA_KEY_ID, "HAS/C/@CID"));

  self->priv->raitv_search_mappings = g_list_append (self->priv->raitv_search_mappings,
                                                     raitv_build_mapping(GRL_METADATA_KEY_PUBLICATION_DATE, "MT[@N='itemDate']/@V"));

  self->priv->raitv_search_mappings = g_list_append (self->priv->raitv_search_mappings,
                                                     raitv_build_mapping(GRL_METADATA_KEY_TITLE, "MT[@N='title\']/@V"));

  self->priv->raitv_search_mappings = g_list_append (self->priv->raitv_search_mappings,
                                                     raitv_build_mapping(GRL_METADATA_KEY_URL, "MT[@N='videourl']/@V"));

  self->priv->raitv_search_mappings = g_list_append (self->priv->raitv_search_mappings,
                                                     raitv_build_mapping(GRL_METADATA_KEY_THUMBNAIL, "MT[@N='vod-image']/@V"));


  //Insert browse mapping
  self->priv->raitv_browse_mappings = g_list_append (self->priv->raitv_browse_mappings,
                                                     raitv_build_mapping(GRL_METADATA_KEY_ID, "localid"));
  self->priv->raitv_browse_mappings = g_list_append (self->priv->raitv_browse_mappings,
                                                     raitv_build_mapping(GRL_METADATA_KEY_PUBLICATION_DATE, "datacreazione"));
  self->priv->raitv_browse_mappings = g_list_append (self->priv->raitv_browse_mappings,
                                                     raitv_build_mapping(GRL_METADATA_KEY_TITLE, "titolo"));
  self->priv->raitv_browse_mappings = g_list_append (self->priv->raitv_browse_mappings,
                                                     raitv_build_mapping(GRL_METADATA_KEY_URL, "h264"));
  self->priv->raitv_browse_mappings = g_list_append (self->priv->raitv_browse_mappings,
                                                     raitv_build_mapping(GRL_METADATA_KEY_THUMBNAIL, "pathImmagine"));

}


static void
raitv_operation_free (RaitvOperation *op)
{
  g_clear_object (&op->cancellable);
  g_clear_object (&op->source);
  g_slice_free (RaitvOperation, op);
}


/* ================== Callbacks ================ */

static void
proxy_call_search_grlnet_async_cb (GObject *source_object,
                                   GAsyncResult *res,
                                   gpointer user_data)
{
  RaitvOperation    *op = (RaitvOperation *) user_data;
  xmlDocPtr           doc = NULL;
  xmlXPathContextPtr  xpath = NULL;
  xmlXPathObjectPtr   obj = NULL;
  gint i, nb_items = 0;
  gsize length;

  GRL_DEBUG ("Response id=%u", op->operation_id);

  GError *wc_error = NULL;
  GError *error = NULL;
  gchar *content = NULL;
  gboolean g_bVideoNotFound = TRUE;

  if (g_cancellable_is_cancelled (op->cancellable)) {
    goto finalize;
  }

  if (!grl_net_wc_request_finish (GRL_NET_WC (source_object),
                                  res,
                                  &content,
                                  &length,
                                  &wc_error)) {
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_SEARCH_FAILED,
                         _("Failed to search: %s"),
                         wc_error->message);

    op->callback (op->source,
                  op->operation_id,
                  NULL,
                  0,
                  op->user_data,
                  error);

   g_error_free (wc_error);
    g_error_free (error);

    return;
  }

  doc = xmlParseMemory (content, (gint) length);

  if (!doc) {
    GRL_DEBUG ("Doc failed");
    goto finalize;
  }

  xpath = xmlXPathNewContext (doc);
  if (!xpath) {
    GRL_DEBUG ("Xpath failed");
    goto finalize;
  }
  obj = xmlXPathEvalExpression ((xmlChar *) "/GSP/RES/R", xpath);
  if (obj)
    {
      nb_items = xmlXPathNodeSetGetLength (obj->nodesetval);
      xmlXPathFreeObject (obj);
    }

  for (i = 0; i < nb_items; i++)
    {
      //Search only videos
      gchar *str;
      str = g_strdup_printf ("string(/GSP/RES/R[%i]/MT[@N='videourl']/@V)",
                             i + 1);
      obj = xmlXPathEvalExpression ((xmlChar *) str, xpath);
      if (obj->stringval && obj->stringval[0] == '\0')
        continue;
      if(op->skip>0) {
        op->skip--;
        continue;
      }

      GrlRaitvSource *source = GRL_RAITV_SOURCE (op->source);
      GList *mapping = source->priv->raitv_search_mappings;
      GrlMedia *media = grl_media_video_new ();
      g_bVideoNotFound = FALSE;
      GRL_DEBUG ("Mappings count: %d",g_list_length(mapping));
      while (mapping)
        {
          RaitvAssoc *assoc = (RaitvAssoc *) mapping->data;
          str = g_strdup_printf ("string(/GSP/RES/R[%i]/%s)",
                                 i + 1, assoc->exp);

          GRL_DEBUG ("Xquery %s", str);
          gchar *strvalue;

          obj = xmlXPathEvalExpression ((xmlChar *) str, xpath);
          if (obj)
            {
              if (obj->stringval && obj->stringval[0] != '\0')
                {
                  strvalue = 	g_strdup((gchar *) obj->stringval);
                  //Sometimes GRL_METADATA_KEY_THUMBNAIL doesn't report complete url
                  if(assoc->grl_key == GRL_METADATA_KEY_THUMBNAIL && !g_str_has_prefix(strvalue,"http://www.rai.tv")) {
                    strvalue = g_strdup_printf("http://www.rai.tv%s",obj->stringval);
                  }

                  GType _type;
                  GRL_DEBUG ("\t%s -> %s", str, obj->stringval);
                  _type = grl_metadata_key_get_type (assoc->grl_key);
                  switch (_type)
                    {
                    case G_TYPE_STRING:
                      grl_data_set_string (GRL_DATA (media),
                                           assoc->grl_key,
                                           strvalue);
                      break;

                    case G_TYPE_INT:
                      grl_data_set_int (GRL_DATA (media),
                                        assoc->grl_key,
                                        (gint) atoi (strvalue));
                      break;

                    case G_TYPE_FLOAT:
                      grl_data_set_float (GRL_DATA (media),
                                          assoc->grl_key,
                                          (gfloat) atof (strvalue));
                      break;

                    default:
                      /* G_TYPE_DATE_TIME is not a constant, so this has to be
                       * in "default:" */
                      if (_type == G_TYPE_DATE_TIME) {
                        int year,month,day;
                        sscanf((const char*)obj->stringval, "%02d/%02d/%04d", &day, &month, &year);
                        GDateTime *date = g_date_time_new_local (year, month, day, 0, 0, 0);
                        GRL_DEBUG ("Setting %s to %s",
                                   grl_metadata_key_get_name (assoc->grl_key),
                                   g_date_time_format (date, "%F %H:%M:%S"));
                        grl_data_set_boxed (GRL_DATA (media),
                                            assoc->grl_key, date);
                        if(date) g_date_time_unref (date);
                      } else {
                        GRL_DEBUG ("\tUnexpected data type: %s",
                                   g_type_name (_type));
                      }
                      break;
                    }
                  g_free (strvalue);
                }
              xmlXPathFreeObject (obj);
            }

          g_free (str);

          mapping = mapping->next;
        }

      op->callback (op->source,
                    op->operation_id,
                    media,
                    --op->count,
                    op->user_data,
                    NULL);

      if (op->count == 0)
        break;
    }

 finalize:
  g_clear_pointer (&xpath, xmlXPathFreeContext);
  g_clear_pointer (&doc, xmlFreeDoc);

  /* Signal the last element if it was not already signaled */
  if (nb_items == 0 || g_bVideoNotFound) {
    op->callback (op->source,
                  op->operation_id,
                  NULL,
                  0,
                  op->user_data,
                  NULL);
  }
  else {
    //Continue the search
    if(op->count>0) {
		op->offset +=  nb_items;
		g_raitv_videos_search(op);
    }
  }

}



static void
proxy_call_browse_grlnet_async_cb (GObject *source_object,
                                   GAsyncResult *res,
                                   gpointer user_data)
{
  RaitvOperation    *op = (RaitvOperation *) user_data;
  xmlDocPtr           doc = NULL;
  xmlXPathContextPtr  xpath = NULL;
  xmlXPathObjectPtr   obj = NULL;
  gint i, nb_items = 0;
  gsize length;


  GRL_DEBUG ("Response id=%u", op->operation_id);

  GError *wc_error = NULL;
  GError *error = NULL;
  gchar *content = NULL;

  if (g_cancellable_is_cancelled (op->cancellable)) {
    goto finalize;
  }


  if (!grl_net_wc_request_finish (GRL_NET_WC (source_object),
                                  res,
                                  &content,
                                  &length,
                                  &wc_error)) {
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_SEARCH_FAILED,
                         _("Failed to browse: %s"),
                         wc_error->message);

    op->callback (op->source,
                  op->operation_id,
                  NULL,
                  0,
                  op->user_data,
                  error);

    g_error_free (wc_error);
    g_error_free (error);

    return;
  }

  /* Work-around leading linefeed */
  if (*content == '\n')
    doc = xmlRecoverMemory (content + 1, (gint) length - 1);
  else
    doc = xmlRecoverMemory (content, (gint) length);

  if (!doc) {
    GRL_DEBUG ("Doc failed");
    goto finalize;
  }

  xpath = xmlXPathNewContext (doc);
  if (!xpath) {
    GRL_DEBUG ("Xpath failed");
    goto finalize;
  }

  gchar* xquery=NULL;
  switch (classify_media_id (op->container_id))
    {
    case RAITV_MEDIA_TYPE_POPULAR_THEME:
      xquery = "/CLASSIFICAVISTI/content";
      break;
    case RAITV_MEDIA_TYPE_RECENT_THEME:
      xquery = "/INFORMAZIONICONTENTS/content";
      break;
    default:
      goto finalize;
    }

  obj = xmlXPathEvalExpression ((xmlChar *) xquery, xpath);
  if (obj)
    {
      nb_items = xmlXPathNodeSetGetLength (obj->nodesetval);
      xmlXPathFreeObject (obj);
    }

  if (nb_items < op->count)
    op->count = nb_items;

  op->category_info->count = nb_items;

  for (i = 0; i < nb_items; i++)
    {

      //Skip
      if(op->skip>0) {
        op->skip--;
        continue;
      }

      GrlRaitvSource *source = GRL_RAITV_SOURCE (op->source);
      GList *mapping = source->priv->raitv_browse_mappings;
      GrlMedia *media = grl_media_video_new ();

      while (mapping)
        {
          gchar *str;
          gchar *strvalue;

          RaitvAssoc *assoc = (RaitvAssoc *) mapping->data;
          str = g_strdup_printf ("string(%s[%i]/%s)",
                                 xquery,i + 1, assoc->exp);


          obj = xmlXPathEvalExpression ((xmlChar *) str, xpath);
          if (obj)
            {
              if (obj->stringval && obj->stringval[0] != '\0')
                {
                  strvalue = 	g_strdup((gchar *) obj->stringval);
                  //Sometimes GRL_METADATA_KEY_THUMBNAIL doesn't report complete url
                  if(assoc->grl_key == GRL_METADATA_KEY_THUMBNAIL && !g_str_has_prefix(strvalue,"http://www.rai.tv/")) {
                    strvalue = g_strdup_printf("http://www.rai.tv%s",obj->stringval);
                  }

                  GType _type;
                  _type = grl_metadata_key_get_type (assoc->grl_key);
                  switch (_type)
                    {
                    case G_TYPE_STRING:
                      grl_data_set_string (GRL_DATA (media),
                                           assoc->grl_key,
                                           strvalue);
                      break;

                    case G_TYPE_INT:
                      grl_data_set_int (GRL_DATA (media),
                                        assoc->grl_key,
                                        (gint) atoi (strvalue));
                      break;

                    case G_TYPE_FLOAT:
                      grl_data_set_float (GRL_DATA (media),
                                          assoc->grl_key,
                                          (gfloat) atof (strvalue));
                      break;

                    default:
                      /* G_TYPE_DATE_TIME is not a constant, so this has to be
                       * in "default:" */
                      if (_type == G_TYPE_DATE_TIME) {
                        int year,month,day,hour,minute,seconds;
                        sscanf((const char*)obj->stringval, "%02d/%02d/%04d %02d:%02d:%02d",
                               &day, &month, &year, &hour,&minute, &seconds);
                        GDateTime *date = g_date_time_new_local (year, month, day, hour,minute, seconds);
                        grl_data_set_boxed (GRL_DATA (media),
                                            assoc->grl_key, date);
                        if(date) g_date_time_unref (date);
                      } else {
                        GRL_DEBUG ("\tUnexpected data type: %s",
                                   g_type_name (_type));
                      }
                      break;
                    }
                  g_free (strvalue);
                }
              xmlXPathFreeObject (obj);
            }

          g_free (str);

          mapping = mapping->next;
        }

      op->callback (op->source,
                    op->operation_id,
                    media,
                    --op->count,
                    op->user_data,
                    NULL);

      if (op->count == 0) {
        break;
      }
    }

 finalize:
  g_clear_pointer (&xpath, xmlXPathFreeContext);
  g_clear_pointer (&doc, xmlFreeDoc);

  /* Signal the last element if it was not already signaled */
  if (nb_items == 0) {
    op->callback (op->source,
                  op->operation_id,
                  NULL,
                  0,
                  op->user_data,
                  NULL);
  }
  else {
    //Continue the search
    if(op->count>0) {
      //Skip the ones already read
      op->skip +=  nb_items;
      op->offset +=  nb_items;
      switch (classify_media_id (op->container_id))
		  {
        case RAITV_MEDIA_TYPE_POPULAR_THEME:
          produce_from_popular_theme(op);
          break;
        case RAITV_MEDIA_TYPE_RECENT_THEME:
          produce_from_recent_theme(op);
          break;
        default:
          g_assert_not_reached ();
          break;
		  }

    }
  }

}

static gchar *
eval_xquery (const gchar *xquery,
             xmlXPathContextPtr xpath)
{
  int i;
  xmlChar *szValue;
  xmlNodePtr curNode;
  xmlNodeSetPtr nodeset;
  xmlXPathObjectPtr xobj;

  xobj = xmlXPathEvalExpression ((xmlChar *) xquery, xpath);

  if(xobj != NULL) {
    nodeset = xobj->nodesetval;
    for (i = 0; i < nodeset->nodeNr; i++) {
        curNode = nodeset->nodeTab[i];
        if(curNode != NULL)
          {
            szValue = xmlGetProp(curNode,BAD_CAST "content");
            if (szValue != NULL)
              {
                xmlXPathFreeObject (xobj);
                return (gchar *) szValue;
              }
          }
      }
    xmlXPathFreeObject (xobj);
  }

  return NULL;
}

static void
proxy_call_resolve_grlnet_async_cb (GObject *source_object,
                                    GAsyncResult *res,
                                    gpointer user_data)
{
  RaitvOperation     *op = (RaitvOperation *) user_data;
  xmlDocPtr           doc = NULL;
  xmlXPathContextPtr  xpath = NULL;
  GError             *wc_error = NULL;
  GError             *error = NULL;
  gchar              *content = NULL;
  gsize               length;
  gchar              *value;
  gchar              *thumbnail;
  gchar             **tokens;
  GDateTime          *date;

  GRL_DEBUG ("Response id=%u", op->operation_id);

  if (g_cancellable_is_cancelled (op->cancellable)) {
    goto finalize;
  }


  if (!grl_net_wc_request_finish (GRL_NET_WC (source_object),
                                  res,
                                  &content,
                                  &length,
                                  &wc_error)) {
    error = g_error_new (GRL_CORE_ERROR,
                         GRL_CORE_ERROR_SEARCH_FAILED,
                         _("Failed to resolve: %s"),
                         wc_error->message);

    op->resolveCb (op->source,
                   op->operation_id,
                   op->media,
                   op->user_data,
                   error);

    g_error_free (wc_error);
    g_error_free (error);

    return;
  }

  doc = xmlRecoverMemory (content, (gint) length);

  if (!doc) {
    GRL_DEBUG ("Doc failed");
    goto finalize;
  }

  xpath = xmlXPathNewContext (doc);
  if (!xpath) {
    GRL_DEBUG ("Xpath failed");
    goto finalize;
  }

  if (!grl_data_has_key (GRL_DATA (op->media), GRL_METADATA_KEY_URL)) {
    value = eval_xquery ("/html/head/meta[@name='videourl']", xpath);
    if (value) {
      grl_media_set_url (op->media, value);
      g_free (value);
    }
  }

  if (!grl_data_has_key (GRL_DATA (op->media), GRL_METADATA_KEY_TITLE)) {
    value = eval_xquery ("/html/head/meta[@name='title']", xpath);
    if (value) {
      grl_media_set_title (op->media, value);
      g_free (value);
    }
  }

  if (!grl_data_has_key (GRL_DATA (op->media), GRL_METADATA_KEY_PUBLICATION_DATE)) {
    value = eval_xquery ("/html/head/meta[@name='itemDate']", xpath);
    if (value) {
      tokens = g_strsplit (value, "/", -1);
      if (g_strv_length (tokens) >= 3) {
        date = g_date_time_new_local (atoi (tokens[2]), atoi (tokens[1]), atoi (tokens[0]), 0, 0, 0);
        grl_media_set_publication_date (op->media, date);
        g_date_time_unref (date);
      }
      g_strfreev (tokens);
      g_free (value);
    }
  }

  if (!grl_data_has_key (GRL_DATA (op->media), GRL_METADATA_KEY_THUMBNAIL)) {
    value = eval_xquery ("/html/head/meta[@name='vod-image']", xpath);
    if (value) {
      /* Sometimes thumbnail doesn't report a complete url */
      if (value[0] == '/') {
        thumbnail = g_strconcat ("http://www.rai.tv", value, NULL);
        g_free (value);
      } else {
        thumbnail = value;
      }

      grl_media_set_thumbnail (op->media, thumbnail);
      g_free (thumbnail);
    }
  }

 finalize:
  op->resolveCb (op->source,
                 op->operation_id,
                 op->media,
                 op->user_data,
                 NULL);

  g_clear_pointer (&xpath, xmlXPathFreeContext);
  g_clear_pointer (&doc, xmlFreeDoc);
}



static guint
get_theme_index_from_id (const gchar *category_id)
{
  guint i;

  for (i=0; i<root_dir[ROOT_DIR_POPULARS_INDEX].count; i++) {
    if (g_strrstr (category_id, themes_dir[i].id)) {
      return i;
    }
  }
  g_assert_not_reached ();
}

static gboolean
is_popular_container (const gchar *container_id)
{
  return g_str_has_prefix (container_id, RAITV_POPULARS_THEME_ID "/");
}

static gboolean
is_recent_container (const gchar *container_id)
{
  return g_str_has_prefix (container_id, RAITV_RECENTS_THEME_ID "/");
}

static RaitvMediaType
classify_media_id (const gchar *media_id)
{
  if (!media_id) {
    return RAITV_MEDIA_TYPE_ROOT;
  } else if (!strcmp (media_id, RAITV_POPULARS_ID)) {
    return RAITV_MEDIA_TYPE_POPULARS;
  } else if (!strcmp (media_id, RAITV_RECENTS_ID)) {
    return RAITV_MEDIA_TYPE_RECENTS;
  } else if (is_popular_container (media_id)) {
    return RAITV_MEDIA_TYPE_POPULAR_THEME;
  } else if (is_recent_container (media_id)) {
    return RAITV_MEDIA_TYPE_RECENT_THEME;
  } else {
    return RAITV_MEDIA_TYPE_VIDEO;
  }
}


static GrlMedia *
produce_container_from_directory (GrlMedia *media,
                                  CategoryInfo *dir,
                                  guint index,
                                  RaitvMediaType type)
{
  GrlMedia *content;
  gchar* mediaid=NULL;

  if (!media) {
    content = grl_media_container_new ();
  } else {
    content = media;
  }

  if (!dir) {
    grl_media_set_id (content, NULL);
    grl_media_set_title (content, RAITV_ROOT_NAME);
  } else {

    switch(type)
      {
      case RAITV_MEDIA_TYPE_ROOT :
      case RAITV_MEDIA_TYPE_POPULARS :
      case RAITV_MEDIA_TYPE_RECENTS :
        mediaid = g_strdup_printf("%s",dir[index].id);
        break;
      case RAITV_MEDIA_TYPE_POPULAR_THEME :
        mediaid = g_strdup_printf("%s/%s", RAITV_POPULARS_THEME_ID, dir[index].id);
        break;
      case RAITV_MEDIA_TYPE_RECENT_THEME :
        mediaid = g_strdup_printf("%s/%s", RAITV_RECENTS_THEME_ID, dir[index].id);
        break;
      default: break;
      }

    GRL_DEBUG ("MediaId=%s, Type:%d, Titolo:%s",mediaid, type, dir[index].name);

    grl_media_set_id (content, mediaid);
    grl_media_set_title (content, g_dgettext (GETTEXT_PACKAGE, dir[index].name));
    g_free(mediaid);
  }

  return content;
}

static void
produce_from_directory (CategoryInfo *dir, gint dir_size, RaitvOperation *os,
                        RaitvMediaType type)
{
  guint index, remaining;

  GRL_DEBUG ("Produce_from_directory. Size=%d",dir_size);

  if (os->skip >= dir_size) {
    /* No results */
    os->callback (os->source,
                  os->operation_id,
                  NULL,
                  0,
                  os->user_data,
                  NULL);
  } else {
    index = os->skip;
    remaining = MIN (dir_size - os->skip, os->count);

    do {
      GrlMedia *content =
        produce_container_from_directory (NULL, dir, index, type);

      remaining--;
      index++;

      os->callback (os->source,
                    os->operation_id,
                    content,
                    remaining,
                    os->user_data,
                    NULL);

    } while (remaining > 0);
  }
}

static void
produce_from_popular_theme (RaitvOperation *op)
{
  guint category_index;
  gchar	*start = NULL;
  gchar	*url = NULL;

  GrlRaitvSource *source = GRL_RAITV_SOURCE (op->source);
  start = g_strdup_printf ("%u", op->offset+op->length);

  category_index = get_theme_index_from_id (op->container_id);
  GRL_DEBUG ("produce_from_popular_theme (container_id=%s, category_index=%d",op->container_id,category_index);

  op->category_info = &themes_dir[category_index];
  url = g_strdup_printf (RAITV_VIDEO_POPULAR, start, op->category_info->tags, op->category_info->excludeTags);

  GRL_DEBUG ("Starting browse request for popular theme (%s)", url);
  grl_net_wc_request_async (source->priv->wc,
                            url,
                            op->cancellable,
                            proxy_call_browse_grlnet_async_cb,
                            op);

  g_free (url);
}



static void
produce_from_recent_theme (RaitvOperation *op)
{
  guint category_index;
  gchar	*start = NULL;
  gchar	*url = NULL;

  GrlRaitvSource *source = GRL_RAITV_SOURCE (op->source);

  category_index = get_theme_index_from_id (op->container_id);
  GRL_DEBUG ("produce_from_recent_theme (container_id=%s, category_index=%d",op->container_id,category_index);

  start = g_strdup_printf ("%u", op->offset+op->length);

  op->category_info = &themes_dir[category_index];
  url = g_strdup_printf (RAITV_VIDEO_RECENT, start, op->category_info->tags, op->category_info->excludeTags);

  GRL_DEBUG ("Starting browse request for recent theme (%s)", url);
  grl_net_wc_request_async (source->priv->wc,
                            url,
                            op->cancellable,
                            proxy_call_browse_grlnet_async_cb,
                            op);

  g_free (url);
}


/* ================== API Implementation ================ */

static const GList *
grl_raitv_source_supported_keys (GrlSource *source)
{
  static GList *keys = NULL;
  if (!keys) {
    keys = grl_metadata_key_list_new (GRL_METADATA_KEY_ID,
                                      GRL_METADATA_KEY_PUBLICATION_DATE,
                                      GRL_METADATA_KEY_TITLE,
                                      GRL_METADATA_KEY_URL,
                                      GRL_METADATA_KEY_THUMBNAIL,
                                      NULL);
  }
  return keys;
}

static void
grl_raitv_source_browse (GrlSource *source,
                         GrlSourceBrowseSpec *bs)
{
  RaitvOperation *op = g_slice_new0 (RaitvOperation);

  const gchar *container_id;
  GRL_DEBUG ("%s: %s", __FUNCTION__, grl_media_get_id (bs->container));
  container_id = grl_media_get_id (bs->container);

  op->source       = g_object_ref (source);
  op->cancellable  = g_cancellable_new ();
  op->length        = grl_operation_options_get_count (bs->options);
  op->operation_id = bs->operation_id;
  op->container_id = container_id;
  op->callback     = bs->callback;
  op->user_data    = bs->user_data;
  op->skip	   = grl_operation_options_get_skip (bs->options);
  op->count	   = op->length;
  op->offset       = 0;

  grl_operation_set_data_full (bs->operation_id, op, (GDestroyNotify) raitv_operation_free);

  RaitvMediaType type = classify_media_id (container_id);
  switch (type)
    {
    case RAITV_MEDIA_TYPE_ROOT:
      produce_from_directory (root_dir, root_dir_size, op, type);
      break;
    case RAITV_MEDIA_TYPE_POPULARS:
      produce_from_directory (themes_dir,
                              root_dir[ROOT_DIR_POPULARS_INDEX].count, op, RAITV_MEDIA_TYPE_POPULAR_THEME);
      break;
    case RAITV_MEDIA_TYPE_RECENTS:
      produce_from_directory (themes_dir,
                              root_dir[ROOT_DIR_RECENTS_INDEX].count, op, RAITV_MEDIA_TYPE_RECENT_THEME);
      break;
    case RAITV_MEDIA_TYPE_POPULAR_THEME:
      produce_from_popular_theme (op);
      break;
    case RAITV_MEDIA_TYPE_RECENT_THEME:
      produce_from_recent_theme (op);
      break;
    case RAITV_MEDIA_TYPE_VIDEO:
    default:
      g_assert_not_reached ();
      break;
    }

}

static void
grl_raitv_source_search (GrlSource *source,
                         GrlSourceSearchSpec *ss)
{
  RaitvOperation *op = g_slice_new0 (RaitvOperation);

  op->source       = g_object_ref (source);
  op->cancellable  = g_cancellable_new ();
  op->length        = grl_operation_options_get_count (ss->options);
  op->operation_id = ss->operation_id;
  op->callback     = ss->callback;
  op->user_data    = ss->user_data;
  op->skip	   = grl_operation_options_get_skip (ss->options);
  op->count	   = op->length;
  op->offset       = 0;
  op->text	   = ss->text;

  grl_operation_set_data_full (ss->operation_id, op, (GDestroyNotify) raitv_operation_free);

  g_raitv_videos_search(op);
}

static void
g_raitv_videos_search(RaitvOperation *op) {

  gchar *start;
  gchar *url = NULL;

  GrlRaitvSource *source = GRL_RAITV_SOURCE (op->source);

  start = g_strdup_printf ("%u", op->offset);

  url = g_strdup_printf (RAITV_VIDEO_SEARCH, op->text, start);

  GRL_DEBUG ("Starting search request (%s)", url);
  grl_net_wc_request_async (source->priv->wc,
                            url,
                            op->cancellable,
                            proxy_call_search_grlnet_async_cb,
                            op);
  g_free (start);
  g_free (url);

}


static void
grl_raitv_source_resolve (GrlSource *source,
                          GrlSourceResolveSpec *rs)
{
  gchar *urltarget;
  GrlRaitvSource *self = GRL_RAITV_SOURCE (source);
  RaitvOperation *op;
  RaitvMediaType mediatype;

  GRL_DEBUG ("Starting resolve source: url=%s",grl_media_get_url (rs->media));

  if (!grl_media_is_video (rs->media) && !grl_media_is_container (rs->media)) {
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
    return;
  }

  mediatype = classify_media_id (grl_media_get_id (rs->media));
  switch (mediatype) {
  case RAITV_MEDIA_TYPE_ROOT:
    rs->media = produce_container_from_directory (rs->media, NULL, 0, mediatype);
    break;
  case RAITV_MEDIA_TYPE_POPULARS:
    rs->media = produce_container_from_directory (rs->media, root_dir, ROOT_DIR_POPULARS_INDEX, mediatype);
    break;
  case RAITV_MEDIA_TYPE_RECENTS:
    rs->media = produce_container_from_directory (rs->media, root_dir, ROOT_DIR_RECENTS_INDEX, mediatype);
    break;
  case RAITV_MEDIA_TYPE_POPULAR_THEME:
  case RAITV_MEDIA_TYPE_RECENT_THEME:
    rs->media = produce_container_from_directory (rs->media,
                                                  themes_dir,
                                                  get_theme_index_from_id (grl_media_get_id (rs->media)),
                                                  mediatype);
    break;
  case RAITV_MEDIA_TYPE_VIDEO:
    op = g_slice_new0 (RaitvOperation);
    op->source       = g_object_ref (source);
    op->cancellable  = g_cancellable_new ();
    op->operation_id = rs->operation_id;
    op->resolveCb    = rs->callback;
    op->user_data    = rs->user_data;
    op->media	      = rs->media;

    grl_operation_set_data_full (rs->operation_id, op, (GDestroyNotify) raitv_operation_free);

    urltarget = g_strdup_printf ("http://www.rai.tv/dl/RaiTV/programmi/media/%s.html",
                                 grl_media_get_id(rs->media));

    GRL_DEBUG ("Opening '%s'", urltarget);

    grl_net_wc_request_async (self->priv->wc,
                              urltarget,
                              op->cancellable,
                              proxy_call_resolve_grlnet_async_cb,
                              op);

    g_free(urltarget);
    return;
  }
  rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
  return;

  if ( grl_media_get_url (rs->media) != NULL) {
    rs->callback (rs->source, rs->operation_id, rs->media, rs->user_data, NULL);
    return;
  }

  op = g_slice_new0 (RaitvOperation);
  op->source       = g_object_ref (source);
  op->cancellable  = g_cancellable_new ();
  op->operation_id = rs->operation_id;
  op->resolveCb     = rs->callback;
  op->user_data    = rs->user_data;
  op->media	   = rs->media;

  grl_operation_set_data_full (rs->operation_id, op, (GDestroyNotify) raitv_operation_free);

  urltarget = g_strdup_printf("%s/%s.html","http://www.rai.tv/dl/RaiTV/programmi/media",grl_media_get_id(rs->media));

  GRL_DEBUG ("Opening '%s'", urltarget);

  grl_net_wc_request_async (self->priv->wc,
                            urltarget,
                            op->cancellable,
                            proxy_call_resolve_grlnet_async_cb,
                            op);

  g_free(urltarget);
}


static void
grl_raitv_source_cancel (GrlSource *source, guint operation_id)
{
  RaitvOperation *op = grl_operation_get_data (operation_id);

  GRL_DEBUG ("Cancelling id=%u", operation_id);

  if (!op)
    {
      GRL_WARNING ("\tNo such operation id=%u", operation_id);
      return;
    }

  if (op->cancellable) {
    g_cancellable_cancel (op->cancellable);
  }
}
