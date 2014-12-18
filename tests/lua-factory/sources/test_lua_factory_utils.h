/*
 * Copyright (C) 2015. All rights reserved.
 *
 * Author: Victor Toso <me@victortoso.com>
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

#ifndef _GRL_LUA_FACTORY_TEST_UTILS_H_
#define _GRL_LUA_FACTORY_TEST_UTILS_H_

#define LUA_FACTORY_ID "grl-lua-factory"

#include <grilo.h>

void test_lua_factory_init (gint *p_argc, gchar ***p_argv, gboolean net_mocked);
void test_lua_factory_setup (GrlConfig *config);
void test_lua_factory_shutdown (void);
void test_lua_factory_deinit (void);
GrlSource* test_lua_factory_get_source (gchar *source_id, GrlSupportedOps source_ops);

#endif /* _GRL_LUA_FACTORY_TEST_UTILS_H_ */
