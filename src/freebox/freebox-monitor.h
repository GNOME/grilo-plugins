/*
 * Copyright (C) 2011 Intel Corp
 * Copyright (C) 2013 Bastien Nocera <hadess@hadess.net>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option) any
 * later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this package; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __FREEBOX_MONITOR_H__
#define __FREEBOX_MONITOR_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define FREEBOX_TYPE_MONITOR (freebox_monitor_get_type())
#define FREEBOX_MONITOR(obj)                                                 \
   (G_TYPE_CHECK_INSTANCE_CAST ((obj),                                  \
                                FREEBOX_TYPE_MONITOR,                        \
                                FreeboxMonitor))
#define FREEBOX_MONITOR_CLASS(klass)                                         \
   (G_TYPE_CHECK_CLASS_CAST ((klass),                                   \
                             FREEBOX_TYPE_MONITOR,                           \
                             FreeboxMonitorClass))
#define IS_FREEBOX_MONITOR(obj)                                              \
   (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                                  \
                                FREEBOX_TYPE_MONITOR))
#define IS_FREEBOX_MONITOR_CLASS(klass)                                      \
   (G_TYPE_CHECK_CLASS_TYPE ((klass),                                   \
                             FREEBOX_TYPE_MONITOR))
#define FREEBOX_MONITOR_GET_CLASS(obj)                                       \
   (G_TYPE_INSTANCE_GET_CLASS ((obj),                                   \
                               FREEBOX_TYPE_MONITOR,                         \
                               FreeboxMonitorClass))

typedef struct _FreeboxMonitorPrivate FreeboxMonitorPrivate;
typedef struct _FreeboxMonitor      FreeboxMonitor;
typedef struct _FreeboxMonitorClass FreeboxMonitorClass;

struct _FreeboxMonitor {
  GObject parent;

  FreeboxMonitorPrivate *priv;
};

struct _FreeboxMonitorClass {
  GObjectClass parent_class;
};

GType freebox_monitor_get_type (void) G_GNUC_CONST;

FreeboxMonitor *freebox_monitor_new (void);

G_END_DECLS

#endif /* __FREEBOX_MONITOR_H__ */
