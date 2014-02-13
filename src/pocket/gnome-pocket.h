/*
 *  Copyright © 2013 Bastien Nocera <hadess@hadess.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any pocket version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef GNOME_POCKET_H
#define GNOME_POCKET_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define POCKET_FAVORITE TRUE
#define POCKET_NOT_FAVORITE FALSE

typedef enum {
  POCKET_STATUS_NORMAL    = 0,
  POCKET_STATUS_ARCHIVED  = 1,
  POCKET_STATUS_DELETED   = 2
} PocketItemStatus;

#define POCKET_IS_ARTICLE TRUE
#define POCKET_IS_NOT_ARTICLE FALSE

typedef enum {
  POCKET_HAS_MEDIA_FALSE     = 0,
  POCKET_HAS_MEDIA_INCLUDED  = 1,
  POCKET_IS_MEDIA            = 2
} PocketMediaInclusion;

typedef struct {
  char                  *id;
  char                  *url;
  char                  *title;
  char                  *thumbnail_url;
  gboolean               favorite;
  PocketItemStatus       status;
  gboolean               is_article;
  PocketMediaInclusion   has_image;
  PocketMediaInclusion   has_video;
  gint64                 time_added;
  char                 **tags;
} GnomePocketItem;

#define GNOME_TYPE_POCKET_ITEM (gnome_pocket_item_get_type ())

GType            gnome_pocket_item_get_type    (void) G_GNUC_CONST;
char            *gnome_pocket_item_to_string   (GnomePocketItem *item);
GnomePocketItem *gnome_pocket_item_from_string (const char *str);
void             gnome_pocket_print_item       (GnomePocketItem *item);

#define GNOME_TYPE_POCKET         (gnome_pocket_get_type ())
#define GNOME_POCKET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GNOME_TYPE_POCKET, GnomePocket))
#define GNOME_POCKET_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GNOME_TYPE_POCKET, GnomePocketClass))
#define GNOME_IS_POCKET(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOME_TYPE_POCKET))
#define GNOME_IS_POCKET_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GNOME_TYPE_POCKET))
#define GNOME_POCKET_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GNOME_TYPE_POCKET, GnomePocketClass))

typedef struct _GnomePocketPrivate GnomePocketPrivate;

typedef struct
{
  GObject parent;

  /*< private >*/
  GnomePocketPrivate *priv;
} GnomePocket;

typedef struct
{
  GObjectClass parent;
} GnomePocketClass;

GType              gnome_pocket_get_type        (void);

GnomePocket       *gnome_pocket_new             (void);
void               gnome_pocket_add_url         (GnomePocket         *self,
                                                 const char          *url,
                                                 const char          *tweet_id,
                                                 GCancellable        *cancellable,
                                                 GAsyncReadyCallback  callback,
                                                 gpointer             user_data);
gboolean           gnome_pocket_add_url_finish  (GnomePocket         *self,
                                                 GAsyncResult        *res,
                                                 GError             **error);
void               gnome_pocket_refresh         (GnomePocket         *self,
                                                 GCancellable        *cancellable,
                                                 GAsyncReadyCallback  callback,
                                                 gpointer             user_data);
gboolean           gnome_pocket_refresh_finish  (GnomePocket         *self,
                                                 GAsyncResult        *res,
                                                 GError             **error);
GList             *gnome_pocket_get_items       (GnomePocket         *self);

void               gnome_pocket_load_cached        (GnomePocket         *self,
                                                    GCancellable        *cancellable,
                                                    GAsyncReadyCallback  callback,
                                                    gpointer             user_data);
gboolean           gnome_pocket_load_cached_finish (GnomePocket         *self,
                                                    GAsyncResult        *res,
                                                    GError             **error);

/* Debug functions */
GList             *gnome_pocket_load_from_file  (GnomePocket         *self,
                                                 const char          *filename,
                                                 GError             **error);

G_END_DECLS

#endif /* GNOME_POCKET_H */
