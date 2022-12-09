/*
 * Copyright (C) 2011-2012 Igalia S.L.
 * Copyright (C) 2011 Intel Corporation.
 * Copyright (C) 2020 Red Hat Inc.
 *
 * Contact: Carlos Garnacho <carlosg@gnome.org>
 *
 * Authors: Carlos Garnacho <carlosg@gnome.org>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gio/gio.h>
#include <libtracker-sparql/tracker-sparql.h>

#include "grl-tracker.h"
#include "grl-tracker-source-priv.h"
#include "grl-tracker-source-statements.h"
#include "grl-tracker-utils.h"

#define MAX_N_CACHED_STATEMENTS 10
#define MINER_FS_BUS_NAME "org.freedesktop.Tracker3.Miner.Files"

typedef struct _CachedStatement CachedStatement;

struct _CachedStatement
{
  GrlTrackerQueryType type;
  GrlOperationOptions *options;
  GList *keys;
  TrackerSparqlStatement *stmt;
  gchar *extra_sparql;
};

static const gchar *query_bases[GRL_TRACKER_QUERY_N_QUERIES] = {
  /* GRL_TRACKER_QUERY_MEDIA_FROM_URI */
  "?urn nie:isStoredAs ~uri ",
  /* GRL_TRACKER_QUERY_RESOLVE */
  "FILTER (STR(?urn) = ~resource) ",
  /* GRL_TRACKER_QUERY_RESOLVE_URI */
  "FILTER (nie:isStoredAs(?urn) = ~uri) ",
  /* GRL_TRACKER_QUERY_ALL */
  "",
  /* GRL_TRACKER_QUERY_FTS_SEARCH */
  "?urn nie:isStoredAs? ?s . ?s fts:match ~match",
};

static void
append_query_variables (GString *str, GList *keys, GrlTypeFilter filter)
{
  GList *l;

  for (l = keys; l; l = l->next) {
    const gchar *var_name;

    if (!grl_tracker_key_get_sparql_statement (GRLPOINTER_TO_KEYID (l->data), filter))
      continue;
    var_name = grl_tracker_key_get_variable_name (GRLPOINTER_TO_KEYID (l->data));
    if (!var_name)
      continue;
    g_string_append_printf (str, "?%s ", var_name);
  }
}

static void
append_subselect_bindings (GString *str, GList *keys, GrlTypeFilter filter)
{
  GList *l;

  for (l = keys; l; l = l->next) {
    const gchar *property_statement, *name;

    property_statement = grl_tracker_key_get_sparql_statement (GRLPOINTER_TO_KEYID (l->data), filter);
    if (!property_statement)
      continue;

    name = grl_tracker_key_get_variable_name (GRLPOINTER_TO_KEYID (l->data));
    g_string_append_printf (str, "(%s AS ?%s) ", property_statement, name);
  }
}

static void
append_filters (GString *str, GrlOperationOptions *options)
{
  GList *filters, *ranges, *l;
  const gchar *name;

  if (!options)
    return;

  filters = grl_operation_options_get_key_filter_list (options);
  ranges = grl_operation_options_get_key_range_filter_list (options);

  if (!filters && !ranges)
    return;

  g_string_append (str, "FILTER (true ");

  for (l = filters; l; l = l->next) {
    name = grl_tracker_key_get_variable_name (GRLPOINTER_TO_KEYID (l->data));
    g_string_append_printf (str, "&& ?%s = ~%s", name, name);
  }

  for (l = ranges; l; l = l->next) {
    GValue *min, *max;

    name = grl_tracker_key_get_variable_name (GRLPOINTER_TO_KEYID (l->data));
    grl_operation_options_get_key_range_filter (options,
                                                GRLPOINTER_TO_KEYID (l->data),
                                                &min, &max);
    if (min)
      g_string_append_printf (str, "&& ?%s >= ~min_%s ", name, name);
    if (max)
      g_string_append_printf (str, "&& ?%s <= ~max_%s ", name, name);
  }

  g_string_append (str, ")");
  g_list_free (filters);
  g_list_free (ranges);
}

static int
key_compare (gconstpointer a,
             gconstpointer b)
{
  GrlKeyID key_a = GRLPOINTER_TO_KEYID (a);
  GrlKeyID key_b = GRLPOINTER_TO_KEYID (b);

  return key_a - key_b;
}

static gboolean
key_equal (gconstpointer a,
           gconstpointer b)
{
  GrlKeyID key_a = GRLPOINTER_TO_KEYID (a);
  GrlKeyID key_b = GRLPOINTER_TO_KEYID (b);

  return key_a == key_b;
}

static GList *
merge_list (GList *target, GList *list)
{
  GList *result = target;
  GList *l;

  for (l = list; l; l = l->next) {
    if (!g_list_find (result, l->data))
      result = g_list_insert_sorted (result, l->data, key_compare);
  }

  return result;
}

static GList *
get_all_keys (GList *keys, GrlOperationOptions *options)
{
  GList *copy, *filters = NULL, *ranges = NULL;

  copy = g_list_copy (keys);

  /* We sort over last modification date, ensure this property is there */
  if (!g_list_find (copy, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_MODIFICATION_DATE))) {
    copy = g_list_insert_sorted (copy, GRLKEYID_TO_POINTER (GRL_METADATA_KEY_MODIFICATION_DATE),
                                 key_compare);
  }

  if (!options)
    return copy;

  filters = grl_operation_options_get_key_filter_list (options);
  ranges = grl_operation_options_get_key_range_filter_list (options);

  copy = merge_list (copy, filters);
  g_list_free (filters);

  copy = merge_list (copy, ranges);
  g_list_free (ranges);

  return copy;
}

static gchar *
create_query_string (GrlTrackerQueryType  type,
                     GrlOperationOptions *options,
                     GList               *keys,
                     const gchar         *extra_sparql)
{
  GrlTypeFilter filter;
  GString *str;
  GList *merged_list;

  if (options)
    filter = grl_operation_options_get_type_filter (options);
  else
    filter = GRL_TYPE_FILTER_ALL;

  str = g_string_new ("SELECT ?mediaType ?urn ");
  append_query_variables (str, keys, filter);
  g_string_append (str, "WHERE { ");

  merged_list = get_all_keys (keys, options);

  /* Remote miner-fs bits */
  g_string_append_printf (str, "SERVICE <dbus:%s> { ",
                          grl_tracker_miner_service ?
                          grl_tracker_miner_service :
                          MINER_FS_BUS_NAME);

  /* Make a subquery so we can apply limit and offset */
  g_string_append (str, "SELECT ?mediaType ?urn ");
  append_query_variables (str, keys, filter);
  g_string_append (str, "{ ");

  if (filter & GRL_TYPE_FILTER_AUDIO) {
    g_string_append_printf (str, "{ GRAPH tracker:Audio { SELECT (%d AS ?mediaType) ?urn ",
                            GRL_MEDIA_TYPE_AUDIO);
    append_subselect_bindings (str, merged_list, GRL_TYPE_FILTER_AUDIO);
    g_string_append_printf (str, "{ ?urn a nfo:Audio . %s } } } ",
                            query_bases[type]);

    if (filter & (GRL_TYPE_FILTER_VIDEO | GRL_TYPE_FILTER_IMAGE))
      g_string_append (str, "UNION ");
  }

  if (filter & GRL_TYPE_FILTER_VIDEO) {
    g_string_append_printf (str, "{ GRAPH tracker:Video { SELECT (%d AS ?mediaType) ?urn ",
                            GRL_MEDIA_TYPE_VIDEO);
    append_subselect_bindings (str, merged_list, GRL_TYPE_FILTER_VIDEO);
    g_string_append_printf (str, "{ ?urn a nfo:Video . %s } } } ",
                            query_bases[type]);

    if (filter & GRL_TYPE_FILTER_IMAGE)
      g_string_append (str, "UNION ");
  }

  if (filter & GRL_TYPE_FILTER_IMAGE) {
    g_string_append_printf (str, "{ GRAPH tracker:Pictures { SELECT (%d AS ?mediaType) ?urn ",
                            GRL_MEDIA_TYPE_IMAGE);
    append_subselect_bindings (str, merged_list, GRL_TYPE_FILTER_IMAGE);
    g_string_append_printf (str, "{ ?urn a nfo:Image . %s } } } ",
                            query_bases[type]);
  }

  append_filters (str, options);

  g_string_append_printf (str,
                          "} LIMIT ~limit OFFSET ~offset"
                          "} %s } "
                          "ORDER BY ?lastModified ",
                          extra_sparql ? extra_sparql : "");

  g_list_free (merged_list);

  return g_string_free (str, FALSE);
}

static gboolean
compare_lists (GList *a, GList *b, GEqualFunc equal)
{
  GList *l1 = a, *l2 = b;

  while (l1 && l2)
    {
      if (!equal (l1->data, l2->data))
        return FALSE;

      l1 = l1->next;
      l2 = l2->next;
    }

  if ((l1 == NULL) != (l2 == NULL))
    return FALSE;

  return TRUE;
}

static gboolean
cached_statement_matches (CachedStatement     *stmt,
                          GrlTrackerQueryType  type,
                          GrlOperationOptions *options,
                          GList               *keys,
                          const gchar         *extra_sparql)
{
  if (stmt->type != type)
    return FALSE;
  if (g_strcmp0 (stmt->extra_sparql, extra_sparql) != 0)
    return FALSE;
  if (!compare_lists (stmt->keys, keys, key_equal))
    return FALSE;
  if ((stmt->options == NULL) != (options == NULL))
    return FALSE;

  if (stmt->options && options) {
    GList *list_a, *list_b;
    gboolean equal;

    if (grl_operation_options_get_type_filter (stmt->options) !=
        grl_operation_options_get_type_filter (options))
      return FALSE;

    list_a = g_list_sort (grl_operation_options_get_key_filter_list (stmt->options), key_compare);
    list_b = g_list_sort (grl_operation_options_get_key_filter_list (options), key_compare);
    equal = compare_lists (list_a, list_b, key_equal);
    g_list_free (list_a);
    g_list_free (list_b);

    if (!equal)
      return FALSE;

    list_a = g_list_sort (grl_operation_options_get_key_range_filter_list (stmt->options), key_compare);
    list_b = g_list_sort (grl_operation_options_get_key_range_filter_list (options), key_compare);
    equal = compare_lists (list_a, list_b, key_equal);
    g_list_free (list_a);
    g_list_free (list_b);

    if (!equal)
      return FALSE;
  }

  return TRUE;
}

GList *
find_cached_statement_link (GrlTrackerSource    *source,
                            GrlTrackerQueryType  type,
                            GrlOperationOptions *options,
                            GList               *keys,
                            const gchar         *extra_sparql)
{
  GList *l;

  for (l = source->priv->cached_statements; l; l = l->next) {
    if (cached_statement_matches (l->data, type, options, keys, extra_sparql))
      return l;
  }

  return NULL;
}

static void
cached_statement_free (CachedStatement *cached)
{
  g_clear_object (&cached->stmt);
  g_clear_object (&cached->options);
  g_list_free (cached->keys);
  g_free (cached->extra_sparql);
  g_free (cached);
}

static void
bind_gvalue (TrackerSparqlStatement *stmt, const gchar *name, GValue *value)
{
  if (G_VALUE_HOLDS_STRING (value))
    tracker_sparql_statement_bind_string (stmt, name, g_value_get_string (value));
  else if (G_VALUE_HOLDS_INT (value))
    tracker_sparql_statement_bind_int (stmt, name, g_value_get_int (value));
  else if (G_VALUE_HOLDS_DOUBLE (value))
    tracker_sparql_statement_bind_double (stmt, name, g_value_get_double (value));
  else if (G_VALUE_HOLDS_FLOAT (value))
    tracker_sparql_statement_bind_double (stmt, name, g_value_get_float (value));
  else if (G_VALUE_HOLDS_BOOLEAN (value))
    tracker_sparql_statement_bind_boolean (stmt, name, g_value_get_boolean (value));
  else if (G_VALUE_HOLDS (value, G_TYPE_DATE_TIME)) {
    GDateTime *time;
    gchar *time_str;

    time = g_value_get_boxed (value);
    time_str = g_date_time_format_iso8601 (time);
    tracker_sparql_statement_bind_string (stmt, name, time_str);
    g_free (time_str);
  }
}

static void
bind_options (TrackerSparqlStatement *stmt,
              GrlOperationOptions    *options)
{
  GList *filters, *ranges, *l;
  const gchar *name;

  if (!options)
    {
      tracker_sparql_statement_bind_int (stmt, "limit", G_MAXUINT);
      tracker_sparql_statement_bind_int (stmt, "offset", 0);
      return;
    }

  tracker_sparql_statement_bind_int (stmt, "limit",
                                     grl_operation_options_get_count (options));
  tracker_sparql_statement_bind_int (stmt, "offset",
                                     grl_operation_options_get_skip (options));

  filters = grl_operation_options_get_key_filter_list (options);
  ranges = grl_operation_options_get_key_range_filter_list (options);

  for (l = filters; l; l = l->next) {
    GValue *value;

    name = grl_tracker_key_get_variable_name (GRLPOINTER_TO_KEYID (l->data));
    value = grl_operation_options_get_key_filter (options, GRLPOINTER_TO_KEYID (l->data));
    bind_gvalue (stmt, name, value);
  }

  for (l = ranges; l; l = l->next) {
    GValue *min, *max;
    gchar *prop;

    name = grl_tracker_key_get_variable_name (GRLPOINTER_TO_KEYID (l->data));
    grl_operation_options_get_key_range_filter (options,
                                                GRLPOINTER_TO_KEYID (l->data),
                                                &min, &max);
    if (min) {
      prop = g_strdup_printf ("min_%s", name);
      bind_gvalue (stmt, prop, min);
      g_free (prop);
    }

    if (max) {
      prop = g_strdup_printf ("max_%s", name);
      bind_gvalue (stmt, prop, max);
      g_free (prop);
    }
  }

  g_list_free (filters);
  g_list_free (ranges);
}

TrackerSparqlStatement *
grl_tracker_source_create_statement (GrlTrackerSource     *source,
                                     GrlTrackerQueryType   type,
                                     GrlOperationOptions  *options,
                                     GList                *keys,
                                     const gchar          *extra_sparql,
                                     GError              **error)
{
  GrlTrackerSourcePrivate *priv = source->priv;
  CachedStatement *cache;
  GError *tracker_error = NULL;
  GList *link, *keys_copy;
  gchar *query_str;

  keys_copy = g_list_sort (g_list_copy (keys), key_compare);
  link = find_cached_statement_link (source, type, options, keys_copy, extra_sparql);

  if (link) {
    cache = link->data;
    priv->cached_statements = g_list_remove_link (priv->cached_statements, link);
    priv->cached_statements = g_list_concat (priv->cached_statements, link);
    g_list_free (keys_copy);
  } else {
    cache = g_new0 (CachedStatement, 1);
    cache->type = type;
    cache->options = options ? grl_operation_options_copy (options) : NULL;
    cache->keys = keys_copy;
    cache->extra_sparql = g_strdup (extra_sparql);

    query_str = create_query_string (type, options, keys, extra_sparql);
    cache->stmt = tracker_sparql_connection_query_statement (priv->tracker_connection,
                                                             query_str,
                                                             NULL,
                                                             &tracker_error);
    g_free (query_str);

    if (!cache->stmt) {
      g_propagate_error (error, tracker_error);
      cached_statement_free (cache);
      return NULL;
    }

    priv->cached_statements = g_list_prepend (priv->cached_statements, cache);

    /* Limit the number of cached statements */
    if (g_list_length (priv->cached_statements) > MAX_N_CACHED_STATEMENTS) {
      CachedStatement *deleted = priv->cached_statements->data;

      priv->cached_statements = g_list_remove (priv->cached_statements, deleted);
      cached_statement_free (deleted);
    }
  }

  bind_options (cache->stmt, options);

  return g_object_ref (cache->stmt);
}
