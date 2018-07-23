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

struct _EpisodeResourcePrivate {
  gint64      db_id;
  gdouble     rating;
  gchar      *series_id;
  gchar      *overview;
  gchar      *language;
  gchar      *imdb_id;
  gchar      *first_aired;
  guint       season_number;
  guint       episode_number;
  guint       absolute_number;
  gchar      *season_id;
  gchar      *episode_id;
  gchar      *episode_name;
  gchar      *url_episode_screen;
  gchar      *director_names;
  gchar      *guest_stars_names;
};

enum {
  PROP_0,
  PROP_DB_ID,
  PROP_LANGUAGE,
  PROP_SERIES_ID,
  PROP_OVERVIEW,
  PROP_IMDB_ID,
  PROP_FIRST_AIRED,
  PROP_RATING,
  PROP_SEASON_NUMBER,
  PROP_EPISODE_NUMBER,
  PROP_ABSOLUTE_NUMBER,
  PROP_SEASON_ID,
  PROP_EPISODE_ID,
  PROP_EPISODE_NAME,
  PROP_URL_EPISODE_SCREEN,
  PROP_DIRECTOR_NAMES,
  PROP_GUEST_STARS_NAMES,
  LAST_PROP
};

static GParamSpec *specs[LAST_PROP];

G_DEFINE_TYPE_WITH_PRIVATE (EpisodeResource, episode_resource, GOM_TYPE_RESOURCE)

static void
episode_resource_finalize (GObject *object)
{
  EpisodeResourcePrivate *priv = EPISODE_RESOURCE(object)->priv;

  g_clear_pointer (&priv->language, g_free);
  g_clear_pointer (&priv->series_id, g_free);
  g_clear_pointer (&priv->overview, g_free);
  g_clear_pointer (&priv->imdb_id, g_free);
  g_clear_pointer (&priv->first_aired, g_free);
  g_clear_pointer (&priv->season_id, g_free);
  g_clear_pointer (&priv->episode_id, g_free);
  g_clear_pointer (&priv->episode_name, g_free);
  g_clear_pointer (&priv->url_episode_screen, g_free);
  g_clear_pointer (&priv->director_names, g_free);
  g_clear_pointer (&priv->guest_stars_names, g_free);

  G_OBJECT_CLASS(episode_resource_parent_class)->finalize(object);
}

static void
episode_resource_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  EpisodeResource *resource = EPISODE_RESOURCE(object);

  switch (prop_id) {
  case PROP_DB_ID:
    g_value_set_int64 (value, resource->priv->db_id);
    break;
  case PROP_LANGUAGE:
    g_value_set_string (value, resource->priv->language);
    break;
  case PROP_SERIES_ID:
    g_value_set_string (value, resource->priv->series_id);
    break;
  case PROP_OVERVIEW:
    g_value_set_string (value, resource->priv->overview);
    break;
  case PROP_IMDB_ID:
    g_value_set_string (value, resource->priv->imdb_id);
    break;
  case PROP_FIRST_AIRED:
    g_value_set_string (value, resource->priv->first_aired);
    break;
  case PROP_RATING:
    g_value_set_double (value, resource->priv->rating);
    break;
  case PROP_SEASON_NUMBER:
    g_value_set_uint (value, resource->priv->season_number);
    break;
  case PROP_EPISODE_NUMBER:
    g_value_set_uint (value, resource->priv->episode_number);
    break;
  case PROP_ABSOLUTE_NUMBER:
    g_value_set_uint (value, resource->priv->absolute_number);
    break;
  case PROP_SEASON_ID:
    g_value_set_string (value, resource->priv->season_id);
    break;
  case PROP_EPISODE_ID:
    g_value_set_string (value, resource->priv->episode_id);
    break;
  case PROP_EPISODE_NAME:
    g_value_set_string (value, resource->priv->episode_name);
    break;
  case PROP_URL_EPISODE_SCREEN:
    g_value_set_string (value, resource->priv->url_episode_screen);
    break;
  case PROP_DIRECTOR_NAMES:
    g_value_set_string (value, resource->priv->director_names);
    break;
  case PROP_GUEST_STARS_NAMES:
    g_value_set_string (value, resource->priv->guest_stars_names);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
episode_resource_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  EpisodeResource *resource = EPISODE_RESOURCE(object);

  switch (prop_id) {
  case PROP_DB_ID:
    resource->priv->db_id = g_value_get_int64 (value);
    break;
  case PROP_LANGUAGE:
    g_clear_pointer (&resource->priv->language, g_free);
    resource->priv->language = g_value_dup_string (value);
    break;
  case PROP_SERIES_ID:
    g_clear_pointer (&resource->priv->series_id, g_free);
    resource->priv->series_id = g_value_dup_string (value);
    break;
  case PROP_OVERVIEW:
    g_clear_pointer (&resource->priv->overview, g_free);
    resource->priv->overview = g_value_dup_string (value);
    break;
  case PROP_IMDB_ID:
    g_clear_pointer (&resource->priv->imdb_id, g_free);
    resource->priv->imdb_id = g_value_dup_string (value);
    break;
  case PROP_FIRST_AIRED:
    g_clear_pointer (&resource->priv->first_aired, g_free);
    resource->priv->first_aired = g_value_dup_string (value);
    break;
  case PROP_RATING:
    resource->priv->rating = g_value_get_double (value);
    break;
  case PROP_SEASON_NUMBER:
    resource->priv->season_number = g_value_get_uint (value);
    break;
  case PROP_EPISODE_NUMBER:
    resource->priv->episode_number = g_value_get_uint (value);
    break;
  case PROP_ABSOLUTE_NUMBER:
    resource->priv->absolute_number = g_value_get_uint (value);
    break;
  case PROP_SEASON_ID:
    g_clear_pointer (&resource->priv->season_id, g_free);
    resource->priv->season_id = g_value_dup_string (value);
    break;
  case PROP_EPISODE_ID:
    g_clear_pointer (&resource->priv->episode_id, g_free);
    resource->priv->episode_id = g_value_dup_string (value);
    break;
  case PROP_EPISODE_NAME:
    g_clear_pointer (&resource->priv->episode_name, g_free);
    resource->priv->episode_name = g_value_dup_string (value);
    break;
  case PROP_URL_EPISODE_SCREEN:
    g_clear_pointer (&resource->priv->url_episode_screen, g_free);
    resource->priv->url_episode_screen = g_value_dup_string (value);
    break;
  case PROP_DIRECTOR_NAMES:
    g_clear_pointer (&resource->priv->director_names, g_free);
    resource->priv->director_names = g_value_dup_string (value);
    break;
  case PROP_GUEST_STARS_NAMES:
    g_clear_pointer (&resource->priv->guest_stars_names, g_free);
    resource->priv->guest_stars_names = g_value_dup_string (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
episode_resource_class_init (EpisodeResourceClass *klass)
{
  GObjectClass *object_class;
  GomResourceClass *resource_class;

  object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = episode_resource_finalize;
  object_class->get_property = episode_resource_get_property;
  object_class->set_property = episode_resource_set_property;

  resource_class = GOM_RESOURCE_CLASS(klass);
  gom_resource_class_set_table(resource_class, EPISODE_TABLE_NAME);

  specs[PROP_DB_ID] = g_param_spec_int64 (EPISODE_COLUMN_ID,
                                          NULL, NULL,
                                          0, G_MAXINT64,
                                          0, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_DB_ID,
                                   specs[PROP_DB_ID]);
  gom_resource_class_set_primary_key (resource_class, EPISODE_COLUMN_ID);

  specs[PROP_LANGUAGE] = g_param_spec_string (EPISODE_COLUMN_LANGUAGE,
                                              NULL, NULL, NULL,
                                              G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_LANGUAGE,
                                   specs[PROP_LANGUAGE]);

  specs[PROP_SERIES_ID] = g_param_spec_string (EPISODE_COLUMN_SERIES_ID,
                                               NULL, NULL, NULL,
                                               G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_SERIES_ID,
                                   specs[PROP_SERIES_ID]);

  specs[PROP_OVERVIEW] = g_param_spec_string (EPISODE_COLUMN_OVERVIEW,
                                              NULL, NULL, NULL,
                                              G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_OVERVIEW,
                                   specs[PROP_OVERVIEW]);

  specs[PROP_IMDB_ID] = g_param_spec_string (EPISODE_COLUMN_IMDB_ID,
                                             NULL, NULL, NULL,
                                             G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_IMDB_ID,
                                   specs[PROP_IMDB_ID]);

  specs[PROP_FIRST_AIRED] = g_param_spec_string (EPISODE_COLUMN_FIRST_AIRED,
                                                 NULL, NULL, NULL,
                                                 G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_FIRST_AIRED,
                                   specs[PROP_FIRST_AIRED]);

  specs[PROP_RATING] = g_param_spec_double (EPISODE_COLUMN_RATING,
                                            NULL, NULL,
                                            0, G_MAXDOUBLE,
                                            0, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_RATING,
                                   specs[PROP_RATING]);

  specs[PROP_SEASON_NUMBER] = g_param_spec_uint (EPISODE_COLUMN_SEASON_NUMBER,
                                                 NULL, NULL,
                                                 0, G_MAXUINT,
                                                 0, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_SEASON_NUMBER,
                                   specs[PROP_SEASON_NUMBER]);

  specs[PROP_EPISODE_NUMBER] = g_param_spec_uint (EPISODE_COLUMN_EPISODE_NUMBER,
                                                  NULL, NULL,
                                                  0, G_MAXUINT,
                                                  0, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_EPISODE_NUMBER,
                                   specs[PROP_EPISODE_NUMBER]);

  specs[PROP_ABSOLUTE_NUMBER] = g_param_spec_uint (EPISODE_COLUMN_ABSOLUTE_NUMBER,
                                                   NULL, NULL,
                                                   0, G_MAXUINT,
                                                   0, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ABSOLUTE_NUMBER,
                                   specs[PROP_ABSOLUTE_NUMBER]);

  specs[PROP_SEASON_ID] = g_param_spec_string (EPISODE_COLUMN_SEASON_ID,
                                               NULL, NULL, NULL,
                                               G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_SEASON_ID,
                                   specs[PROP_SEASON_ID]);

  specs[PROP_EPISODE_ID] = g_param_spec_string (EPISODE_COLUMN_EPISODE_ID,
                                                NULL, NULL, NULL,
                                                G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_EPISODE_ID,
                                   specs[PROP_EPISODE_ID]);
  gom_resource_class_set_unique (resource_class, EPISODE_COLUMN_EPISODE_ID);

  specs[PROP_EPISODE_NAME] = g_param_spec_string (EPISODE_COLUMN_EPISODE_NAME,
                                                  NULL, NULL, NULL,
                                                  G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_EPISODE_NAME,
                                   specs[PROP_EPISODE_NAME]);

  specs[PROP_URL_EPISODE_SCREEN] = g_param_spec_string (EPISODE_COLUMN_URL_EPISODE_SCREEN,
                                                        NULL, NULL, NULL,
                                                        G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_URL_EPISODE_SCREEN,
                                   specs[PROP_URL_EPISODE_SCREEN]);

  specs[PROP_DIRECTOR_NAMES] = g_param_spec_string (EPISODE_COLUMN_DIRECTOR_NAMES,
                                                    NULL, NULL, NULL,
                                                    G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_DIRECTOR_NAMES,
                                   specs[PROP_DIRECTOR_NAMES]);

  specs[PROP_GUEST_STARS_NAMES] = g_param_spec_string (EPISODE_COLUMN_GUEST_STARS_NAMES,
                                                       NULL, NULL, NULL,
                                                       G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_GUEST_STARS_NAMES,
                                   specs[PROP_GUEST_STARS_NAMES]);
}

static void
episode_resource_init (EpisodeResource *resource)
{
  resource->priv = G_TYPE_INSTANCE_GET_PRIVATE(resource,
                                               EPISODE_TYPE_RESOURCE,
                                               EpisodeResourcePrivate);
}
