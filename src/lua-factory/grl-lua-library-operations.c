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

/* =========================================================================
 * Internal functions ======================================================
 * ========================================================================= */

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

/* =========================================================================
 * Exported functions ======================================================
 * ========================================================================= */

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
  g_assert_true (lua_istable (L, index));

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
  lua_pushvalue (L, index - 3);
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
  if (lua_pcall (L, nargs, 0, 0)) {
    gint error_code = (os) ? os->error_code : G_IO_ERROR_CANCELLED;
    *err = g_error_new_literal (GRL_CORE_ERROR,
                                error_code,
                                lua_tolstring (L, -1, NULL));
    lua_pop (L, 1);
  }

  lua_gc (L, LUA_GCCOLLECT, 0);
  return (*err == NULL);
}
