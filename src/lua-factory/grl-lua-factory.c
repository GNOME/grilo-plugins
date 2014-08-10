/*
 * Copyright (C) 2013 Victor Toso.
 *
 * Contact: Victor Toso <me@victortoso.com>
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

#include "grl-lua-common.h"
#include "grl-lua-factory.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define GRL_LUA_FACTORY_SOURCE_GET_PRIVATE(object)           \
  (G_TYPE_INSTANCE_GET_PRIVATE((object),                     \
                               GRL_LUA_FACTORY_SOURCE_TYPE,  \
                               GrlLuaFactorySourcePrivate))

/* --------- Logging  -------- */

#define GRL_LOG_DOMAIN_DEFAULT lua_factory_log_domain
GRL_LOG_DOMAIN_STATIC (lua_factory_log_domain);

#define ENV_LUA_SOURCES_PATH  "GRL_LUA_SOURCES_PATH"
#define LUA_FACTORY_PLUGIN_ID "grl-lua-factory"

/* --- Main table --- */
#define LUA_SOURCE_TABLE            "source"
#define LUA_SOURCE_ID               "id"
#define LUA_SOURCE_NAME             "name"
#define LUA_SOURCE_DESCRIPTION      "description"
#define LUA_SOURCE_SUPPORTED_MEDIA  "supported_media"
#define LUA_SOURCE_ICON             "icon"
#define LUA_SOURCE_AUTO_SPLIT_THRESHOLD "auto_split_threshold"
#define LUA_SOURCE_TAGS             "tags"
#define LUA_SOURCE_CONFIG_KEYS      "config_keys"
#define LUA_SOURCE_SUPPORTED_KEYS   "supported_keys"
#define LUA_SOURCE_SLOW_KEYS        "slow_keys"
#define LUA_SOURCE_RESOLVE_KEYS     "resolve_keys"
#define LUA_SOURCE_MODULES_DEPS     "dependencies"
#define LUA_REQUIRED_TABLE          "required"
#define LUA_OPTIONAL_TABLE          "optional"

static const char *LUA_SOURCE_OPERATION[LUA_NUM_OPERATIONS] = {
  [LUA_SEARCH] = "grl_source_search",
  [LUA_BROWSE] = "grl_source_browse",
  [LUA_QUERY] = "grl_source_query",
  [LUA_RESOLVE] = "grl_source_resolve",
  [LUA_SOURCE_INIT] = "grl_source_init"
};

struct _GrlLuaFactorySourcePrivate {
  lua_State *l_st;
  gboolean fn[LUA_NUM_OPERATIONS];
  GList *supported_keys;
  GList *slow_keys;
  GList *resolve_keys;
  GrlMediaType resolve_type;
  GHashTable *config_keys;
  GrlConfig *configs;
};

static GList *get_lua_sources (void);

static GrlLuaFactorySource *grl_lua_factory_source_new (gchar *lua_plugin_path,
                                                        GList *configs);

static gint lua_plugin_source_info (lua_State *L,
                                    gchar **source_id,
                                    gchar **source_name,
                                    gchar **source_desc,
                                    GrlMediaType *source_supported_media,
                                    GIcon **source_icon,
                                    guint *auto_split_threshold,
                                    gchar ***source_tags);

static gint lua_plugin_source_operations (lua_State *L,
                                          gboolean fn[LUA_NUM_OPERATIONS]);

static gint lua_plugin_source_all_dependencies (lua_State *L);

static gint lua_plugin_source_all_keys (lua_State *L,
                                        const gchar *source_id,
                                        GList **supported_keys,
                                        GList **slow_keys,
                                        GList **resolve_keys,
                                        GrlMediaType *resolve_type,
                                        GHashTable **config_keys);

static void grl_lua_factory_source_finalize (GObject *object);

static const GList *grl_lua_factory_source_supported_keys (GrlSource *source);

static const GList *grl_lua_factory_source_slow_keys(GrlSource *source);

static void grl_lua_factory_source_search (GrlSource *source,
                                           GrlSourceSearchSpec *ss);

static void grl_lua_factory_source_browse (GrlSource *source,
                                           GrlSourceBrowseSpec *bs);

static void grl_lua_factory_source_query (GrlSource *source,
                                          GrlSourceQuerySpec *qs);

static void grl_lua_factory_source_resolve (GrlSource *source,
                                            GrlSourceResolveSpec *rs);

static gboolean grl_lua_factory_source_may_resolve (GrlSource *source,
                                                    GrlMedia *media,
                                                    GrlKeyID key_id,
                                                    GList **missing_keys);

static GrlSupportedOps
grl_lua_factory_source_supported_operations (GrlSource *source);

static GrlConfig *merge_all_configs (const gchar *source_id,
                                     GHashTable *source_configs,
                                     GList *available_configs);

static gboolean all_mandatory_options_has_value (GHashTable *source_configs,
                                                 GrlConfig *merged_configs);

static gboolean lua_plugin_source_init (GrlLuaFactorySource *lua_source);

/* ================== Lua-Factory Plugin  ================================== */

static gboolean
grl_lua_factory_plugin_init (GrlRegistry *registry,
                             GrlPlugin *plugin,
                             GList *configs)
{
  GList *it = NULL;
  GList *lua_sources = NULL;
  GError *err = NULL;
  gboolean source_loaded = FALSE;

  GRL_LOG_DOMAIN_INIT (lua_factory_log_domain, "lua-factory");

  GRL_DEBUG ("grl_lua_factory_plugin_init");

  lua_sources = get_lua_sources ();
  if (!lua_sources)
    return TRUE;

  for (it = lua_sources; it; it = g_list_next (it)) {
    GrlLuaFactorySource *source;

    source = grl_lua_factory_source_new (it->data, configs);
    if (source == NULL) {
      GRL_DEBUG ("Fail to initialize.");
      continue;
    }

    /* In case the plugin gets unref'ed during registration */
    g_object_add_weak_pointer (G_OBJECT (source), (gpointer *) &source);

    if (!grl_registry_register_source (registry, plugin,
                                       GRL_SOURCE (source), &err)) {
      GRL_DEBUG ("Fail to register source: %s", err->message);
      g_error_free (err);
      continue;
    }

    if (!source)
      continue;

    g_object_remove_weak_pointer (G_OBJECT (source), (gpointer *) &source);
    source_loaded = TRUE;
    GRL_DEBUG ("Successfully initialized: %s",
               grl_source_get_id (GRL_SOURCE (source)));
  }
  g_list_free_full (lua_sources, g_free);
  return source_loaded;
}

GRL_PLUGIN_REGISTER (grl_lua_factory_plugin_init, NULL, LUA_FACTORY_PLUGIN_ID);

/* ================== Lua-Factory GObject ================================== */

static GrlLuaFactorySource *
grl_lua_factory_source_new (gchar *lua_plugin_path,
                            GList *configs)
{
  GrlLuaFactorySource *source = NULL;
  lua_State *L = NULL;
  GHashTable *config_keys = NULL;
  gchar *source_id = NULL;
  gchar *source_name = NULL;
  gchar *source_desc = NULL;
  GIcon *source_icon = NULL;
  GrlMediaType source_supported_media = GRL_MEDIA_TYPE_ALL;
  guint auto_split_threshold;
  gchar **source_tags;
  gint ret = 0;

  GRL_DEBUG ("grl_lua_factory_source_new");

  L = luaL_newstate ();
  if (L == NULL) {
    GRL_WARNING ("Unable to create new lua state.");
    return NULL;
  }

  GRL_DEBUG ("Loading '%s'", lua_plugin_path);

  /* Standard Lua libraries */
  luaL_openlibs (L);

  /* Grilo library */
  luaL_requiref (L, GRILO_LUA_LIBRARY_NAME, &luaopen_grilo, TRUE);
  lua_pop (L, 1);

  /* Load the plugin */
  ret = luaL_loadfile (L, lua_plugin_path);
  if (ret != LUA_OK) {
    GRL_WARNING ("[%s] failed to load: %s", lua_plugin_path, lua_tostring (L, -1));
    goto bail;
  }

  ret = lua_pcall (L, 0, 0, 0);
  if (ret != LUA_OK) {
    GRL_WARNING ("[%s] failed to run: %s", lua_plugin_path, lua_tostring (L, -1));
    goto bail;
  }

  ret = lua_plugin_source_info (L, &source_id, &source_name, &source_desc,
                                &source_supported_media, &source_icon,
                                &auto_split_threshold, &source_tags);
  if (ret != LUA_OK)
    goto bail;

  GRL_DEBUG ("source_info ok! source_id: '%s'", source_id);

  source = g_object_new (GRL_LUA_FACTORY_SOURCE_TYPE,
                         "source-id", source_id,
                         "source-name", source_name,
                         "source-desc", source_desc,
                         "supported-media", source_supported_media,
                         "source-icon", source_icon,
                         "auto-split-threshold", auto_split_threshold,
                         "source-tags", source_tags,
                         NULL);
  g_free (source_name);
  g_free (source_desc);
  g_clear_pointer (&source_tags, g_strfreev);
  g_clear_object (&source_icon);

  ret = lua_plugin_source_operations (L, source->priv->fn);
  if (ret != LUA_OK)
    goto bail;

  ret = lua_plugin_source_all_dependencies (L);
  if (ret != LUA_OK)
    goto bail;

  ret = lua_plugin_source_all_keys (L,
                                    source_id,
                                    &source->priv->supported_keys,
                                    &source->priv->slow_keys,
                                    &source->priv->resolve_keys,
                                    &source->priv->resolve_type,
                                    &config_keys);
  if (ret != LUA_OK)
    goto bail;

  source->priv->configs = merge_all_configs (source_id, config_keys, configs);
  if (!all_mandatory_options_has_value (config_keys, source->priv->configs))
    goto bail;

  g_free (source_id);
  source->priv->config_keys = config_keys;
  source->priv->l_st = L;

  if (lua_plugin_source_init (source) == FALSE) {
    g_clear_object (&source);
  }

  return source;

bail:
  if (source != NULL) {
    g_clear_pointer (&config_keys, g_hash_table_unref);
    g_clear_object (&source->priv->configs);
    g_list_free (source->priv->resolve_keys);
    g_list_free (source->priv->supported_keys);
    g_list_free (source->priv->slow_keys);
  }

  g_free (source_id);
  lua_close (L);
  return NULL;
}

static void
grl_lua_factory_source_class_init (GrlLuaFactorySourceClass *klass)
{
  GObjectClass *g_class = G_OBJECT_CLASS (klass);
  GrlSourceClass *source_class = GRL_SOURCE_CLASS (klass);

  g_class->finalize = grl_lua_factory_source_finalize;

  source_class->supported_keys = grl_lua_factory_source_supported_keys;
  source_class->slow_keys= grl_lua_factory_source_slow_keys;
  source_class->supported_operations = grl_lua_factory_source_supported_operations;
  source_class->search = grl_lua_factory_source_search;
  source_class->browse = grl_lua_factory_source_browse;
  source_class->query = grl_lua_factory_source_query;
  source_class->resolve = grl_lua_factory_source_resolve;
  source_class->may_resolve = grl_lua_factory_source_may_resolve;

  g_type_class_add_private (klass, sizeof (GrlLuaFactorySourcePrivate));
}

G_DEFINE_TYPE (GrlLuaFactorySource, grl_lua_factory_source, GRL_TYPE_SOURCE);

static void
grl_lua_factory_source_init (GrlLuaFactorySource *source)
{
  source->priv = GRL_LUA_FACTORY_SOURCE_GET_PRIVATE (source);
}

static void
grl_lua_factory_source_finalize (GObject *object)
{
  GrlLuaFactorySource *source = GRL_LUA_FACTORY_SOURCE (object);

  g_clear_object (&source->priv->configs);
  g_clear_pointer (&source->priv->config_keys, g_hash_table_unref);

  g_list_free (source->priv->resolve_keys);
  g_list_free (source->priv->supported_keys);
  g_list_free (source->priv->slow_keys);
  lua_close (source->priv->l_st);

  G_OBJECT_CLASS (grl_lua_factory_source_parent_class)->finalize (object);
}

/* lua_plugin_source_init
 *
 * Calls a init function with current config_keys to check if the source is
 * ready to operate.
 */
static gboolean
lua_plugin_source_init (GrlLuaFactorySource *lua_source)
{
  lua_State *L = lua_source->priv->l_st;
  GList *it_keys = NULL;
  GList *list_keys = NULL;
  const gchar *key = NULL;
  const gchar *value = NULL;
  gboolean ret = FALSE;

  /* Source does not have grl_source_init() */
  if (lua_source->priv->fn[LUA_SOURCE_INIT] == FALSE) {
    GRL_DEBUG ("grl_source_init not implemented");
    return TRUE;
  }

  lua_getglobal (L, LUA_SOURCE_OPERATION[LUA_SOURCE_INIT]);

  if (lua_source->priv->config_keys != NULL) {
    /* configs */
    lua_newtable (L);

    list_keys = g_hash_table_get_keys (lua_source->priv->config_keys);
    for (it_keys = list_keys; it_keys; it_keys = g_list_next (it_keys)) {
      key = it_keys->data;
      value = grl_config_get_string (lua_source->priv->configs, key);

      if (value) {
        lua_pushstring (L, key);
        lua_pushstring (L, value);
        lua_settable (L, -3);
      }
    }
    g_list_free (list_keys);

  } else {
    lua_pushnil (L);
  }

  if (lua_pcall (L, 1, 1, 0)) {
    GRL_WARNING ("calling grl_source_init function failed: %s",
                 lua_tolstring (L, -1, NULL));
    lua_pop (L, 1);
    return FALSE;
  }

  ret = (lua_isboolean (L, -1) && lua_toboolean (L, -1)) ? TRUE : FALSE;
  lua_pop (L, 1);
  return ret;
}

/* ======================= Utilities ======================================= */

static GList *
table_array_to_list (lua_State *L,
                     const gchar *array_name)
{
  GList *list = NULL;
  gint i = 0;
  gint array_len = 0;

  lua_pushstring (L, array_name);
  lua_gettable (L, -2);

  if (lua_istable (L, -1)) {
    array_len = luaL_len (L, -1);

    for (i = 0; i < array_len; i++) {
      lua_pushinteger (L, i + 1);
      lua_gettable (L, -2);
      if (lua_isstring (L, -1)) {
        list = g_list_prepend (list, g_strdup (lua_tostring (L, -1)));
      }
      lua_pop (L, 1);
    }
  }
  lua_pop (L, 1);

  return g_list_reverse (list);
}

static GList *
keys_table_array_to_list (lua_State *L,
                          const gchar *array_name,
                          GrlRegistry *registry,
                          const gchar *source_id)
{
  GList *list, *filtered_list, *l;

  list = table_array_to_list (L, array_name);

  if (list == NULL)
    return NULL;

  filtered_list = NULL;

  for (l = list; l != NULL; l = l->next) {
    const gchar *key_name = l->data;
    GrlKeyID key_id;

    key_id = grl_registry_lookup_metadata_key (registry, key_name);

    if (key_id != GRL_METADATA_KEY_INVALID) {
      filtered_list = g_list_prepend (filtered_list, GRLKEYID_TO_POINTER (key_id));
    } else {
      GRL_WARNING ("Unknown key '%s' in property '%s' for source '%s'", key_name, array_name, source_id);
    }
  }
  g_list_free_full (list, g_free);

  return g_list_reverse (filtered_list);
}

static gboolean
lua_module_exists (const gchar *lua_module)
{
  gboolean exists = TRUE;
  lua_State *L;

  L = luaL_newstate ();
  if (L == NULL) {
    GRL_WARNING ("Unable to create new lua state.");
    return FALSE;
  }
  luaL_openlibs (L);

  lua_getglobal (L, "require");
  lua_pushstring (L, lua_module);
  if (lua_pcall (L, 1, 0, 0) != LUA_OK) {
    GRL_DEBUG ("%s", lua_tolstring (L, -1, NULL));
    exists = FALSE;
    lua_pop (L, 1);
  }

  lua_close (L);
  return exists;
}

static GList *
get_lua_sources (void)
{
  GList *it_path = NULL;
  GList *lua_sources = NULL;
  GList *l_locations = NULL;
  GDir *dir = NULL;
  gint i = 0;
  const gchar *envvar = NULL;
  const gchar *it_file = NULL;
  GHashTable *ht;

  GRL_DEBUG ("get_lua_sources");

  envvar = g_getenv (ENV_LUA_SOURCES_PATH);

  if (envvar != NULL) {
    gchar **local_dirs;

    /* Environment-only plugins */
    GRL_DEBUG ("'%s' is set - Getting lua-sources only from there.", ENV_LUA_SOURCES_PATH);
    local_dirs = g_strsplit (envvar, G_SEARCHPATH_SEPARATOR_S, -1);
    if (local_dirs) {
      while (local_dirs[i] != NULL) {
        l_locations = g_list_prepend (l_locations, g_strdup (local_dirs[i]));
        i++;
      }
      g_strfreev (local_dirs);
    }
  } else {
    const gchar *const *system_dirs = NULL;

    /* System locations */
    for (system_dirs = g_get_system_data_dirs ();
         *system_dirs != NULL;
         system_dirs++) {

      l_locations = g_list_prepend (l_locations,
                                    g_build_filename (*system_dirs,
                                                      LUA_FACTORY_SOURCE_LOCATION,
                                                      NULL));
    }
    l_locations = g_list_reverse (l_locations);

    /* User locations */
    l_locations = g_list_prepend (l_locations,
                                  g_build_filename (g_get_user_data_dir (),
                                                    LUA_FACTORY_SOURCE_LOCATION,
                                                    NULL));
  }

  ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  for (it_path = l_locations; it_path; it_path = it_path->next) {
    dir = g_dir_open (it_path->data, 0, NULL);
    if (dir == NULL)
      continue;

    for (it_file = g_dir_read_name (dir);
         it_file;
         it_file = g_dir_read_name (dir)) {
      if (g_str_has_suffix (it_file, ".lua")) {
        if (g_hash_table_lookup (ht, it_file) == NULL) {
          lua_sources = g_list_prepend (lua_sources,
                                        g_build_filename (it_path->data,
                                                          it_file, NULL));
          g_hash_table_insert (ht, g_strdup (it_file), GINT_TO_POINTER(TRUE));
        }
      }
    }
    g_dir_close (dir);
  }

  g_hash_table_destroy (ht);
  g_list_free_full (l_locations, g_free);
  return g_list_reverse (lua_sources);
}

/* all_mandatory_options_has_value
 *
 * If source_configs has mandatory options, check in our merged_configs
 * if those mandatory options are settled.
 * If any mandatory option is not settled, return FALSE.
 */
static gboolean
all_mandatory_options_has_value (GHashTable *source_configs,
                                 GrlConfig *merged_configs)
{
  const gchar *key = NULL;
  const gchar *is_mandatory = NULL;
  GList *list_keys = NULL;
  GList *it_keys = NULL;

  list_keys = (source_configs != NULL) ?
              g_hash_table_get_keys (source_configs) : NULL;
  for (it_keys = list_keys; it_keys; it_keys = g_list_next (it_keys)) {
    key = it_keys->data;
    is_mandatory = g_hash_table_lookup (source_configs, key);

    if (g_strcmp0 (is_mandatory, "true") == 0
        && grl_config_get_string (merged_configs, key) == NULL) {

      g_list_free (list_keys);
      return FALSE;
    }
  }
  g_list_free (list_keys);
  return TRUE;
}

/* Insert in @into all the @options in @from not already present */
static void
merge_config (GList *options,
              GrlConfig *into,
              GrlConfig *from)
{
  gchar *value = NULL;

  if (from == NULL)
    return;

  while (options) {
    if (!grl_config_has_param (into, options->data) &&
        grl_config_has_param (from, options->data)) {
      value = grl_config_get_string (from, options->data);
      grl_config_set_string (into, options->data, value);
      g_free (value);
    }
    options = g_list_next (options);
  }
}

/* Create a config with default values from source and values from application.
 * The @source_configs hash table has as keys, the config keys of @source_id
 * source. This hash table may have keys with default values or only boolean
 * values to point mandatory and optional config keys. */
static GrlConfig *
merge_all_configs (const gchar *source_id,
                   GHashTable *source_configs,
                   GList *available_configs)
{
  GList *list_all = NULL;
  GList *list_generic = NULL;
  GList *list_specific = NULL;
  GList *it_config = NULL;
  GList *options = NULL;
  GrlConfig *merged_config = NULL;

  /* From available configs get specific and generic options */
  while (available_configs) {
    gchar *config_source_id;

    config_source_id = grl_config_get_source (available_configs->data);

    if (config_source_id == NULL) {
      list_generic = g_list_prepend (list_generic, available_configs->data);
    } else if (g_strcmp0 (config_source_id, source_id) == 0) {
      list_specific = g_list_prepend (list_specific, available_configs->data);
    }

    g_free (config_source_id);
    available_configs = g_list_next (available_configs);
  }

  list_all = g_list_concat (g_list_reverse (list_specific),
                            g_list_reverse (list_generic));

  /* From source configs get default configs */
  if (source_configs != NULL) {
    GList *list_keys = NULL;
    GList *it_keys = NULL;

    list_keys = g_hash_table_get_keys (source_configs);
    for (it_keys = list_keys; it_keys; it_keys = g_list_next (it_keys)) {
      options = g_list_append (options, g_strdup (it_keys->data));
    }
  }

  /* Now merge them */
  merged_config = grl_config_new (LUA_FACTORY_PLUGIN_ID, source_id);
  for (it_config = list_all; it_config; it_config = g_list_next (it_config)) {
    merge_config (options, merged_config, it_config->data);
  }
  g_list_free (list_all);

  if (options)
    g_list_free_full (options, g_free);

  return merged_config;
}

static gchar **
table_to_tags (lua_State *L)
{
  GPtrArray *array;
  gint i = 0;
  gint array_len = 0;

  if (!lua_istable (L, -1))
    return NULL;

  array = g_ptr_array_new_with_free_func (g_free);

  array_len = luaL_len (L, -1);

  for (i = 0; i < array_len; i++) {
    lua_pushinteger (L, i + 1);
    lua_gettable (L, -2);
    if (lua_isstring (L, -1)) {
      g_ptr_array_add (array, g_strdup (lua_tostring (L, -1)));
    }
    lua_pop (L, 1);
  }

  lua_pop (L, 1);

  if (array->len == 0) {
    g_ptr_array_free (array, TRUE);
    return NULL;
  }

  g_ptr_array_add (array, NULL);

  return (gchar **) g_ptr_array_free (array, FALSE);
}

/* Get from the main table of the plugin
 * the mandatory information to create a source. */
static gint
lua_plugin_source_info (lua_State *L,
                        gchar **source_id,
                        gchar **source_name,
                        gchar **source_desc,
                        GrlMediaType *source_supported_media,
                        GIcon **source_icon,
                        guint *auto_split_threshold,
                        gchar ***source_tags)
{
  const char *lua_source_id = NULL;
  const char *lua_source_name = NULL;
  const char *lua_source_desc = NULL;
  const char *lua_source_icon = NULL;
  const char *lua_source_media = NULL;
  guint lua_auto_split_threshold;
  gchar **lua_source_tags = NULL;

  GRL_DEBUG ("lua_plugin_source_info");

  lua_getglobal (L, LUA_SOURCE_TABLE);
  if (!lua_istable (L, -1)) {
    GRL_DEBUG ("'%s' %s", LUA_SOURCE_TABLE, "table is not defined");
    return !LUA_OK;
  }

  /* Source ID */
  lua_getfield (L, -1, LUA_SOURCE_ID);
  lua_source_id = lua_tolstring (L, -1, NULL);

  /* Source Name */
  lua_getfield (L, -2, LUA_SOURCE_NAME);
  lua_source_name = lua_tolstring (L, -1, NULL);

  /* Source Description */
  lua_getfield (L, -3, LUA_SOURCE_DESCRIPTION);
  lua_source_desc = lua_tolstring (L, -1, NULL);

  /* Source Supported Media */
  lua_getfield (L, -4, LUA_SOURCE_SUPPORTED_MEDIA);
  lua_source_media = lua_tolstring (L, -1, NULL);

  /* Source Icon */
  lua_getfield (L, -5, LUA_SOURCE_ICON);
  lua_source_icon = lua_tolstring (L, -1, NULL);

  /* Auto-split-threshold */
  lua_getfield (L, -6, LUA_SOURCE_AUTO_SPLIT_THRESHOLD);
  lua_auto_split_threshold = lua_tonumber (L, -1);

  /* Source Tags */
  lua_getfield (L, -7, LUA_SOURCE_TAGS);
  lua_source_tags = table_to_tags (L);

  /* Remove source info and main table from stack */
  lua_pop (L, 8);

  if (lua_source_id == NULL
      || lua_source_name == NULL) {
    GRL_DEBUG ("Lua source info is not well defined.");
    g_clear_pointer (&lua_source_tags, g_strfreev);
    return !LUA_OK;
  }

  *source_id = g_strdup (lua_source_id);
  *source_name = g_strdup (lua_source_name);
  *source_desc = g_strdup (lua_source_desc);

  if (lua_source_media != NULL) {
    if (g_strcmp0 (lua_source_media, "audio") == 0)
      *source_supported_media = GRL_MEDIA_TYPE_AUDIO;
    else if (g_strcmp0 (lua_source_media, "video") == 0)
      *source_supported_media = GRL_MEDIA_TYPE_VIDEO;
    else if (g_strcmp0 (lua_source_media, "image") == 0)
      *source_supported_media = GRL_MEDIA_TYPE_IMAGE;
    else if (g_strcmp0 (lua_source_media, "all") == 0)
      *source_supported_media = GRL_MEDIA_TYPE_ALL;
  }

  if (lua_source_icon != NULL) {
    GFile *file = g_file_new_for_uri (lua_source_icon);
    *source_icon = g_file_icon_new (file);
    g_object_unref (file);
  }

  *auto_split_threshold = lua_auto_split_threshold;

  *source_tags = lua_source_tags;

  return LUA_OK;
}

static gint
lua_plugin_source_operations (lua_State *L,
                              gboolean fn[LUA_NUM_OPERATIONS])
{
  gint i = 0;

  GRL_DEBUG ("lua_plugin_source_operations");

  /* Initialize fn array */
  for (i = 0; i < LUA_NUM_OPERATIONS; i++) {
    lua_getglobal (L, LUA_SOURCE_OPERATION[i]);
    fn[i] = (lua_isfunction (L, -1)) ? TRUE : FALSE;
    lua_pop (L, 1);
  }

  return LUA_OK;
}

static gint
lua_plugin_source_all_dependencies (lua_State *L)
{
  GList *it = NULL;
  GList *table_list = NULL;
  gboolean module_fail = FALSE;

  GRL_DEBUG ("lua_plugin_source_all_dependencies");

  /* Dependencies are in the main table */
  lua_getglobal (L, LUA_SOURCE_TABLE);

  /* Check if lua modules dependencies are installed */
  table_list = table_array_to_list (L, LUA_SOURCE_MODULES_DEPS);
  if (table_list != NULL) {
    gchar *lua_module = NULL;

    for (it = table_list; it; it = g_list_next (it)) {
      lua_module = it->data;

      if (lua_module_exists (lua_module) == FALSE) {
        module_fail = TRUE;
        GRL_INFO ("%s %s", lua_module, "lua module is not installed");
      }
    }
  }

  g_list_free_full (table_list, g_free);
  return (module_fail) ? !LUA_OK : LUA_OK;
}

static gint
lua_plugin_source_all_keys (lua_State *L,
                            const gchar *source_id,
                            GList **supported_keys,
                            GList **slow_keys,
                            GList **resolve_keys,
                            GrlMediaType *resolve_type,
                            GHashTable **config_keys)
{
  GrlRegistry *registry = NULL;
  GList *table_list = NULL;
  GList *it = NULL;
  GHashTable *htable = NULL;
  const gchar *key_name = NULL;

  GRL_DEBUG ("lua_plugin_source_all_keys");

  /* Keys are in the main table */
  lua_getglobal (L, LUA_SOURCE_TABLE);

  /* Registry to get metadata keys from key's name */
  registry = grl_registry_get_default ();

  /* Supported keys */
  *supported_keys = keys_table_array_to_list (L, LUA_SOURCE_SUPPORTED_KEYS, registry, source_id);

  /* Slow keys */
  *slow_keys = keys_table_array_to_list (L, LUA_SOURCE_SLOW_KEYS, registry, source_id);

  /* Resolve keys - type, required fields */
  lua_pushstring (L, LUA_SOURCE_RESOLVE_KEYS);
  lua_gettable (L, -2);
  if (lua_istable (L, -1)) {
    GrlMediaType media_type = GRL_MEDIA_TYPE_NONE;

    /* check required type field */
    lua_pushstring (L, "type");
    lua_gettable (L, -2);
    if (lua_isstring (L, -1)) {
      key_name = lua_tostring (L, -1);
      if (g_strcmp0 (key_name, "audio") == 0)
        media_type = GRL_MEDIA_TYPE_AUDIO;
      else if (g_strcmp0 (key_name, "video") == 0)
        media_type = GRL_MEDIA_TYPE_VIDEO;
      else if (g_strcmp0 (key_name, "image") == 0)
        media_type = GRL_MEDIA_TYPE_IMAGE;
      else if (g_strcmp0 (key_name, "all") == 0)
        media_type = GRL_MEDIA_TYPE_ALL;
    }
    lua_pop (L, 1);

    *resolve_type = media_type;

    /* check required table field */
    *resolve_keys = keys_table_array_to_list (L, LUA_REQUIRED_TABLE, registry, source_id);
  }
  lua_pop (L, 1);

  /* Config keys - required and optional fields */
  lua_pushstring (L, LUA_SOURCE_CONFIG_KEYS);
  lua_gettable (L, -2);
  if (lua_istable (L, -1)) {
    htable = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

    /* check required table field */
    table_list = table_array_to_list (L, LUA_REQUIRED_TABLE);
    if (table_list != NULL) {
      for (it = table_list; it; it = g_list_next (it)) {
        key_name = it->data;
        g_hash_table_insert (htable, g_strdup (key_name), g_strdup ("true"));
      }
    }
    g_list_free_full (table_list, g_free);

    /* check optional table field */
    table_list = table_array_to_list (L, LUA_OPTIONAL_TABLE);
    if (table_list != NULL) {
      for (it = table_list; it; it = g_list_next (it)) {
        key_name = it->data;
        g_hash_table_insert (htable, g_strdup (key_name), g_strdup ("false"));
      }
    }
    g_list_free_full (table_list, g_free);

    *config_keys = htable;
  }
  lua_pop (L, 1);

  return LUA_OK;
}

/* ================== API Implementation =================================== */

static const GList *
grl_lua_factory_source_supported_keys (GrlSource *source)
{
  GrlLuaFactorySource *lua_source = GRL_LUA_FACTORY_SOURCE (source);
  return lua_source->priv->supported_keys;
}

static const GList *
grl_lua_factory_source_slow_keys (GrlSource *source)
{
  GrlLuaFactorySource *lua_source = GRL_LUA_FACTORY_SOURCE (source);
  return lua_source->priv->slow_keys;
}

static GrlSupportedOps
grl_lua_factory_source_supported_operations (GrlSource *source)
{
  GrlSupportedOps caps = 0;
  GrlLuaFactorySource *lua_source = GRL_LUA_FACTORY_SOURCE (source);

  caps |= (lua_source->priv->fn[LUA_SEARCH]) ? GRL_OP_SEARCH : caps;
  caps |= (lua_source->priv->fn[LUA_BROWSE]) ? GRL_OP_BROWSE : caps;
  caps |= (lua_source->priv->fn[LUA_QUERY]) ? GRL_OP_QUERY : caps;
  caps |= (lua_source->priv->fn[LUA_RESOLVE]) ? GRL_OP_RESOLVE : caps;

  return caps;
}

static void
grl_lua_factory_source_search (GrlSource *source,
                               GrlSourceSearchSpec *ss)
{
  GrlLuaFactorySource *lua_source = GRL_LUA_FACTORY_SOURCE (source);
  lua_State *L = lua_source->priv->l_st;
  OperationSpec *os = NULL;
  const gchar *text = NULL;

  GRL_DEBUG ("grl_lua_factory_source_search");

  text = (ss->text == NULL) ? "" : ss->text;

  os = g_slice_new0 (OperationSpec);
  os->source = ss->source;
  os->operation_id = ss->operation_id;
  os->cb.result = ss->callback;
  os->user_data = ss->user_data;
  os->string = g_strdup (text);
  os->error_code = GRL_CORE_ERROR_SEARCH_FAILED;
  os->keys = g_list_copy (ss->keys);
  os->options = grl_operation_options_copy (ss->options);
  os->op_type = LUA_SEARCH;

  grl_lua_library_save_operation_data (L, os);
  grl_lua_library_set_current_operation (L, os->operation_id);
  lua_getglobal (L, LUA_SOURCE_OPERATION[LUA_SEARCH]);

  lua_pushstring (L, text);
  if (lua_pcall (L, 1, 0, 0)) {
    GRL_WARNING ("%s '%s'", "calling search function fail:",
                 lua_tolstring (L, -1, NULL));
    lua_pop (L, 1);
  }

  grl_lua_library_set_current_operation (L, 0);
}

static void
grl_lua_factory_source_browse (GrlSource *source,
                               GrlSourceBrowseSpec *bs)
{
  GrlLuaFactorySource *lua_source = GRL_LUA_FACTORY_SOURCE (source);
  lua_State *L = lua_source->priv->l_st;
  OperationSpec *os = NULL;
  const gchar *media_id = NULL;

  GRL_DEBUG ("grl_lua_factory_source_browse");

  media_id = bs->container ? grl_media_get_id (bs->container) : NULL;

  os = g_slice_new0 (OperationSpec);
  os->source = bs->source;
  os->operation_id = bs->operation_id;
  os->media = bs->container;
  os->cb.result = bs->callback;
  os->user_data = bs->user_data;
  os->string = g_strdup (media_id);
  os->error_code = GRL_CORE_ERROR_BROWSE_FAILED;
  os->keys = g_list_copy (bs->keys);
  os->options = grl_operation_options_copy (bs->options);
  os->op_type = LUA_BROWSE;

  grl_lua_library_save_operation_data (L, os);
  grl_lua_library_set_current_operation (L, os->operation_id);
  lua_getglobal (L, LUA_SOURCE_OPERATION[LUA_BROWSE]);

  lua_pushstring (L, media_id);
  if (lua_pcall (L, 1, 0, 0)) {
    GRL_WARNING ("%s '%s'", "calling browse function fail:",
                 lua_tolstring (L, -1, NULL));
    lua_pop (L, 1);
  }

  grl_lua_library_set_current_operation (L, 0);
}

static void
grl_lua_factory_source_query (GrlSource *source,
                              GrlSourceQuerySpec *qs)
{
  GrlLuaFactorySource *lua_source = GRL_LUA_FACTORY_SOURCE (source);
  lua_State *L = lua_source->priv->l_st;
  OperationSpec *os = NULL;
  const gchar *query = NULL;

  GRL_DEBUG ("grl_lua_factory_source_query");

  query = (qs->query == NULL) ? "" : qs->query;

  os = g_slice_new0 (OperationSpec);
  os->source = qs->source;
  os->operation_id = qs->operation_id;
  os->cb.result = qs->callback;
  os->user_data = qs->user_data;
  os->string = g_strdup (query);
  os->error_code = GRL_CORE_ERROR_QUERY_FAILED;
  os->keys = g_list_copy (qs->keys);
  os->options = grl_operation_options_copy (qs->options);
  os->op_type = LUA_QUERY;

  grl_lua_library_save_operation_data (L, os);
  grl_lua_library_set_current_operation (L, os->operation_id);
  lua_getglobal (L, LUA_SOURCE_OPERATION[LUA_QUERY]);

  lua_pushstring (L, query);
  if (lua_pcall (L, 1, 0, 0)) {
    GRL_WARNING ("%s '%s'", "calling query function fail:",
                 lua_tolstring (L, -1, NULL));
    lua_pop (L, 1);
  }

  grl_lua_library_set_current_operation (L, 0);
}

static void
grl_lua_factory_source_resolve (GrlSource *source,
                                GrlSourceResolveSpec *rs)
{
  GrlLuaFactorySource *lua_source = GRL_LUA_FACTORY_SOURCE (source);
  lua_State *L = lua_source->priv->l_st;
  OperationSpec *os = NULL;

  GRL_DEBUG ("grl_lua_factory_source_resolve");

  g_return_if_fail (grl_lua_library_load_operation_data (L, rs->operation_id) == NULL);

  os = g_slice_new0 (OperationSpec);
  os->source = rs->source;
  os->operation_id = rs->operation_id;
  os->cb.resolve = rs->callback;
  os->media = rs->media;
  os->user_data = rs->user_data;
  os->error_code = GRL_CORE_ERROR_RESOLVE_FAILED;
  os->keys = g_list_copy (rs->keys);
  os->options = grl_operation_options_copy (rs->options);
  os->op_type = LUA_RESOLVE;

  grl_lua_library_save_operation_data (L, os);
  grl_lua_library_set_current_operation (L, os->operation_id);
  lua_getglobal (L, LUA_SOURCE_OPERATION[LUA_RESOLVE]);

  if (lua_pcall (L, 0, 0, 0)) {
    GRL_WARNING ("%s '%s'", "calling resolve function fail:",
                 lua_tolstring (L, -1, NULL));
    lua_pop (L, 1);
  }

  grl_lua_library_set_current_operation (L, 0);
}

static gboolean
grl_lua_factory_source_may_resolve (GrlSource *source,
                                    GrlMedia *media,
                                    GrlKeyID key_id,
                                    GList **missing_keys)
{
  GrlLuaFactorySource *lua_source = GRL_LUA_FACTORY_SOURCE (source);
  GList *it_keys = NULL;
  GList *missing = NULL;
  GrlMediaType res_type = GRL_MEDIA_TYPE_NONE;
  GrlKeyID it_key_id = GRL_METADATA_KEY_INVALID;

  GRL_DEBUG ("grl_lua_factory_source_may_resolve");

  if (lua_source->priv->resolve_keys == NULL
      || !g_list_find (lua_source->priv->supported_keys,
                       GRLKEYID_TO_POINTER (key_id))) {
    return FALSE;
  }

  /* Verify if the source resolve type and media type match */
  res_type = lua_source->priv->resolve_type;
  if ((GRL_IS_MEDIA_BOX (media) && (res_type != GRL_MEDIA_TYPE_ALL))
      || (GRL_IS_MEDIA_AUDIO (media) && !(res_type & GRL_MEDIA_TYPE_AUDIO))
      || (GRL_IS_MEDIA_IMAGE (media) && !(res_type & GRL_MEDIA_TYPE_IMAGE))
      || (GRL_IS_MEDIA_VIDEO (media) && !(res_type & GRL_MEDIA_TYPE_VIDEO))) {
    return FALSE;
  }

  /* Verify if all keys needed are present */
  for (it_keys = lua_source->priv->resolve_keys;
       it_keys;
       it_keys = g_list_next (it_keys)) {

    it_key_id = GRLPOINTER_TO_KEYID (it_keys->data);
    if (it_key_id != GRL_METADATA_KEY_INVALID
        && grl_data_has_key (GRL_DATA (media), it_key_id) == FALSE) {
      missing = g_list_prepend (missing, GRLKEYID_TO_POINTER (it_key_id));
    }
  }

  *missing_keys = missing;
  return (missing == NULL) ? TRUE : FALSE;
}
