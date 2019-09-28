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

#include <grilo.h>
#include <gst/gst.h>
#include "test_chromaprint_utils.h"

GrlSource *source = NULL;

void
test_setup_chromaprint (void)
{
  GrlRegistry *registry;
  GstElement *chromaprint;
  GError *error = NULL;

  gst_init (NULL, NULL);
  chromaprint = gst_element_factory_make ("chromaprint", "test");
  if (chromaprint == NULL) {
      g_warning ("chromaprint GStreamer plugin missing, verify your installation");
      g_assert_nonnull (chromaprint);
  }

  registry = grl_registry_get_default ();
  grl_registry_load_plugin (registry,
                            CHROMAPRINT_PLUGIN_PATH "libgrlchromaprint.so",
                            &error);
  g_assert_no_error (error);
  grl_registry_activate_plugin_by_id (registry, CHROMAPRINT_ID, &error);
  g_assert_no_error (error);

  source = GRL_SOURCE (grl_registry_lookup_source (registry, CHROMAPRINT_ID));
  g_assert (source != NULL);

  g_assert (grl_source_supported_operations (source) & GRL_OP_RESOLVE);
}

GrlSource* test_get_source (void)
{
  return source;
}

void
test_shutdown_chromaprint (void)
{
  GrlRegistry *registry;
  GError *error = NULL;

  registry = grl_registry_get_default ();
  grl_registry_unload_plugin (registry, CHROMAPRINT_ID, &error);
  g_assert_no_error (error);
}
