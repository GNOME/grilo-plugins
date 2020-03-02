/*
 * DPAPRecord factory class
 *
 * Copyright (C) 2008 W. Michael Petullo <mike@flyn.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <grilo.h>
#include <libdmapsharing/dmap.h>

#include "grl-dpap-compat.h"
#include "grl-common.h"
#include "grl-dpap-record-factory.h"
#include "grl-dpap-record.h"

DmapRecord *
grl_dpap_record_factory_create (DmapRecordFactory *factory,
                                gpointer user_data,
                                GError **error)
{
  return DMAP_RECORD (grl_dpap_record_new ());
}

static void
grl_dpap_record_factory_init (GrlDpapRecordFactory *factory)
{
}

static void
grl_dpap_record_factory_class_init (GrlDpapRecordFactoryClass *klass)
{
}

static void
grl_dpap_record_factory_interface_init (gpointer iface, gpointer data)
{
  DmapRecordFactoryInterface *factory = iface;

  g_assert (G_TYPE_FROM_INTERFACE (factory) == DMAP_TYPE_RECORD_FACTORY);

  factory->create = grl_dpap_record_factory_create_compat;
}

G_DEFINE_TYPE_WITH_CODE (GrlDpapRecordFactory, grl_dpap_record_factory, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (DMAP_TYPE_RECORD_FACTORY,
                                                grl_dpap_record_factory_interface_init))

GrlDpapRecordFactory *
grl_dpap_record_factory_new (void)
{
  return SIMPLE_DPAP_RECORD_FACTORY (g_object_new (TYPE_SIMPLE_DPAP_RECORD_FACTORY, NULL));
}
