/*
 * Copyright (C) 2011 Intel Corporation.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
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

#ifndef _GRL_TRACKER_REQUEST_QUEUE_H_
#define _GRL_TRACKER_REQUEST_QUEUE_H_

#include <grilo.h>
#include <tracker-sparql.h>

/**/

typedef enum {
  GRL_TRACKER_OP_TYPE_QUERY,
  GRL_TRACKER_OP_TYPE_UPDATE,
} GrlTrackerOpType;

typedef struct {
  GrlTrackerOpType         type;
  GAsyncReadyCallback      callback;
  GCancellable            *cancel;
  TrackerSparqlConnection *connection;
  gchar                   *request;
  const GList             *keys;
  gpointer                 data;

  TrackerSparqlCursor *cursor;

  guint operation_id;

  guint skip;
  guint count;
  guint current;
  GrlTypeFilter type_filter;
} GrlTrackerOp;

typedef struct _GrlTrackerQueue GrlTrackerQueue;

/**/

GrlTrackerOp *grl_tracker_op_initiate_query (guint                operation_id,
                                             gchar               *request,
                                             GAsyncReadyCallback  callback,
                                             gpointer             data);

GrlTrackerOp *grl_tracker_op_initiate_metadata (gchar               *request,
                                                GAsyncReadyCallback  callback,
                                                gpointer             data);

GrlTrackerOp *grl_tracker_op_initiate_set_metadata (gchar               *request,
                                                    GAsyncReadyCallback  callback,
                                                    gpointer             data);

/**/

GrlTrackerQueue *grl_tracker_queue_new (void);

void grl_tracker_queue_push (GrlTrackerQueue *queue, GrlTrackerOp *os);

void grl_tracker_queue_cancel (GrlTrackerQueue *queue, GrlTrackerOp *os);

void grl_tracker_queue_done (GrlTrackerQueue *queue, GrlTrackerOp *os);

#endif /* _GRL_TRACKER_REQUEST_QUEUE_H_ */
