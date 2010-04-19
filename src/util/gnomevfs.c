/*
 * Copyright (C) 2010 Igalia S.L.
 *
 * Contact: Iago Toral Quiroga <itoral@igalia.com>
 *
 * Authors: Xabier Rodriguez Calvar <xrcalvar@igalia.com>
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

#include <gnomevfs.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <string.h>


/* --------- Logging  -------- */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "grl-util-gnomevfs"

/* --- Misc --- */

#define GNOME_VFS_READ_SIZE 4096

typedef struct {
  GString *contents;
  GrlUtilGnomeVFSReadDoneCb read_done_cb;
  gpointer read_done_user_data;
} ReadCompleteInfo;

static void
gnome_vfs_close_cb (GnomeVFSAsyncHandle *handle, GnomeVFSResult result,
                    gpointer user_data)
{
  GrlUtilGnomeVFSReadDoneCb read_done_cb;
  gpointer read_done_user_data;
  gchar *contents;
  ReadCompleteInfo *rci = user_data;

  read_done_cb = rci->read_done_cb;
  read_done_user_data = rci->read_done_user_data;

  contents = g_string_free (rci->contents, result != GNOME_VFS_OK);

  g_debug ("result ok? %s, contents: %s", result == GNOME_VFS_OK ? "yes" : "no", contents);

  g_free (rci);

  read_done_cb (contents, read_done_user_data);
}

static void
gnome_vfs_read_cb (GnomeVFSAsyncHandle *handle, GnomeVFSResult result,
                   gpointer buffer, GnomeVFSFileSize bytes_requested,
                   GnomeVFSFileSize bytes_read, gpointer user_data)
{
  if (result == GNOME_VFS_OK || result == GNOME_VFS_ERROR_EOF) {
    ReadCompleteInfo *rci = user_data;
    g_string_append_len (rci->contents, buffer, bytes_read);
  }

  if (result == GNOME_VFS_OK) {
    gnome_vfs_async_read (handle, buffer, GNOME_VFS_READ_SIZE,
                          gnome_vfs_read_cb, user_data);
  } else {
    g_free (buffer);
    gnome_vfs_async_close (handle, gnome_vfs_close_cb, user_data);
  }
}

static void
gnome_vfs_open_cb (GnomeVFSAsyncHandle *handle, GnomeVFSResult result,
                   gpointer user_data)
{
  guchar *buffer = g_malloc (GNOME_VFS_READ_SIZE);

  gnome_vfs_async_read (handle, buffer, GNOME_VFS_READ_SIZE,
                        gnome_vfs_read_cb, user_data);
}

void
grl_plugins_gnome_vfs_read_url_async (const gchar *url,
                                      GrlUtilGnomeVFSReadDoneCb cb,
                                      gpointer user_data)
{
  GnomeVFSAsyncHandle *handle = NULL;
  ReadCompleteInfo *rci;

  rci = g_new (ReadCompleteInfo, 1);
  rci->contents = g_string_sized_new (GNOME_VFS_READ_SIZE);
  rci->read_done_cb = cb;
  rci->read_done_user_data = user_data;

  gnome_vfs_init ();

  g_debug ("Opening '%s'", url);
  gnome_vfs_async_open (&handle, url, GNOME_VFS_OPEN_READ,
                        GNOME_VFS_PRIORITY_DEFAULT, gnome_vfs_open_cb, rci);
}
