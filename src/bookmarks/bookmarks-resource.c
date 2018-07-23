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

#include <glib/gi18n.h>

#include "bookmarks-resource.h"

struct _BookmarksResourcePrivate
{
   int            id;
   int            parent;
   BookmarksType  type;
   char          *url;
   char          *title;
   char          *date;
   char          *mime;
   char          *desc;
   char          *thumbnail_url;
};

enum
{
   PROP_0,
   PROP_ID,
   PROP_PARENT,
   PROP_TYPE,
   PROP_URL,
   PROP_TITLE,
   PROP_DATE,
   PROP_MIME,
   PROP_DESC,
   PROP_THUMBNAIL_URL,
   LAST_PROP
};

static GParamSpec *specs[LAST_PROP];

G_DEFINE_TYPE_WITH_PRIVATE (BookmarksResource, bookmarks_resource, GOM_TYPE_RESOURCE)

static void
bookmarks_resource_finalize (GObject *object)
{
   BookmarksResourcePrivate *priv = BOOKMARKS_RESOURCE(object)->priv;

   g_free (priv->url);
   g_free (priv->title);
   g_free (priv->date);
   g_free (priv->mime);
   g_free (priv->desc);
   g_free (priv->thumbnail_url);

   G_OBJECT_CLASS(bookmarks_resource_parent_class)->finalize(object);
}

static void
bookmarks_resource_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
   BookmarksResource *resource = BOOKMARKS_RESOURCE(object);

   switch (prop_id) {
   case PROP_ID:
      g_value_set_int64(value, resource->priv->id);
      break;
   case PROP_PARENT:
      g_value_set_int64(value, resource->priv->parent);
      break;
   case PROP_TYPE:
      g_value_set_enum(value, resource->priv->type);
      break;
   case PROP_URL:
      g_value_set_string(value, resource->priv->url);
      break;
   case PROP_TITLE:
      g_value_set_string(value, resource->priv->title);
      break;
   case PROP_DATE:
      g_value_set_string(value, resource->priv->date);
      break;
   case PROP_MIME:
      g_value_set_string(value, resource->priv->mime);
      break;
   case PROP_DESC:
      g_value_set_string(value, resource->priv->desc);
      break;
   case PROP_THUMBNAIL_URL:
      g_value_set_string(value, resource->priv->thumbnail_url);
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
bookmarks_resource_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
   BookmarksResource *resource = BOOKMARKS_RESOURCE(object);

   switch (prop_id) {
   case PROP_ID:
      resource->priv->id = g_value_get_int64 (value);
      break;
   case PROP_PARENT:
      resource->priv->parent = g_value_get_int64 (value);
      break;
   case PROP_TYPE:
      resource->priv->type = g_value_get_enum (value);
      break;
   case PROP_URL:
      g_free (resource->priv->url);
      resource->priv->url = g_value_dup_string (value);
      break;
   case PROP_TITLE:
      g_free (resource->priv->title);
      resource->priv->title = g_value_dup_string (value);
      break;
   case PROP_DATE:
      g_free (resource->priv->date);
      resource->priv->date = g_value_dup_string (value);
      break;
   case PROP_MIME:
      g_free (resource->priv->mime);
      resource->priv->mime = g_value_dup_string (value);
      break;
   case PROP_DESC:
      g_free (resource->priv->desc);
      resource->priv->desc = g_value_dup_string (value);
      break;
   case PROP_THUMBNAIL_URL:
      g_free (resource->priv->thumbnail_url);
      resource->priv->thumbnail_url = g_value_dup_string (value);
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
bookmarks_resource_class_init (BookmarksResourceClass *klass)
{
   GObjectClass *object_class;
   GomResourceClass *resource_class;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = bookmarks_resource_finalize;
   object_class->get_property = bookmarks_resource_get_property;
   object_class->set_property = bookmarks_resource_set_property;

   resource_class = GOM_RESOURCE_CLASS(klass);
   gom_resource_class_set_table(resource_class, "bookmarks");

   specs[PROP_ID] =
      g_param_spec_int64 ("id",
                          NULL,
			  NULL,
			  0, G_MAXINT64,
			  0, G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_ID,
                                   specs[PROP_ID]);
   gom_resource_class_set_primary_key(resource_class, "id");

   specs[PROP_PARENT] =
      g_param_spec_int64 ("parent",
                          NULL,
			  NULL,
			  0, G_MAXINT64,
			  0, G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_PARENT,
                                   specs[PROP_PARENT]);
   gom_resource_class_set_reference(resource_class, "parent",
                                    NULL, "id");

   specs[PROP_TYPE] =
      g_param_spec_enum ("type",
                         NULL,
			NULL,
			BOOKMARKS_TYPE_TYPE,
			BOOKMARKS_TYPE_STREAM,
			G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_TYPE,
                                   specs[PROP_TYPE]);

   specs[PROP_URL] =
      g_param_spec_string("url",
                          NULL,
                          NULL,
                          NULL,
                          G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_URL,
                                   specs[PROP_URL]);

   specs[PROP_TITLE] =
      g_param_spec_string("title",
                          NULL,
                          NULL,
                          NULL,
                          G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_TITLE,
                                   specs[PROP_TITLE]);

   specs[PROP_DATE] =
      g_param_spec_string("date",
                          NULL,
                          NULL,
                          NULL,
                          G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_DATE,
                                   specs[PROP_DATE]);

   specs[PROP_MIME] =
      g_param_spec_string("mime",
                          NULL,
                          NULL,
                          NULL,
                          G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_MIME,
                                   specs[PROP_MIME]);

   specs[PROP_DESC] =
      g_param_spec_string("desc",
                          NULL,
                          NULL,
                          NULL,
                          G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_DESC,
                                   specs[PROP_DESC]);
   specs[PROP_THUMBNAIL_URL] =
      g_param_spec_string("thumbnail-url",
                          NULL,
                          NULL,
                          NULL,
                          G_PARAM_READWRITE);
   g_object_class_install_property(object_class, PROP_THUMBNAIL_URL,
                                   specs[PROP_THUMBNAIL_URL]);
   gom_resource_class_set_property_new_in_version(resource_class,
                                                  "thumbnail-url", 2);
}

static void
bookmarks_resource_init (BookmarksResource *resource)
{
   resource->priv = bookmarks_resource_get_instance_private (resource);
}

GType
bookmarks_type_get_type (void)
{
   static GType type_id = 0;
   static gsize initialized = FALSE;
   static GEnumValue values[] = {
      { BOOKMARKS_TYPE_CATEGORY, "BOOKMARK_TYPE_CATEGORY", "CATEGORY" },
      { BOOKMARKS_TYPE_STREAM, "BOOKMARK_TYPE_STREAM", "STREAM" }
   };

   if (g_once_init_enter(&initialized)) {
      type_id = g_enum_register_static("BookmarksType", values);
      g_once_init_leave(&initialized, TRUE);
   }

   return type_id;
}
