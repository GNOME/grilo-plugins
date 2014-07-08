/*
 * Copyright (C) 2016 Grilo Project
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

#ifndef _GRL_CHROMAPRINT_TEST_UTILS_H_
#define _GRL_CHROMAPRINT_TEST_UTILS_H_

#include <grilo.h>

#define CHROMAPRINT_ID "grl-chromaprint"
#define TEST_PATH    "data/"

void test_setup_chromaprint (void);
GrlSource* test_get_source (void);
void test_shutdown_chromaprint (void);

#endif /* _GRL_CHROMAPRINT_TEST_UTILS_H_ */
