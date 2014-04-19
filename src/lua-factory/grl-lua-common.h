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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GRL_LUA_LIBRARY_COMMON_H_
#define _GRL_LUA_LIBRARY_COMMON_H_

#include "grl-lua-library.h"
#include <glib/gi18n-lib.h>

typedef enum {
  LUA_SEARCH,
  LUA_BROWSE,
  LUA_QUERY,
  LUA_RESOLVE,
  LUA_SOURCE_INIT,
  LUA_NUM_OPERATIONS
} LuaOperationType;

/**
* OperationSpec:
* @source: The GrlLuaFactorySource of operation.
* @operation_id: The operation_id of operation that generate this structure.
* @op_type: Witch operation its being executed.
* @cb: union to user callback. The function parameters depends on operation.
*      resolve is used for LUA_RESOLVE operations
*      result is used for LUA_SEARCH, LUA_BROWSE and LUA_QUERY operations.
* @string: The text to search for for LUA_SEARCH operations,
*      the query for LUA_QUERY operations and the media ID for
*      LUA_BROWSE operations.
* @content: Save the current user media if already have one.
* @user_data: User data passed in user defined callback.
* @error_code: To set GRL_CORE_ERROR of the operation.
* @pending_ops: The number of pending async calls for this operation
* @callback_done: Whether grl.callback() was called
*
* This structure is used to save important data in the communication between
* lua-factory and lua-libraries.
*/
typedef struct _OperationSpec {
  GrlSource *source;
  guint operation_id;
  GrlOperationOptions *options;
  GList *keys;
  LuaOperationType op_type;
  union {
    GrlSourceResultCb result;
    GrlSourceResolveCb resolve;
  } cb;
  char *string;
  GrlMedia *media;
  gpointer user_data;
  guint error_code;
  guint pending_ops;
  gboolean callback_done;
} OperationSpec;

void grl_lua_library_save_operation_data (lua_State *L, OperationSpec *os);
void grl_lua_library_remove_operation_data (lua_State *L, guint operation_id);
OperationSpec *grl_lua_library_load_operation_data (lua_State *L, guint operation_id);

void grl_lua_library_set_current_operation (lua_State *L, guint operation_id);
OperationSpec * grl_lua_library_get_current_operation (lua_State *L);

#endif /* _GRL_LUA_LIBRARY_COMMON_H_ */
