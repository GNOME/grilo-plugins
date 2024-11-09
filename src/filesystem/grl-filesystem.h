/*
 * Copyright (C) 2010, 2011 Igalia S.L.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
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

#ifndef _GRL_FILESYSTEM_SOURCE_H_
#define _GRL_FILESYSTEM_SOURCE_H_

#include <grilo.h>

#define GRL_FILESYSTEM_SOURCE_TYPE              \
  (grl_filesystem_source_get_type ())

#define GRL_FILESYSTEM_SOURCE(obj)                              \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                           \
                               GRL_FILESYSTEM_SOURCE_TYPE,      \
                               GrlFilesystemSource))

#define GRL_IS_FILESYSTEM_SOURCE(obj)                           \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                           \
                               GRL_FILESYSTEM_SOURCE_TYPE))

#define GRL_FILESYSTEM_SOURCE_CLASS(klass)              \
  (G_TYPE_CHECK_CLASS_CAST((klass),                     \
                           GRL_FILESYSTEM_SOURCE_TYPE,  \
                           GrlFilesystemSourceClass))

#define GRL_IS_FILESYSTEM_SOURCE_CLASS(klass)           \
  (G_TYPE_CHECK_CLASS_TYPE((klass)                      \
                           GRL_FILESYSTEM_SOURCE_TYPE))

#define GRL_FILESYSTEM_SOURCE_GET_CLASS(obj)                    \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                            \
                              GRL_FILESYSTEM_SOURCE_TYPE,       \
                              GrlFilesystemSourceClass))

/* --- Grilo Configuration --- */
#define GRILO_CONF_CHOSEN_URI "base-uri"
#define GRILO_CONF_MAX_SEARCH_DEPTH "maximum-search-depth"
#define GRILO_CONF_HANDLE_PLS "handle-pls"
#define GRILO_CONF_SEPARATE_SRC "separate-src"
#define GRILO_CONF_SOURCE_ID_SUFFIX "source-id-suffix"
#define GRILO_CONF_SOURCE_NAME "source-name"
#define GRILO_CONF_SOURCE_DESC "source-desc"
#define GRILO_CONF_MAX_SEARCH_DEPTH_DEFAULT 6


typedef struct _GrlFilesystemSource GrlFilesystemSource;
typedef struct _GrlFilesystemSourcePrivate GrlFilesystemSourcePrivate;

struct _GrlFilesystemSource {

  GrlSource parent;

  /*< private >*/
  GrlFilesystemSourcePrivate *priv;
};

typedef struct _GrlFilesystemSourceClass GrlFilesystemSourceClass;

struct _GrlFilesystemSourceClass {

  GrlSourceClass parent_class;

};

GType grl_filesystem_source_get_type (void);

#endif /* _GRL_FILESYSTEM_SOURCE_H_ */
