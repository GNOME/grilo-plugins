/*
 * Authors: Marco Piazza <mpiazza@gmail.com>
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

#ifndef _GRL_RAITV_H
#define _GRL_RAITV_H

#include <glib-object.h>
#include <grilo.h>

G_BEGIN_DECLS

#define GRL_TYPE_RAITV_SOURCE grl_raitv_source_get_type()

#define GRL_RAITV_SOURCE(obj)                         \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                 \
                               GRL_TYPE_RAITV_SOURCE, \
                               GrlRaitvSource))

#define GRL_RAITV_SOURCE_CLASS(klass)              \
  (G_TYPE_CHECK_CLASS_CAST ((klass),               \
                            GRL_TYPE_RAITV_SOURCE, \
                            GrlRaitvSourceClass))

#define GRL_IS_RAITV_SOURCE(obj)                         \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                    \
                               GRL_TYPE_RAITV_SOURCE))


#define GRL_IS_RAITV_SOURCE_CLASS(klass)              \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                  \
                            GRL_TYPE_RAITV_SOURCE))

#define GRL_RAITV_SOURCE_GET_CLASS(obj)               \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                  \
                              GRL_TYPE_RAITV_SOURCE,  \
                              GrlRaitvSourceClass))

typedef struct _GrlRaitvSource        GrlRaitvSource;
typedef struct _GrlRaitvSourceClass   GrlRaitvSourceClass;
typedef struct _GrlRaitvSourcePrivate GrlRaitvSourcePrivate;

struct _GrlRaitvSource
{
  GrlSource parent;

  /*< private >*/
  GrlRaitvSourcePrivate *priv;
};

struct _GrlRaitvSourceClass
{
  GrlSourceClass parent_class;
};

GType grl_raitv_source_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* _GRL_RAITV_H */
