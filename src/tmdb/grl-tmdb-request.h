/*
 * Copyright (C) 2012 Canonical Ltd.
 *
 * Author: Jens Georg <jensg@openismus.com>
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

#ifndef _GRL_TMDB_REQUEST_H_
#define _GRL_TMDB_REQUEST_H_

#include <glib-object.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>

#include <net/grl-net.h>

#define GRL_TMDB_REQUEST_TYPE (grl_tmdb_request_get_type())
#define GRL_TMDB_REQUEST(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                               GRL_TMDB_REQUEST_TYPE, \
                               GrlTmdbRequest))
#define GRL_IS_TMDB_REQUEST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                               GRL_TMDB_REQUEST_TYPE))

#define GRL_TMDB_REQUEST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
                            GRL_TMDB_REQUEST_TYPE, \
                            GrlTmdbRequestClass))
#define GRL_IS_TMDB_REQUEST_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                            GRL_TMDB_REQUEST_TYPE))

#define GRL_TMDB_REQUEST_GET_CLASS(klass) \
  (G_TYPE_INSTANCE_GET_CLASS ((klass), \
                              GRL_TMDB_REQUEST_TYPE, \
                              GrlTmdbRequestClass))

typedef struct _GrlTmdbRequest GrlTmdbRequest;
typedef struct _GrlTmdbRequestClass GrlTmdbRequestClass;
typedef struct _GrlTmdbRequestPrivate GrlTmdbRequestPrivate;

struct _GrlTmdbRequest {
  GObject parent;

  GrlTmdbRequestPrivate *priv;
};

struct _GrlTmdbRequestClass {
  GObjectClass parent_class;
};

enum _GrlTmdbRequestDetail {
  GRL_TMDB_REQUEST_DETAIL_MOVIE,
  GRL_TMDB_REQUEST_DETAIL_MOVIE_CAST,
  GRL_TMDB_REQUEST_DETAIL_MOVIE_IMAGES,
  GRL_TMDB_REQUEST_DETAIL_MOVIE_KEYWORDS,
  GRL_TMDB_REQUEST_DETAIL_MOVIE_RELEASE_INFO,
  GRL_TMDB_REQUEST_DETAIL_COUNT
};
typedef enum _GrlTmdbRequestDetail GrlTmdbRequestDetail;

typedef gboolean (*GrlTmdbRequestFilterFunc) (JsonNode *element);
typedef char *(*GrlTmdbRequestStringFilterFunc) (JsonNode *element);

GType grl_tmdb_request_get_type (void);

GrlTmdbRequest *
grl_tmdb_request_new (const char *api_key, const char *uri, GHashTable *args);

GrlTmdbRequest *
grl_tmdb_request_new_search (const char *api_key, const char *needle);

GrlTmdbRequest *
grl_tmdb_request_new_details (const char *api_key,
                              GrlTmdbRequestDetail detail,
                              guint64 id);

GrlTmdbRequest *
grl_tmdb_request_new_details_list (const char *api_key,
                                   GList *details,
                                   guint64 id);

GrlTmdbRequest *
grl_tmdb_request_new_configuration (const char *api_key);

GrlTmdbRequestDetail
grl_tmdb_request_get_detail (GrlTmdbRequest *request);

const char *
grl_tmdb_request_get_uri (GrlTmdbRequest *request);

void
grl_tmdb_request_run_async (GrlTmdbRequest *request,
                            GrlNetWc *wc,
                            GAsyncReadyCallback callback,
                            GCancellable *cancellable,
                            gpointer user_data);

gboolean
grl_tmdb_request_run_finish (GrlTmdbRequest *request,
                             GAsyncResult *result,
                             GError **error);

GValue *
grl_tmdb_request_get (GrlTmdbRequest *request,
                      const char *key);

GList *
grl_tmdb_request_get_string_list (GrlTmdbRequest *request,
                                  const char *path);

GList *
grl_tmdb_request_get_list_with_filter (GrlTmdbRequest *self,
                                       const char *path,
                                       GrlTmdbRequestFilterFunc filter);

GList *
grl_tmdb_request_get_string_list_with_filter (GrlTmdbRequest *self,
                                              const char *path,
                                              GrlTmdbRequestStringFilterFunc filter);

const char *
grl_tmdb_request_detail_to_string (GrlTmdbRequestDetail detail);

#endif /* _GRL_TMDB_REQUEST_H_ */
