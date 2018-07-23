/*
 * Copyright (C) 2015 Victor Toso.
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

struct _FuzzySeriesNamesResourcePrivate {
  gint64      db_id;
  gchar      *series_id;
  gchar      *fuzzy_name;
};

enum {
  PROP_0,
  PROP_DB_ID,
  PROP_SERIES_ID,
  PROP_FUZZY_NAME,
  LAST_PROP
};

static GParamSpec *specs[LAST_PROP];

G_DEFINE_TYPE_WITH_PRIVATE (FuzzySeriesNamesResource, fuzzy_series_names_resource, GOM_TYPE_RESOURCE)

static void
fuzzy_series_names_resource_finalize (GObject *object)
{
  FuzzySeriesNamesResourcePrivate *priv = FUZZY_SERIES_NAMES_RESOURCE(object)->priv;

  g_clear_pointer (&priv->series_id, g_free);
  g_clear_pointer (&priv->fuzzy_name, g_free);

  G_OBJECT_CLASS(fuzzy_series_names_resource_parent_class)->finalize(object);
}

static void
fuzzy_series_names_resource_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  FuzzySeriesNamesResource *resource = FUZZY_SERIES_NAMES_RESOURCE(object);

  switch (prop_id) {
  case PROP_DB_ID:
    g_value_set_int64 (value, resource->priv->db_id);
    break;
  case PROP_SERIES_ID:
    g_value_set_string (value, resource->priv->series_id);
    break;
  case PROP_FUZZY_NAME:
    g_value_set_string (value, resource->priv->fuzzy_name);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
fuzzy_series_names_resource_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  FuzzySeriesNamesResource *resource = FUZZY_SERIES_NAMES_RESOURCE(object);

  switch (prop_id) {
  case PROP_DB_ID:
    resource->priv->db_id = g_value_get_int64 (value);
    break;
  case PROP_SERIES_ID:
    g_clear_pointer (&resource->priv->series_id, g_free);
    resource->priv->series_id = g_value_dup_string (value);
    break;
  case PROP_FUZZY_NAME:
    g_clear_pointer (&resource->priv->fuzzy_name, g_free);
    resource->priv->fuzzy_name = g_value_dup_string (value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void
fuzzy_series_names_resource_class_init (FuzzySeriesNamesResourceClass *klass)
{
  GObjectClass *object_class;
  GomResourceClass *resource_class;

  object_class = G_OBJECT_CLASS(klass);
  object_class->finalize = fuzzy_series_names_resource_finalize;
  object_class->get_property = fuzzy_series_names_resource_get_property;
  object_class->set_property = fuzzy_series_names_resource_set_property;

  resource_class = GOM_RESOURCE_CLASS(klass);
  gom_resource_class_set_table(resource_class, FUZZY_SERIES_NAMES_TABLE_NAME);

  specs[PROP_DB_ID] = g_param_spec_int64 (FUZZY_SERIES_NAMES_COLUMN_ID,
                                          NULL, NULL,
                                          0, G_MAXINT64,
                                          0, G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_DB_ID,
                                   specs[PROP_DB_ID]);
  gom_resource_class_set_primary_key (resource_class, FUZZY_SERIES_NAMES_COLUMN_ID);
  gom_resource_class_set_property_new_in_version (resource_class,
                                                  FUZZY_SERIES_NAMES_COLUMN_ID,
                                                  3);

  specs[PROP_FUZZY_NAME] = g_param_spec_string (FUZZY_SERIES_NAMES_COLUMN_FUZZY_NAME,
                                                 NULL, NULL, NULL,
                                                 G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_FUZZY_NAME,
                                   specs[PROP_FUZZY_NAME]);
  gom_resource_class_set_property_new_in_version (resource_class,
                                                  FUZZY_SERIES_NAMES_COLUMN_FUZZY_NAME,
                                                  3);

  specs[PROP_SERIES_ID] = g_param_spec_string (FUZZY_SERIES_NAMES_COLUMN_SERIES_ID,
                                               NULL, NULL, NULL,
                                               G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_SERIES_ID,
                                   specs[PROP_SERIES_ID]);
  gom_resource_class_set_reference (resource_class, FUZZY_SERIES_NAMES_COLUMN_SERIES_ID,
                                    SERIES_TABLE_NAME, SERIES_COLUMN_SERIES_ID);
  gom_resource_class_set_property_new_in_version (resource_class,
                                                  FUZZY_SERIES_NAMES_COLUMN_SERIES_ID,
                                                  3);
}

static void
fuzzy_series_names_resource_init (FuzzySeriesNamesResource *resource)
{
  resource->priv = fuzzy_series_names_resource_get_instance_private (resource);
}
