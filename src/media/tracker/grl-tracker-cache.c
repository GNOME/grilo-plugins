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

#include <glib.h>

#include "grl-tracker-cache.h"

/* TODO: is it worth to have an LRU ? */
struct _GrlTrackerItemCache {
  gsize size_limit;
  gsize size_current;

  GHashTable *table;
  GList *list;
};

GrlTrackerItemCache *
grl_tracker_item_cache_new (gsize size)
{
  GrlTrackerItemCache *cache;

  g_return_val_if_fail (size > 0, NULL);

  cache = g_slice_new0 (GrlTrackerItemCache);

  if (!cache)
    return NULL;

  cache->size_limit = size;
  cache->table = g_hash_table_new (g_direct_hash, g_direct_equal);

  return cache;
}

void
grl_tracker_item_cache_free (GrlTrackerItemCache *cache)
{
  g_return_if_fail (cache != NULL);

  g_list_free (cache->list);
  g_hash_table_destroy (cache->table);

  g_slice_free (GrlTrackerItemCache, cache);
}

void
grl_tracker_item_cache_add_item (GrlTrackerItemCache *cache,
				 guint id,
				 GrlTrackerSource *source)
{
  GList *last;

  g_return_if_fail (cache != NULL);

  if (g_hash_table_lookup (cache->table, GSIZE_TO_POINTER (id)) != NULL)
    return;

  if (cache->size_current >= cache->size_limit) {
    last = g_list_last (cache->list); /* TODO: optimize that ! */
    g_hash_table_remove (cache->table, last->data);
    cache->list = g_list_remove_link (cache->list, last);
  }

  cache->list = g_list_prepend (cache->list, GSIZE_TO_POINTER (id));
  g_hash_table_insert (cache->table, GSIZE_TO_POINTER (id), source);
  cache->size_current++;
}

GrlTrackerSource *
grl_tracker_item_cache_get_source (GrlTrackerItemCache *cache, guint id)
{
  g_return_val_if_fail (cache != NULL, NULL);

  return (GrlTrackerSource *) g_hash_table_lookup (cache->table,
						   GSIZE_TO_POINTER (id));
}
