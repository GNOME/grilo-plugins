/*
 * Copyright (C) 2010 Igalia S.L.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Joaquim Rocha <jrocha@igalia.com>
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

#ifndef _G_VIMEO_H_
#define _G_VIMEO_H_

#include <glib-object.h>

#define G_VIMEO_TYPE                           \
  (g_vimeo_get_type ())

#define G_VIMEO(obj)                                   \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                   \
                               G_VIMEO_TYPE,           \
                               GVimeo))

#define G_IS_VIMEO(obj)                                \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                   \
                               G_VIMEO_TYPE))

#define G_VIMEO_CLASS(klass)                           \
  (G_TYPE_CHECK_CLASS_CAST((klass),                     \
                           G_VIMEO_TYPE,               \
                           GVimeoClass))

#define G_IS_VIMEO_CLASS(klass)                        \
  (G_TYPE_CHECK_CLASS_TYPE((klass),                     \
                           G_VIMEO_TYPE))

#define G_VIMEO_GET_CLASS(obj)                         \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                    \
                              G_VIMEO_TYPE,            \
                              GVimeoClass))

#define VIMEO_VIDEO "video"
#define VIMEO_VIDEO_ID VIMEO_VIDEO "_id"
#define VIMEO_VIDEO_TITLE "title"
#define VIMEO_VIDEO_DESCRIPTION "description"
#define VIMEO_VIDEO_URL "url"
#define VIMEO_VIDEO_UPLOAD_DATE "upload_date"
#define VIMEO_VIDEO_WIDTH "width"
#define VIMEO_VIDEO_HEIGHT "height"
#define VIMEO_VIDEO_DURATION "duration"
#define VIMEO_VIDEO_OWNER "owner"
#define VIMEO_VIDEO_THUMBNAIL "thumbnail"

#define VIMEO_VIDEO_OWNER_NAME VIMEO_VIDEO_OWNER "_realname"

typedef struct _GVimeo        GVimeo;
typedef struct _GVimeoPrivate GVimeoPrivate;

struct _GVimeo {

  GObject parent;

  /*< private >*/
  GVimeoPrivate *priv;
};

typedef struct _GVimeoClass GVimeoClass;

struct _GVimeoClass {

  GObjectClass parent_class;

};

typedef void (*GVimeoVideoSearchCb) (GVimeo *vimeo,
				     GList *videolist,
				     gpointer user_data);

typedef void (*GVimeoURLCb) (const gchar *url, gpointer user_data);

GType g_vimeo_get_type (void);

GVimeo *g_vimeo_new (const gchar *api_key, const gchar *auth_secret);

void g_vimeo_video_get_play_url (GVimeo *vimeo,
				 gint id,
				 GVimeoURLCb callback,
				 gpointer user_data);

void g_vimeo_set_per_page (GVimeo *vimeo, gint per_page);

void g_vimeo_videos_search (GVimeo *vimeo,
			    const gchar *text,
			    gint page,
			    GVimeoVideoSearchCb callback,
			    gpointer user_data);

#endif /* _G_VIMEO_H_ */
