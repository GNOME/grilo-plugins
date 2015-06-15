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

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <grilo.h>

#ifndef _GRL_LUA_LIBRARY_H_
#define _GRL_LUA_LIBRARY_H_

#define GRILO_LUA_LIBRARY_NAME "grl"
#define LUA_MODULES_NAME       "lua"

#define LUA_ENV_TABLE "_G"

#define GRILO_LUA_OPERATION_INDEX "grl-lua-operation-spec"
#define GRILO_LUA_INSPECT_INDEX   "grl-lua-data-inspect"

gint luaopen_grilo (lua_State *L);

#endif /* _GRL_LUA_LIBRARY_H_ */
