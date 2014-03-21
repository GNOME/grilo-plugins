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

#ifndef BOOKMARKS_RESOURCE_H
#define BOOKMARKS_RESOURCE_H

#include <gom/gom.h>

G_BEGIN_DECLS

#define BOOKMARKS_TYPE_RESOURCE            (bookmarks_resource_get_type())
#define BOOKMARKS_TYPE_TYPE                (bookmarks_type_get_type())
#define BOOKMARKS_RESOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), BOOKMARKS_TYPE_RESOURCE, BookmarksResource))
#define BOOKMARKS_RESOURCE_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), BOOKMARKS_TYPE_RESOURCE, BookmarksResource const))
#define BOOKMARKS_RESOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  BOOKMARKS_TYPE_RESOURCE, BookmarksResourceClass))
#define BOOKMARKS_IS_RESOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BOOKMARKS_TYPE_RESOURCE))
#define BOOKMARKS_IS_RESOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  BOOKMARKS_TYPE_RESOURCE))
#define BOOKMARKS_RESOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  BOOKMARKS_TYPE_RESOURCE, BookmarksResourceClass))
#define BOOKMARKS_RESOURCE_ERROR           (bookmarks_resource_error_quark())

typedef struct _BookmarksResource        BookmarksResource;
typedef struct _BookmarksResourceClass   BookmarksResourceClass;
typedef struct _BookmarksResourcePrivate BookmarksResourcePrivate;
typedef enum   _BookmarksResourceError   BookmarksResourceError;
typedef enum   _BookmarksType            BookmarksType;

enum _BookmarksType
{
   BOOKMARKS_TYPE_CATEGORY = 0,
   BOOKMARKS_TYPE_STREAM
};

struct _BookmarksResource
{
   GomResource parent;

   /*< private >*/
   BookmarksResourcePrivate *priv;
};

struct _BookmarksResourceClass
{
   GomResourceClass parent_class;
};

GType        bookmarks_type_get_type                   (void) G_GNUC_CONST;
GType        bookmarks_resource_get_type               (void) G_GNUC_CONST;

G_END_DECLS

#endif /* BOOKMARKS_RESOURCE_H */
