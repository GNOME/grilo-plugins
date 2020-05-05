/*
 * Copyright (C) 2011 Intel Corporation.
 * Copyright (C) 2011 Igalia S.L.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
 *          Juan A. Suarez Romero <jasuarez@igalia.com>
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

#include "grl-tracker-source-cache.h"

typedef struct {
  GrlTrackerSource *source;

  GHashTable *id_table;
} GrlTrackerCacheSource;

struct _GrlTrackerCache {
  gsize size_limit;
  gsize size_current;

  GHashTable *id_table;
  GHashTable *source_table;
  GList      *id_list;
};

static GrlTrackerCacheSource *
grl_tracker_cache_source_new (GrlTrackerSource *source)
{
  GrlTrackerCacheSource *csource = g_slice_new0 (GrlTrackerCacheSource);

  csource->source = source;
  csource->id_table = g_hash_table_new (g_direct_hash, g_direct_equal);

  return csource;
}

static void
grl_tracker_cache_source_free (GrlTrackerCacheSource *csource)
{
  g_hash_table_destroy (csource->id_table);

  g_slice_free (GrlTrackerCacheSource, csource);
}

/**/

GrlTrackerCache *
grl_tracker_source_cache_new (gsize size)
{
  GrlTrackerCache *cache;

  g_return_val_if_fail (size > 0, NULL);

  cache = g_slice_new0 (GrlTrackerCache);

  if (!cache)
    return NULL;

  cache->size_limit   = size;
  cache->id_table     = g_hash_table_new (g_direct_hash, g_direct_equal);
  cache->source_table = g_hash_table_new (g_direct_hash, g_direct_equal);

  return cache;
}

void
grl_tracker_source_cache_free (GrlTrackerCache *cache)
{
  GHashTableIter iter;
  gpointer key, value;

  g_return_if_fail (cache != NULL);

  g_hash_table_iter_init (&iter, cache->source_table);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    grl_tracker_source_cache_del_source (cache, key);
  }

  if (cache->id_list) {
    g_warning ("Memleak detected");
    g_list_free (cache->id_list);
  }
  g_hash_table_destroy (cache->id_table);
  g_hash_table_destroy (cache->source_table);

  g_slice_free (GrlTrackerCache, cache);
}

void
grl_tracker_source_cache_add_item (GrlTrackerCache *cache,
                                   guint id,
                                   GrlTrackerSource *source)
{
  GList *lid;
  GrlTrackerCacheSource *csource;

  g_return_if_fail (cache != NULL);

  if (g_hash_table_lookup (cache->id_table, GSIZE_TO_POINTER (id)) != NULL)
    return; /* TODO: is it worth to have an LRU ? */

  csource = g_hash_table_lookup (cache->source_table, source);

  if (!csource) {
    csource = grl_tracker_cache_source_new (source);
    g_hash_table_insert (cache->source_table, source, csource);
  }

  if (cache->size_current >= cache->size_limit) {
    lid = g_list_last (cache->id_list); /* TODO: optimize that ! */
    g_hash_table_remove (cache->id_table, lid->data);
    cache->id_list = g_list_remove_link (cache->id_list, lid);

    lid->data = GSIZE_TO_POINTER (id);
    lid->next = cache->id_list;
    cache->id_list->prev = lid;
    cache->id_list = lid;
  } else {
    cache->id_list = g_list_prepend (cache->id_list, GSIZE_TO_POINTER (id));
    cache->size_current++;
  }

  g_hash_table_insert (cache->id_table, GSIZE_TO_POINTER (id), csource);
  g_hash_table_insert (csource->id_table, GSIZE_TO_POINTER (id),
                       cache->id_list);
}

void
grl_tracker_source_cache_del_source (GrlTrackerCache *cache,
                                     GrlTrackerSource *source)
{
  GrlTrackerCacheSource *csource;
  GHashTableIter iter;
  gpointer key, value;

  g_return_if_fail (cache != NULL);
  g_return_if_fail (source != NULL);

  csource = g_hash_table_lookup (cache->source_table, source);

  if (!csource)
    return;

  g_hash_table_iter_init (&iter, csource->id_table);

  while (g_hash_table_iter_next (&iter, &key, &value)) {
    g_hash_table_remove (cache->id_table, key);
    cache->id_list = g_list_delete_link (cache->id_list, value);
  }

  g_hash_table_remove (cache->source_table, source);
  grl_tracker_cache_source_free (csource);
}

GrlTrackerSource *
grl_tracker_source_cache_get_source (GrlTrackerCache *cache, guint id)
{
  GrlTrackerCacheSource *csource;

  g_return_val_if_fail (cache != NULL, NULL);

  csource = (GrlTrackerCacheSource *) g_hash_table_lookup (cache->id_table,
                                                           GSIZE_TO_POINTER (id));

  if (csource) {
    return csource->source;
  }

  return NULL;
}
