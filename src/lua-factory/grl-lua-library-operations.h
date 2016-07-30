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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GRL_LUA_LIBRARY_OPERATIONS_COMMON_H_
#define _GRL_LUA_LIBRARY_OPERATIONS_COMMON_H_

/* Private state of operations of the source */
#define LUA_SOURCE_PRIV_STATE "__priv_state"
#define LUA_SOURCE_OPERATIONS "operations"
#define LUA_SOURCE_CURRENT_OP "current_operation"
#define LUA_SOURCE_PROPERTIES "properties"

#define SOURCE_OP_STATE "state"
#define SOURCE_OP_DATA  "data"
#define SOURCE_OP_ID    "op_id"

#define SOURCE_PROP_NET_WC "net_wc"

#endif /* _GRL_LUA_LIBRARY_OPERATIONS_COMMON_H_ */
