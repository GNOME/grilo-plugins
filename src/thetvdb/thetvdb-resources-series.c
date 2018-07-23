/*
 * Copyright (C) 2014 Victor Toso.
 *
 * Contact: Victor Toso <me@victortoso.com>
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

#include "thetvdb-resources.h"

struct _SeriesResourcePrivate {
  gint64      db_id;
  gdouble     rating;
  gchar      *series_id;
  gchar      *overview;
  gchar      *language;
  gchar      *imdb_id;
  gchar      *first_aired;
  gchar      *series_name;
  gchar      *status;
  gchar      *url_banner;
  gchar      *url_fanart;
  gchar      *url_poster;
  gchar      *zap2it_id;
  gchar      *actor_names;
  gchar      *alias_names;
  gchar      *genres;
};

enum {
  PROP_0,
  PROP_DB_ID,
  PROP_LANGUAGE,
  PROP_SERIES_NAME,
  PROP_SERIES_ID,
  PROP_STATUS,
  PROP_OVERVIEW,
  PROP_IMDB_ID,
  PROP_ZAP2IT_ID,
  PROP_FIRST_AIRED,
  PROP_RATING,
  PROP_ACTOR_NAMES,
  PROP_GENRES,
  PROP_URL_BANNER,
  PROP_URL_FANART,
  PROP_URL_POSTER,
  LAST_PROP
};

static GParamSpec *specs[LAST_PROP];

G_DEFINE_TYPE_WITH_PRIVATE (SeriesResource, series_resource, GOM_TYPE_RESOURCE)

static void
series_resource_finalize (GObject *object)
{
  SeriesResourcePrivate *priv = SERIES_RESOURCE(object)->priv;

  g_clear_pointer (&priv->language, g_free);
  g_clear_pointer (&priv->series_name, g_free);
  g_clear_pointer (&priv->series_id, g_free);
  g_clear_pointer (&priv->status, g_free);
  g_clear_pointer (&priv->overview, g_free);
  g_clear_pointer (&priv->imdb_id, g_free);
  g_clear_pointer (&priv->zap2it_id, g_free);
  g_clear_pointer (&priv->first_aired, g_free);
  g_clear_pointer (&priv->actor_names, g_free);
  g_clear_pointer (&priv->alias_names, g_free);
  g_clear_pointer (&priv->genres, g_free);
  g_clear_pointer (&priv->url_banner, g_free);
  g_clear_pointer (&priv->url_fanart, g_free);
  g_clear_pointer (&priv->url_poster, g_free);

  G_OBJECT_CLASS(series_resource_parent_class)->finalize(object);
}

static void
series_resource_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  SeriesResource *resource = SERIES_RESOURCE(object);

  switch (prop_id) {
  case PROP_DB_ID:
    g_value_set_int64 (value, resource->priv->db_id);
    break;
  case PROP_LANGUAGE:
    g_value_set_string (value, resource->priv->language);
    break;
  case PROP_SERIES_NAME:
    g_value_set_string (value, resource->priv->series_name);
    break;
  case PROP_SERIES_ID:
    g_value_set_string (value, resource->priv->series_id);
    break;
  case PROP_STATUS:
    g_value_set_string (value, resource->priv->status);
    break;
  case PROP_OVERVIEW:
    g_value_set_string (value, resource->priv->overview);
    break;
  case PROP_IMDB_ID:
    g_value_set_string (value, resource->priv->imdb_id);
    break;
  case PROP_ZAP2IT_ID:
    g_value_set_string (value, resource->priv->zap2it_id);
    break;
  case PROP_FIRST_AIRED:
    g_value_set_string (value, resource->priv->first_aired);
    break;
  case PROP_RATING:
    g_value_set_double (value, resource->priv->rating);
    break;
  case PROP_ACTOR_NAMES:
    g_value_set_string (value, resource->priv->actor_names);
    break;
  case PROP_GENRES:
    g_value_set_string (value, resource->priv->genres);
    break;
  case PROP_URL_BANNER:
    g_value_set_string (value, resource->priv->url_banner);
    break;
  case PROP_URL_FANART:
    g_value_set_string (value, resource->priv->url_fanart);
    break;
  case PROP_URL_POSTER:
    g_value_set_string (value, resource->priv->url_poster);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
series_resource_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  SeriesResource *resource = SERIES_RESOURCE(object);

  switch (prop_id) {
  case PROP_DB_ID:
    resource->priv->db_id = g_value_get_int64 (value);
    break;
  case PROP_LANGUAGE:
    g_clear_pointer (&resource->priv->language, g_free);
    resource->priv->language = g_value_dup_string (value);
    break;
  case PROP_SERIES_NAME:
    g_clear_pointer (&resource->priv->series_name, g_free);
    resource->priv->series_name = g_value_dup_string (value);
    break;
  case PROP_SERIES_ID:
    g_clear_pointer (&resource->priv->series_id, g_free);
    resource->priv->series_id = g_value_dup_string (value);
    break;
  case PROP_STATUS:
    g_clear_pointer (&resource->priv->status, g_free);
    resource->priv->status = g_value_dup_string (value);
    break;
  case PROP_OVERVIEW:
    g_clear_pointer (&resource->priv->overview, g_free);
    resource->priv->overview = g_value_dup_string (value);
    break;
  case PROP_IMDB_ID:
    g_clear_pointer (&resource->priv->imdb_id, g_free);
    resource->priv->imdb_id = g_value_dup_string (value);
    break;
  case PROP_ZAP2IT_ID:
    g_clear_pointer (&resource->priv->zap2it_id, g_free);
    resource->priv->zap2it_id = g_value_dup_string (value);
    break;
  case PROP_FIRST_AIRED:
    g_clear_pointer (&resource->priv->first_aired, g_free);
    resource->priv->first_aired = g_value_dup_string (value);
    break;
  case PROP_RATING:
    resource->priv->rating = g_value_get_double (value);
    break;
  case PROP_ACTOR_NAMES:
    g_clear_pointer (&resource->priv->actor_names, g_free);
    resource->priv->actor_names = g_value_dup_string (value);
    break;
  case PROP_GENRES:
    g_clear_pointer (&resource->priv->genres, g_free);
    resource->priv->genres = g_value_dup_string (value);
    break;
  case PROP_URL_BANNER:
    g_clear_pointer (&resource->priv->url_banner, g_free);
    resource->priv->url_banner = g_value_dup_string (value);
    break;
  case PROP_URL_FANART:
    g_clear_pointer (&resource->priv->url_fanart, g_free);
    resource->priv->url_fanart = g_value_dup_string (value);
    break;
  case PROP_URL_POSTER:
    g_clear_pointer (&resource->priv->url_poster, g_free);
    resource->priv->url_poster = g_value_dup_string (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
series_resource_class_init (SeriesResourceClass *klass)
{
  GObjectClass *object_class;
  GomResourceClass *resource_class;

  object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = series_resource_finalize;
  object_class->get_property = series_resource_get_property;
  object_class->set_property = series_resource_set_property;

  resource_class = GOM_RESOURCE_CLASS(klass);
  gom_resource_class_set_table(resource_class, SERIES_TABLE_NAME);

  specs[PROP_DB_ID] = g_param_spec_int64 (SERIES_COLUMN_ID,
                                          NULL, NULL,
                                          0, G_MAXINT64,
                                          0, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_DB_ID,
                                   specs[PROP_DB_ID]);
  gom_resource_class_set_primary_key (resource_class, SERIES_COLUMN_ID);

  specs[PROP_LANGUAGE] = g_param_spec_string (SERIES_COLUMN_LANGUAGE,
                                              NULL, NULL, NULL,
                                              G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_LANGUAGE,
                                   specs[PROP_LANGUAGE]);

  specs[PROP_SERIES_NAME] = g_param_spec_string (SERIES_COLUMN_SERIES_NAME,
                                                 NULL, NULL, NULL,
                                                 G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_SERIES_NAME,
                                   specs[PROP_SERIES_NAME]);

  specs[PROP_SERIES_ID] = g_param_spec_string (SERIES_COLUMN_SERIES_ID,
                                               NULL, NULL, NULL,
                                               G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_SERIES_ID,
                                   specs[PROP_SERIES_ID]);
  gom_resource_class_set_unique (resource_class, SERIES_COLUMN_SERIES_ID);

  specs[PROP_STATUS] = g_param_spec_string (SERIES_COLUMN_STATUS,
                                            NULL, NULL, NULL,
                                            G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_STATUS,
                                   specs[PROP_STATUS]);

  specs[PROP_OVERVIEW] = g_param_spec_string (SERIES_COLUMN_OVERVIEW,
                                              NULL, NULL, NULL,
                                              G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_OVERVIEW,
                                   specs[PROP_OVERVIEW]);

  specs[PROP_IMDB_ID] = g_param_spec_string (SERIES_COLUMN_IMDB_ID,
                                              NULL, NULL, NULL,
                                              G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_IMDB_ID,
                                   specs[PROP_IMDB_ID]);

  specs[PROP_ZAP2IT_ID] = g_param_spec_string (SERIES_COLUMN_ZAP2IT_ID,
                                               NULL, NULL, NULL,
                                               G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ZAP2IT_ID,
                                   specs[PROP_ZAP2IT_ID]);

  specs[PROP_FIRST_AIRED] = g_param_spec_string (SERIES_COLUMN_FIRST_AIRED,
                                                 NULL, NULL, NULL,
                                                 G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_FIRST_AIRED,
                                   specs[PROP_FIRST_AIRED]);

  specs[PROP_RATING] = g_param_spec_double (SERIES_COLUMN_RATING,
                                            NULL, NULL,
                                            0, G_MAXDOUBLE,
                                            0, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_RATING,
                                   specs[PROP_RATING]);

  specs[PROP_ACTOR_NAMES] = g_param_spec_string (SERIES_COLUMN_ACTOR_NAMES,
                                                 NULL, NULL, NULL,
                                                 G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ACTOR_NAMES,
                                   specs[PROP_ACTOR_NAMES]);

  specs[PROP_GENRES] = g_param_spec_string (SERIES_COLUMN_GENRES,
                                            NULL, NULL, NULL,
                                            G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_GENRES,
                                   specs[PROP_GENRES]);

  specs[PROP_URL_BANNER] = g_param_spec_string (SERIES_COLUMN_URL_BANNER,
                                                NULL, NULL, NULL,
                                                G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_URL_BANNER,
                                   specs[PROP_URL_BANNER]);

  specs[PROP_URL_FANART] = g_param_spec_string (SERIES_COLUMN_URL_FANART,
                                                NULL, NULL, NULL,
                                                G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_URL_FANART,
                                   specs[PROP_URL_FANART]);

  specs[PROP_URL_POSTER] = g_param_spec_string (SERIES_COLUMN_URL_POSTER,
                                                NULL, NULL, NULL,
                                                G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_URL_POSTER,
                                   specs[PROP_URL_POSTER]);
}

static void
series_resource_init (SeriesResource *resource)
{
  resource->priv = series_resource_get_instance_private (resource);
}
