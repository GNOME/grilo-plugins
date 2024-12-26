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

#include "lua-proxy-call.h"

G_DEFINE_FINAL_TYPE (LuaProxyCall, lua_proxy_call, REST_TYPE_PROXY_CALL)

static gboolean
lua_proxy_call_serialize_params (RestProxyCall    *self,
                                 gchar           **content_type,
                                 gchar           **content,
                                 gsize            *content_len,
                                 GError          **error)
{
  g_return_val_if_fail (LUA_IS_PROXY_CALL (self), FALSE);
  RestParam *param;

  param = rest_proxy_call_lookup_param (self, "grl-json");
  if (param == NULL)
    return FALSE;

  *content_type = g_strdup ("application/json");
  *content = g_strdup (rest_param_get_content (param));
  *content_len = strlen(*content);

  return TRUE;
}

static void
lua_proxy_call_class_init (LuaProxyCallClass *klass)
{
  RestProxyCallClass *call_class = REST_PROXY_CALL_CLASS (klass);

  call_class->serialize_params = lua_proxy_call_serialize_params;
}

static void
lua_proxy_call_init (LuaProxyCall *self)
{
}

RestProxyCall *
lua_rest_proxy_call_new (RestProxy *proxy)
{
  return g_object_new (LUA_TYPE_PROXY_CALL,
                       "proxy", proxy,
                       NULL);
}
