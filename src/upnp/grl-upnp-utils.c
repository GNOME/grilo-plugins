/*
 * Copyright (C) 2014 Giovanni Campagna <scampa.giovanni@gmail.com>
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

#define _GNU_SOURCE

#include <string.h>
#include <gio/gio.h>

#ifdef G_OS_UNIX
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#endif

#include "grl-upnp-utils.h"

/* 255 is the hardcoded limit in all interesting systems.
   It's actually 64 on linux.
*/
#define HOSTNAME_LENGTH 255

#ifdef G_OS_UNIX

static gboolean
is_our_ip_address (GInetAddress *address)
{
  /* The simplest way to determine if an address belongs
     to the current machine is to try and bind something.

     If no interface has that address, bind() will fail
     with EADDRNOTAVAIL.
  */

  GSocketAddress *sockaddr;
  struct sockaddr *native_sockaddr;
  gsize native_len;
  GSocket *socket;
  GError *error;
  gboolean ret = FALSE;
  int bound;

  sockaddr = g_inet_socket_address_new (address, 0);

  native_len = g_socket_address_get_native_size (sockaddr);
  native_sockaddr = g_alloca (native_len);
  g_socket_address_to_native (sockaddr, native_sockaddr, native_len, NULL);

  error = NULL;
  socket = g_socket_new (g_inet_address_get_family (address), G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, &error);
  if (socket == NULL)
    goto out;

  /* we need to resort to native bind() instead of g_socket_bind()
     because we want to check for EADDRNOTAVAIL, which is not covered
     by GIOError
  */
  bound = bind (g_socket_get_fd (socket), native_sockaddr, native_len);
  if (bound < 0)
    ret = (errno != EADDRNOTAVAIL);
  else
    ret = TRUE;

  g_socket_close (socket, NULL);
  g_object_unref (socket);

 out:
  g_clear_error (&error);
  g_object_unref (sockaddr);

  return ret;
}

gboolean
grl_upnp_util_uri_is_localhost (SoupURI *uri)
{
  char hostname_buffer[HOSTNAME_LENGTH+1];
  const char *host;
  GInetAddress *ip_address;
  gboolean ret;

  host = soup_uri_get_host (uri);
  if (host == NULL)
    return FALSE;

  gethostname (hostname_buffer, sizeof(hostname_buffer));
  if (strcmp (hostname_buffer, host) == 0)
    return TRUE;

  ip_address = g_inet_address_new_from_string (host);
  if (ip_address == NULL)
    return FALSE;

  ret = is_our_ip_address (ip_address);
  g_object_unref (ip_address);

  return ret;
}

#else

gboolean
grl_upnp_util_uri_is_localhost (SoupURI *uri)
{
  return FALSE;
}

#endif
