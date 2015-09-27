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

#ifndef _GRL_LUA_LIBRARY_JSON_H_
#define _GRL_LUA_LIBRARY_JSON_H_

#define GRESOURCE_LIBRARY_PREFIX    "resource:///org/gnome/grilo/plugins/lua-factory/lua-library/"
#define URI_LUA_LIBRARY_INSPECT     GRESOURCE_LIBRARY_PREFIX "inspect.lua"

#define GRILO_LUA_LIBRARY_INSPECT   "inspect"
#define GRILO_LUA_LIBRARY_JSON      "json"
#define GRILO_LUA_LIBRARY_XML       "xml"

gint luaopen_json (lua_State *L);
gint luaopen_xml (lua_State *L);

#endif /* _GRL_LUA_LIBRARY_JSON_H_ */
