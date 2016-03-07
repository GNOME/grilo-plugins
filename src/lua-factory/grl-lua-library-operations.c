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
 * Exported functions ======================================================
 * ========================================================================= */

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
