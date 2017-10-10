/*
 * Copyright (C) 2016 Victor Toso.
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

#include "config.h"
#include "grl-lua-common.h"
#include "grl-lua-library-operations.h"

#define GRL_LOG_DOMAIN_DEFAULT lua_library_operations_log_domain
GRL_LOG_DOMAIN_STATIC (lua_library_operations_log_domain);

static const gchar * const source_op_state_str[LUA_SOURCE_NUM_STATES] = {
  "running",
  "waiting",
  "finalized"
};

static OperationSpec * priv_state_current_op_get_op_data (lua_State *L);
static void priv_state_properties_free (lua_State *L);

/* =========================================================================
 * Internal functions ======================================================
 * ========================================================================= */

/* ============== Helpers ================================================== */

static void
free_operation_spec (OperationSpec *os)
{
  g_clear_pointer (&os->string, g_free);
  g_clear_object (&os->options);

  if (os->cancellable) {
    g_cancellable_cancel (os->cancellable);
    g_clear_object (&os->cancellable);
  }

  if (os->keys)
    g_list_free (os->keys);

  g_slice_free (OperationSpec, os);
}

/* ============== Proxy related ============================================ */

/*
 * Get the original table from proxy which is still able to rw operations
 *
 * @index: position to the proxy table in the stack
 * return: the original table in top of the stack
 *
 */
static void
proxy_table_get_rw (lua_State *L,
                    guint index)
{
  gint *table_ref;

  /* using table as function */
  lua_pushvalue (L, index);
  table_ref = lua_newuserdata (L, sizeof (gint));
  *table_ref = 0;
  if (lua_pcall (L, 1, 0, 0)) {
    GRL_WARNING ("Failed to get rw table due: %s",
                 lua_tolstring (L, -1, NULL));
    lua_pop (L, 1);
  }
  lua_rawgeti (L, LUA_REGISTRYINDEX, *table_ref);
  luaL_unref (L, LUA_REGISTRYINDEX, *table_ref);
}

/*
 * proxy handler for __call metamethod; This metamethod is called when using
 * the table as a function (e.g {}()). The proxy uses this metamethod in order
 * to retrieve a reference to original table which is still capabable of
 * read-write operations.
 *
 * @userdata: Expects pointer to integer which will hold the reference to the
 * requested table.
 */
static int
proxy_metatable_handle_call (lua_State *L)
{
  luaL_argcheck (L, lua_istable (L, 1), 1, "First argument is always itself");
  luaL_argcheck (L, lua_isuserdata (L, 2), 2,
                 "expecting userdata as reference holder (gint *)");
  gint *table_ref = lua_touserdata (L, 2);
  lua_pushvalue (L, lua_upvalueindex (1));
  *table_ref = luaL_ref (L, LUA_REGISTRYINDEX);
  return 0;
}

/*
 * proxy handler for __newindex metamethod
 * This metamethod is called when updating a table with a new key/value.
 */
static int
proxy_metatable_handle_newindex (lua_State *L)
{
  luaL_error (L, "Trying to change read-only table");
  return 0;
}

/* ============== Private State helpers ==================================== */

/*
 * Should clear all data stored by us in private state table
 */
static int
priv_state_metatable_gc (lua_State *L)
{
  priv_state_properties_free (L);
  return 0;
}

/*
 * Expects private state table in top of stack so it can set its metatable
 */
static void
priv_state_set_metatable (lua_State *L)
{
  g_return_if_fail(lua_istable(L, -1));

  /* create the metatable */
  lua_createtable (L, 0, 1);
  /* push the __gc key string */
  lua_pushstring (L, "__gc");
  /* push the __gc metamethod */
  lua_pushcfunction (L, priv_state_metatable_gc);
  /* set the __gc field in the metatable */
  lua_settable (L, -3);
  /* set table as the metatable of the userdata */
  lua_setmetatable (L, -2);
}
/*
 * Helper function to let rw table from proxy in the top of stack
 */
static void
priv_state_get_rw_table (lua_State *L,
                         const gchar *table_name)
{
  gint top_stack = 3;

  lua_getglobal (L, GRILO_LUA_LIBRARY_NAME);
  g_return_if_fail (lua_istable (L, -1));
  lua_getfield (L, -1, LUA_SOURCE_PRIV_STATE);
  g_return_if_fail (lua_istable (L, -1));

  if (!g_str_equal (table_name, LUA_SOURCE_PRIV_STATE)) {
    top_stack = 4;
    lua_getfield (L, -1, table_name);
    g_return_if_fail (lua_istable (L, -1));
  }

  proxy_table_get_rw (L, -1);
  g_return_if_fail (lua_istable (L, -1));

  /* keep the rw table but remove the others */
  lua_replace (L, -top_stack);
  lua_pop (L, top_stack - 2);
}

/* ============== Private State (current operation field) =================== */

/*
 * Set the state of current operation based on ongoing Grilo Operation.
 * Note that each source only supports one RUNNING operation at time.
 *
 * @index: index for State table
 * -- It does't modify the stack
 */
static void
priv_state_current_op_set (lua_State *L,
                           gint index)
{
  priv_state_get_rw_table (L, LUA_SOURCE_PRIV_STATE);

  /* Check for no ongoing operation */
  lua_getfield (L, -1, LUA_SOURCE_CURRENT_OP);
  if (!lua_isnil (L, -1)) {
    GRL_DEBUG ("Current operation is already set. Might be a bug.");
  }
  lua_pop (L, 1);

  g_return_if_fail (lua_istable (L, -1));

  /* Set current operation */
  lua_pushstring (L, LUA_SOURCE_CURRENT_OP);
  lua_pushvalue (L, index - 2);
  lua_settable (L, -3);

  /* Remove rw table from stack */
  lua_pop (L, 1);
}

/*
 * Remove the current state operation based on ongoing Grilo Operation.
 * Note that each source only supports one RUNNING operation at time.
 *
 * @index: index for State table
 * -- It does't modify the stack
 */
static void
priv_state_current_op_remove (lua_State *L)
{
  priv_state_get_rw_table (L, LUA_SOURCE_PRIV_STATE);

  /* Check for a ongoing operation */
  lua_getfield (L, -1, LUA_SOURCE_CURRENT_OP);
  g_return_if_fail (lua_istable (L, -1));
  lua_pop (L, 1);

  /* Remove current operation */
  lua_pushstring (L, LUA_SOURCE_CURRENT_OP);
  lua_pushnil (L);
  lua_settable (L, -3);

  /* Remove rw table from stack */
  lua_pop (L, 1);
}

static OperationSpec *
priv_state_current_op_get_op_data (lua_State *L)
{
  OperationSpec *os = NULL;

  priv_state_get_rw_table (L, LUA_SOURCE_PRIV_STATE);

  /* Check for no ongoing operation */
  lua_getfield (L, -1, LUA_SOURCE_CURRENT_OP);
  if (!lua_istable (L, -1)) {
    GRL_WARNING ("No ongoing operation!");
    lua_pop (L, 2);
    return NULL;
  }

  lua_getfield (L, -1, SOURCE_OP_DATA);
  g_return_val_if_fail (lua_islightuserdata (L, -1), NULL);
  os = lua_touserdata (L, -1);
  g_return_val_if_fail (os != NULL, NULL);

  lua_pop (L, 3);
  return os;
}

/* ============== Private State (operations array field) ==================== */

/*
 * Create a Lua Source State in order to track operations
 * @os: OperationSpec of operation
 * -- keep the Lua Source State in the top of the stack
 */
static void
priv_state_operations_create_source_state (lua_State *L,
                                           OperationSpec *os)
{
  GRL_DEBUG ("%s | %s (op-id: %u)", __func__,
             grl_source_get_id (os->source),
             os->operation_id);

  /* Lua Source State table */
  lua_newtable (L);

  lua_pushstring (L, SOURCE_OP_ID);
  lua_pushinteger (L, os->operation_id);
  lua_settable (L, -3);

  lua_pushstring (L, SOURCE_OP_STATE);
  lua_pushstring (L, source_op_state_str[LUA_SOURCE_RUNNING]);
  lua_settable (L, -3);

  /* The OperationSpec here stored is only the pointer. It is expected
   * that the real OperationSpec data being stored under userdata with
   * a __gcc metamethod assigned to it */
  lua_pushstring (L, SOURCE_OP_DATA);
  lua_pushlightuserdata (L, os);
  lua_settable (L, -3);
}

/*
 * Look in operations table if @op_id is found. In case it was found, remove it
 * and keep it in the top of the stack otherwise we push nil to the top of the
 * stack
 *
 * @op_id: Grilo operation-id
 * -- push nil or Lua Source State table in the top of the stack
 */
static void
priv_state_operations_get_source_state (lua_State *L,
                                        guint op_id)
{
  guint op_position = 0;
  priv_state_get_rw_table (L, LUA_SOURCE_OPERATIONS);

  lua_pushnil (L);
  while (lua_next (L, -2) != 0) {
    gint id;

    lua_getfield (L, -1, SOURCE_OP_ID);
    id = lua_tointeger (L, -1);
    if (id == op_id) {
      op_position = lua_tointeger (L, -3);
      lua_pop (L, 3);
      break;
    }
    lua_pop (L, 2);
  }

  if (op_position == 0) {
    /* remove the rw table */
    lua_pop (L, 1);
    lua_pushnil (L);
    return;
  }

  /* push to stack the Lua Source Table */
  lua_pushinteger (L, op_position);
  lua_gettable (L, -2);

  /* remove the state from operations table */
  lua_pushinteger (L, op_position);
  lua_pushnil (L);
  lua_settable (L, -4);

  /* remove the rw table and only keep the state table */
  lua_replace (L, -2);
}

/*
 * Insert the Lua Source State pointed by @index in the table of
 * opeartions
 * @index:  position in the stack for Lua Source table
 * -- It does't modify the stack
 */
static void
priv_state_operations_insert_source_state (lua_State *L,
                                           gint index)
{
  guint num_operations = 0;

  priv_state_get_rw_table (L, LUA_SOURCE_OPERATIONS);

  num_operations = luaL_len (L, -1);
  lua_pushinteger (L, num_operations + 1);
  lua_pushvalue (L, index - 2);
  lua_settable (L, -3);

  /* remove the rw table */
  lua_pop (L, 1);
}

static void
priv_state_operations_remove_source_state (lua_State *L,
                                           guint operation_id)
{
  priv_state_operations_get_source_state (L, operation_id);
  if (lua_isnil (L, -1)) {
    GRL_DEBUG ("Operation %u not found!", operation_id);
  }
  lua_pop (L, 1);
}

/*
 * Get from Operations array the LuaSourceState string which matches
 * the @operation_id. Returns NULL if it was not found.
 *
 * @L: The LuaState of source
 * @operation_id: Grilo operation-id for Operation data.
 * -- It does not change the stack
 */
static const gchar *
priv_state_operations_source_get_state_str (lua_State *L,
                                            guint operation_id)
{
  const gchar *str;

  priv_state_operations_get_source_state (L, operation_id);
  if (lua_isnil (L, -1)) {
    lua_pop (L, 1);
    return NULL;
  }

  g_return_val_if_fail (lua_istable (L, -1), NULL);
  lua_getfield (L, -1, SOURCE_OP_STATE);
  str = lua_tostring (L, -1);

  /* Keep the stack as it was before and insert the state table back
   * to Operations array */
  priv_state_operations_insert_source_state (L, -2);
  lua_pop (L, 2);
  return str;
}

static LuaSourceState
priv_state_operations_source_get_state (lua_State *L,
                                        guint operation_id)
{
  const gchar *state_str;
  guint i;

  state_str = priv_state_operations_source_get_state_str (L, operation_id);
  for (i = LUA_SOURCE_RUNNING; i < LUA_SOURCE_NUM_STATES; i++) {
   if (g_strcmp0 (state_str, source_op_state_str[i]) == 0)
     return i;
  }

  g_assert_not_reached ();
}

/*
 * Get the from Operations array the OperationSpec which matches the
 * @operation_id. Returns NULL it it was not found.
 *
 * @L: The LuaState of source
 * @operation_id: Grilo operation-id for Operation data.
 * -- It does not change the stack
 */
static OperationSpec *
priv_state_operations_source_get_op_data (lua_State *L,
                                          guint operation_id)
{
  OperationSpec *os;

  priv_state_operations_get_source_state (L, operation_id);
  if (lua_isnil (L, -1)) {
    lua_pop (L, 1);
    return NULL;
  }

  g_return_val_if_fail (lua_istable (L, -1), NULL);
  lua_getfield (L, -1, SOURCE_OP_DATA);
  os = lua_touserdata (L, -1);

  /* Keep the stack as it was before and insert the state table back
   * to Operations array */
  priv_state_operations_insert_source_state (L, -2);
  lua_pop (L, 2);
  return os;
}

static void
priv_state_operations_update (lua_State *L,
                              OperationSpec *os,
                              LuaSourceState state)
{
  priv_state_operations_get_source_state (L, os->operation_id);

  if (lua_istable (L, -1)) {
    lua_pushstring (L, SOURCE_OP_STATE);
    lua_pushstring (L, source_op_state_str[state]);
    lua_settable (L, -3);
    priv_state_operations_insert_source_state (L, -1);
    return;
  }

  if (lua_isnil (L, -1) && state == LUA_SOURCE_RUNNING) {
    lua_pop (L, 1);
    priv_state_operations_create_source_state (L, os);
    priv_state_operations_insert_source_state (L, -1);
    return;
  }

  GRL_ERROR ("Ongoig operation not found (op-id: %d)", os->operation_id);
}

/* ============== Private State - Properties ================================ */

/**
 * priv_state_properties_new
 *
 * Creates a table of properties that this lua_State will hold for all
 * operations that might happen.
 *
 * @L: LuaState of this GrlSource
 *
 * Leaves a new table in top of the stack
 **/
static void
priv_state_properties_new (lua_State *L)
{
  GrlNetWc *wc;

  lua_newtable (L);

  wc = grl_net_wc_new ();
  lua_pushstring (L, SOURCE_PROP_NET_WC);
  lua_pushlightuserdata (L, wc);
  lua_settable (L, -3);
}

/**
 * priv_state_properties_free
 *
 * Free the data inside the properties table but don't destroy the table itself
 * as garbage collection should handle that.
 *
 * @L: LuaState of this GrlSource
 *
 * Does not change the stack.
 **/
static void
priv_state_properties_free (lua_State *L)
{
  GrlNetWc *wc;

  priv_state_get_rw_table (L, LUA_SOURCE_PROPERTIES);

  lua_getfield (L, -1, SOURCE_PROP_NET_WC);
  g_return_if_fail (lua_islightuserdata (L, -1));
  wc = lua_touserdata (L, -1);
  g_object_unref (wc);

  /* Keep the stack as it was before */
  lua_pop (L, 2);
}

/**
 * priv_state_properties_get_prop
 *
 * Get the property given by @prop_name.
 *
 * @L: LuaState of this GrlSource
 * @prop_name: Property as gpointer
 *
 * Does not change the stack.
 **/
static gpointer
priv_state_properties_get_prop (lua_State *L,
                                const gchar *prop_name)
{
  gpointer property;

  priv_state_get_rw_table (L, LUA_SOURCE_PROPERTIES);
  lua_getfield (L, -1, prop_name);
  /* FIXME: Should we consider all properties as userdata?
   * https://bugzilla.gnome.org/show_bug.cgi?id=770794 */
  property = lua_touserdata (L, -1);

  /* Keep the stack as it was before */
  lua_pop (L, 2);
  return property;
}

/* ============== Watchdog related ========================================= */

/**
 * grl_util_operation_spec_gc
 *
 * This function is called when Lua GC is about to collect the userdata
 * representing OperationSpec. Here we check that the finishing callback
 * was done and free the memory.
 *
 * @L: LuaState where the data is stored.
 * @return: 0, as the number of objects left on stack.
 *          It is important, for Lua stack to not be corrupted.
 **/
static int
watchdog_operation_gc (lua_State *L)
{
  guint *pid = lua_touserdata (L, 1);
  LuaSourceState state = priv_state_operations_source_get_state (L, *pid);
  OperationSpec *os = priv_state_operations_source_get_op_data (L, *pid);
  OperationSpec *current_os = priv_state_current_op_get_op_data (L);
  const char *type;

  GRL_DEBUG ("%s | %s (op-id: %u) current state is: %s (num-async-op: %u)", __func__,
             grl_source_get_id (os->source),
             os->operation_id,
             source_op_state_str[state],
             os->lua_source_waiting_ops);

  switch (state) {
  case LUA_SOURCE_RUNNING:
    /* Check if waiting for async op, otherwise it is an error */
    if (os->lua_source_waiting_ops > 0) {
      GRL_DEBUG ("%s | %s (op-id: %u) awaiting for %u async operations", __func__,
                 grl_source_get_id (os->source),
                 os->operation_id,
                 os->lua_source_waiting_ops);
      pid = NULL;
      return 0;
    }
    break;

  case LUA_SOURCE_WAITING:
    /* Waiting for async op to finish, that's fine! */
    pid = NULL;
    return 0;
    break;

  case LUA_SOURCE_FINALIZED:
    if (os->lua_source_waiting_ops > 0) {
      GRL_WARNING ("Source '%s' is broken, as the finishing callback was called "
                   "while %u operations are still ongoing",
                   grl_source_get_id (os->source),
                   os->lua_source_waiting_ops);
      pid = NULL;
      return 0;
    }

    priv_state_operations_remove_source_state (L, os->operation_id);
    if (current_os->operation_id == os->operation_id)
      priv_state_current_op_remove (L);
    free_operation_spec (os);
    pid = NULL;
    return 0;
    break;

  default:
    g_assert_not_reached ();
  }

  switch (os->op_type) {
  case LUA_SEARCH:
    type = "search";
    break;
  case LUA_BROWSE:
    type = "browse";
    break;
  case LUA_QUERY:
    type = "query";
    break;
  case LUA_RESOLVE:
    type = "resolve";
    break;
  default:
    g_assert_not_reached ();
  }

  GRL_WARNING ("Source '%s' is broken, as the finishing "
               "callback was not called for %s operation",
               grl_source_get_id (os->source),
               type);
  switch (os->op_type) {
  case LUA_RESOLVE:
    os->cb.resolve (os->source, os->operation_id, os->media, os->user_data, NULL);
    break;

  default:
    os->cb.result (os->source, os->operation_id, NULL,
                   0, os->user_data, NULL);
  }

  free_operation_spec (os);
  pid = NULL;
  return 0;
}

/**
 * push_operation_spec_userdata
 *
 * Creates a userdata on top of the Lua stack, with the GC function
 * assigned to it.
 *
 * @L: LuaState where the data is stored.
 * @os: OperationSpec from which to create userdata.
 * @return: Nothing.
 **/
static void
watchdog_operation_push (lua_State *L, guint operation_id)
{
  gint *pid = lua_newuserdata (L, sizeof (guint));
  *pid = operation_id;

  /* create the metatable */
  lua_createtable (L, 0, 1);
  /* push the __gc key string */
  lua_pushstring (L, "__gc");
  /* push the __gc metamethod */
  lua_pushcfunction (L, watchdog_operation_gc);
  /* set the __gc field in the metatable */
  lua_settable (L, -3);
  /* set table as the metatable of the userdata */
  lua_setmetatable (L, -2);
}

/* =========================================================================
 * Exported functions ======================================================
 * ========================================================================= */

void
grl_lua_operations_init_priv_state (lua_State *L)
{
  GRL_LOG_DOMAIN_INIT (lua_library_operations_log_domain, "lua-library-operations");
  GRL_DEBUG ("lua-library-operations");

  g_return_if_fail (lua_istable (L, -1));
  lua_pushstring (L, LUA_SOURCE_PRIV_STATE);
  lua_newtable (L);

  lua_pushstring (L, LUA_SOURCE_OPERATIONS);
  lua_newtable (L);
  grl_lua_operations_set_proxy_table (L, -1);
  lua_settable (L, -3);

  lua_pushstring (L, LUA_SOURCE_CURRENT_OP);
  lua_pushnil (L);
  lua_settable (L, -3);

  lua_pushstring (L, LUA_SOURCE_PROPERTIES);
  priv_state_properties_new (L);
  grl_lua_operations_set_proxy_table (L, -1);
  lua_settable (L, -3);

  priv_state_set_metatable (L);
  grl_lua_operations_set_proxy_table (L, -1);
  lua_settable (L, -3);
}

GrlNetWc *
grl_lua_operations_get_grl_net_wc (lua_State *L)
{
  return priv_state_properties_get_prop (L, SOURCE_PROP_NET_WC);
}

/*
 * Create a read-only proxy table which will only be allowed to access the
 * original table.
 *
 * @index: position to the table in the stack
 * return: switch the table at @index with a read-only proxy
 */
void
grl_lua_operations_set_proxy_table (lua_State *L,
                                    gint index)
{
  g_return_if_fail (lua_istable (L, index));

  /* Proxy table that will be switched with the one at index */
  lua_newtable (L);

  /* Metatable */
  lua_createtable (L, 0, 3);

  /* __index: triggered when acessing a value of given table */
  lua_pushstring (L, "__index");
  lua_pushvalue (L, index - 3);
  lua_settable (L, -3);

  /* __len: triggered when counting the length of given table */
  lua_pushstring (L, "__len");
  lua_pushvalue (L, index - 3);
  lua_settable (L, -3);

  /* __newindex: triggered when inserting new key/value to given table */
  lua_pushstring (L, "__newindex");
  lua_pushcfunction (L, proxy_metatable_handle_newindex);
  lua_settable (L, -3);

  /* __call: triggered when using the table as a function */
  lua_pushstring (L, "__call");
  lua_pushvalue (L, index - 3);
  lua_pushcclosure (L, proxy_metatable_handle_call, 1);
  lua_settable (L, -3);

  /* Set metatable to our proxy */
  lua_setmetatable (L, -2);

  /* Replace original table with our proxy */
  lua_replace (L, index - 1);
}

OperationSpec *
grl_lua_operations_get_current_op (lua_State *L)
{
  OperationSpec *os;
  LuaSourceState state;

  os = priv_state_current_op_get_op_data (L);
  g_return_val_if_fail (os != NULL, NULL);

  state = priv_state_operations_source_get_state (L, os->operation_id);
  if (state == LUA_SOURCE_FINALIZED) {
    /* Source State is finalized. At this state it should be waiting the
     * watchdog to free its data. Only a broken source would request
     * OperationSpec on FINALIZED State */
    GRL_DEBUG ("The grilo operation ended when grl.callback() was called. "
               "No current operation for op-id: %u", os->operation_id);
    return NULL;
  }
  return os;
}

void
grl_lua_operations_cancel_operation (lua_State *L,
                                     guint operation_id)
{
    OperationSpec *os, *current_os;
    LuaSourceState state;

    os = priv_state_operations_source_get_op_data (L, operation_id);
    g_return_if_fail (os != NULL);

    state = priv_state_operations_source_get_state (L, operation_id);
    if (state != LUA_SOURCE_WAITING) {
      GRL_DEBUG ("Can't cancel operation (%u) on source (%s) with as state is: %s",
                 operation_id, grl_source_get_id (os->source),
                 source_op_state_str[state]);
      return;
    }

    /* All async operations on lua-library should verify os->cancellable to
     * proper handling the cancelation of ongoing operation */
    g_cancellable_cancel (os->cancellable);

    current_os = priv_state_current_op_get_op_data (L);

    priv_state_operations_remove_source_state (L, os->operation_id);
    if (current_os != NULL && current_os->operation_id == os->operation_id)
      priv_state_current_op_remove (L);
    free_operation_spec (os);
}

/*
 * This is a wrapper to do execute the lua_pcall and all internals that might
 * be necessary to Lua-Library before calling the Lua function. The stack
 * requirements are the same of lua_pcall, function and arguments in expected
 * order.
 */
gboolean
grl_lua_operations_pcall (lua_State *L,
                          gint nargs,
                          OperationSpec *os,
                          GError **err)
{
  gint ret;

  g_return_val_if_fail (os != NULL, FALSE);
  g_return_val_if_fail (err != NULL, FALSE);
  g_return_val_if_fail (*err == NULL, FALSE);

  GRL_DEBUG ("%s | %s (op-id: %u)", __func__,
             grl_source_get_id (os->source),
             os->operation_id);

  /* We control the GC during our function calls due the fact that we rely that
   * watchdog's finalizer function is called after the lua_pcall, otherwise the
   * watchdog fails to check for errors in the source and could lead to bugs */
  lua_gc (L, LUA_GCSTOP, 0);

  watchdog_operation_push (L, os->operation_id);
  grl_lua_operations_set_source_state (L, LUA_SOURCE_RUNNING, os);

  ret = lua_pcall (L, nargs + 1, 0, 0);
  if (ret != LUA_OK) {
    const gchar *msg = lua_tolstring (L, -1, NULL);
    lua_pop (L, 1);

    GRL_DEBUG ("lua_pcall failed: due %s (err %d)", msg, ret);
    *err = g_error_new_literal (GRL_CORE_ERROR, os->error_code, msg);
    grl_lua_operations_set_source_state (L, LUA_SOURCE_FINALIZED, os);
  }

  lua_gc (L, LUA_GCCOLLECT, 0);
  lua_gc (L, LUA_GCRESTART, 0);
  return (ret == LUA_OK);
}

/*
 * grl_lua_operations_set_source_state
 *
 * Sets the state for a Lua Source State operation table (LSS). If state is
 * LUA_SOURCE_RUNNING on a new operation, it will create the LSS and push it
 * to the Operations arrays and also set it as current operation;
 *
 * In any other case, this function will only set the given state. The memory
 * and error management on LSS is done by Operations' watchdog, check
 * watchdog_operation_gc for more details.
 */
void
grl_lua_operations_set_source_state (lua_State *L,
                                     LuaSourceState state,
                                     OperationSpec *os)
{
  g_return_if_fail (state < LUA_SOURCE_NUM_STATES);
  g_return_if_fail (os != NULL);

  GRL_DEBUG ("%s | %s (op-id: %u) state: %s", __func__,
             grl_source_get_id (os->source),
             os->operation_id,
             source_op_state_str[state]);
  switch (state) {
  case LUA_SOURCE_RUNNING:
    priv_state_operations_update (L, os, state);
    priv_state_current_op_set (L, -1);

    if (os->lua_source_waiting_ops > 0) {
      os->lua_source_waiting_ops -= 1;
    }
    break;

  case LUA_SOURCE_WAITING:
    priv_state_operations_update (L, os, state);

    os->lua_source_waiting_ops += 1;
    break;

  case LUA_SOURCE_FINALIZED:
    priv_state_operations_update (L, os, state);
    break;

  default:
    g_assert_not_reached ();
  }

  /* Remove Source State from stack */
  lua_pop (L, 1);
}
