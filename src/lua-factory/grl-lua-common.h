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
#include <net/grl-net.h>

#ifdef GOA_ENABLED
#define GOA_API_IS_SUBJECT_TO_CHANGE
#include <goa/goa.h>
#endif

#define GOA_LUA_NAME "goa_object"

typedef enum {
  LUA_SEARCH,
  LUA_BROWSE,
  LUA_QUERY,
  LUA_RESOLVE,
  LUA_SOURCE_INIT,
  LUA_NUM_OPERATIONS
} LuaOperationType;

typedef enum {
  LUA_SOURCE_RUNNING = 0,
  LUA_SOURCE_WAITING,
  LUA_SOURCE_FINALIZED,
  LUA_SOURCE_NUM_STATES
} LuaSourceState;

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
  GCancellable *cancellable;
  GList *keys;
  LuaOperationType op_type;
  union {
    GrlSourceResultCb result;
    GrlSourceResolveCb resolve;
  } cb;
  gchar *string;
  GrlMedia *media;
  gpointer user_data;
  guint error_code;
  guint lua_source_waiting_ops;
} OperationSpec;

void grl_lua_library_save_goa_data (lua_State *L, gpointer goa_object);

/* grl-lua-library-operations */
void grl_lua_operations_init_priv_state (lua_State *L);
void grl_lua_operations_set_proxy_table (lua_State *L, gint index);
void grl_lua_operations_set_source_state (lua_State *L, LuaSourceState state, OperationSpec *os);
void grl_lua_operations_cancel_operation (lua_State *L, guint operation_id);
OperationSpec * grl_lua_operations_get_current_op (lua_State *L);
gboolean grl_lua_operations_pcall (lua_State *L, gint nargs, OperationSpec *os, GError **err);
GrlNetWc * grl_lua_operations_get_grl_net_wc (lua_State *L);

#endif /* _GRL_LUA_LIBRARY_COMMON_H_ */
