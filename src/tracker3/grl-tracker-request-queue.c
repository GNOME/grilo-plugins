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

#include "grl-tracker.h"
#include "grl-tracker-request-queue.h"

/**/

struct _GrlTrackerQueue {
  GList      *head;
  GList      *tail;
  GHashTable *operations;
  GHashTable *operations_ids;
};

/**/

static void
grl_tracker_op_terminate (GrlTrackerOp *os)
{
  if (os == NULL)
    return;

  g_clear_object (&os->cursor);
  g_object_unref (os->cancel);
  g_free (os->request);

  g_slice_free (GrlTrackerOp, os);
}

static GrlTrackerOp *
grl_tracker_op_initiate (gchar               *request,
                         GAsyncReadyCallback  callback,
                         gpointer             data)
{
  GrlTrackerOp *os = g_slice_new0 (GrlTrackerOp);

  os->request  = request;
  os->callback = callback;
  os->data     = data;
  os->cancel   = g_cancellable_new ();

  return os;
}

GrlTrackerOp *
grl_tracker_op_initiate_query (guint                operation_id,
                               gchar               *request,
                               GAsyncReadyCallback  callback,
                               gpointer             data)
{
  GrlTrackerOp *os = grl_tracker_op_initiate (request,
                                              callback,
                                              data);

  os->type         = GRL_TRACKER_OP_TYPE_QUERY;
  os->operation_id = operation_id;

  /* g_hash_table_insert (grl_tracker_operations, */
  /*                      GSIZE_TO_POINTER (operation_id), os); */

  return os;
}

GrlTrackerOp *
grl_tracker_op_initiate_metadata (gchar               *request,
                                  GAsyncReadyCallback  callback,
                                  gpointer             data)
{
  GrlTrackerOp *os = grl_tracker_op_initiate (request,
                                              callback,
                                              data);

  os->type = GRL_TRACKER_OP_TYPE_QUERY;

  return os;
}

GrlTrackerOp *
grl_tracker_op_initiate_set_metadata (gchar               *request,
                                      GAsyncReadyCallback  callback,
                                      gpointer             data)
{
  GrlTrackerOp *os = grl_tracker_op_initiate (request,
                                              callback,
                                              data);

  os->type = GRL_TRACKER_OP_TYPE_UPDATE;

  return os;
}

static void
grl_tracker_op_start (GrlTrackerOp *os)
{
  switch (os->type) {
  case GRL_TRACKER_OP_TYPE_QUERY:
    tracker_sparql_connection_query_async (grl_tracker_connection,
                                           os->request,
                                           NULL,
                                           os->callback,
                                           os);
    break;

  case GRL_TRACKER_OP_TYPE_UPDATE:
    tracker_sparql_connection_update_async (grl_tracker_connection,
                                            os->request,
                                            G_PRIORITY_DEFAULT,
                                            NULL,
                                            os->callback,
                                            os);
    break;

  default:
    g_assert_not_reached();
    break;
  }
}

/**/

GrlTrackerQueue *
grl_tracker_queue_new (void)
{
  GrlTrackerQueue *queue = g_new0 (GrlTrackerQueue, 1);

  queue->operations     = g_hash_table_new (g_direct_hash, g_direct_equal);
  queue->operations_ids = g_hash_table_new (g_direct_hash, g_direct_equal);

  return queue;
}

void
grl_tracker_queue_push (GrlTrackerQueue *queue,
                        GrlTrackerOp    *os)
{
  gboolean first = FALSE;

  queue->tail = g_list_append (queue->tail, os);
  if (queue->tail->next)
    queue->tail = queue->tail->next;
  else {
    queue->head = queue->tail;
    first = TRUE;
  }

  g_assert (queue->tail->next == NULL);

  g_hash_table_insert (queue->operations, os, queue->tail);
  if (os->operation_id != 0)
    g_hash_table_insert (queue->operations_ids,
                         GSIZE_TO_POINTER (os->operation_id), os);

  if (first)
    grl_tracker_op_start (os);
}

void
grl_tracker_queue_cancel (GrlTrackerQueue *queue,
                          GrlTrackerOp    *os)
{
  GList *item = g_hash_table_lookup (queue->operations, os);

  if (!item)
    return;

  g_cancellable_cancel (os->cancel);

  g_hash_table_remove (queue->operations, os);
  if (os->operation_id != 0)
    g_hash_table_remove (queue->operations_ids,
                         GSIZE_TO_POINTER (os->operation_id));

  if (item == queue->head) {
    queue->head = queue->head->next;
  }
  if (item == queue->tail) {
    queue->tail = queue->tail->prev;
  }

  if (item->prev)
    item->prev->next = item->next;
  if (item->next)
    item->next->prev = item->prev;

  item->next = NULL;
  item->prev = NULL;
  g_list_free (item);
}

void
grl_tracker_queue_done (GrlTrackerQueue *queue,
                        GrlTrackerOp    *os)
{
  GrlTrackerOp *next_os;

  grl_tracker_queue_cancel (queue, os);
  grl_tracker_op_terminate (os);

  if (!queue->head)
    return;

  next_os = queue->head->data;

  grl_tracker_op_start (next_os);
}
