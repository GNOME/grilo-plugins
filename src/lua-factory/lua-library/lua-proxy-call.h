/*
 * Copyright (C) 2024 Krifa75.
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

#pragma once

#include <rest/rest.h>

G_BEGIN_DECLS

#define LUA_TYPE_PROXY_CALL (lua_proxy_call_get_type())

G_DECLARE_DERIVABLE_TYPE (LuaProxyCall, lua_proxy_call, LUA, PROXY_CALL, RestProxyCall)

struct _LuaProxyCallClass {
  RestProxyCallClass parent_class;
};

RestProxyCall*  lua_rest_proxy_call_new (RestProxy *proxy);

G_END_DECLS
