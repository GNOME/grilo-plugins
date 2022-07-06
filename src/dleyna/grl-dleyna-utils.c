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
#include <stdlib.h>
#include <gio/gio.h>

#ifdef G_OS_UNIX
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#ifdef __linux__
#include <netinet/in.h>
#include <netinet/ip.h>
#endif
#endif

#include "grl-dleyna-utils.h"

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

#ifdef __linux__
static gboolean
is_our_user_ipv4 (struct sockaddr_in *address)
{
  GIOChannel *file;
  gboolean found;
  uid_t uid;
  gboolean ret;
  GIOStatus status;
  gchar *line;

  ret = FALSE;
  file = g_io_channel_new_file ("/proc/net/tcp", "r", NULL);
  if (file == NULL)
    return FALSE;

  found = FALSE;
  /* skip first line, it's the header */
  status = g_io_channel_read_line (file, &line, NULL, NULL, NULL);
  g_free (line);
  if (status != G_IO_STATUS_NORMAL)
    goto out;

  status = g_io_channel_read_line (file, &line, NULL, NULL, NULL);
  while (status == G_IO_STATUS_NORMAL) {
    int j, k, l;
    /* 8 for IP, 4 for port, 1 for :, 1 for NUL */
    char buffer[8 + 4 + 1 + 1];
    guint32 ip;
    guint16 port;

    j = 0;

    /* skip leading space */
    while (line[j] == ' ')
      j++;

    /* skip the first field */
    while (line[j] != ' ' && line[j] != '\0')
      j++;
    if (line[j] == '\0' )
      continue;
    while (line[j] == ' ')
      j++;

    strncpy(buffer, line + j, sizeof(buffer));
    buffer[8+4+1] = 0;
    buffer[8] = 0;
    j += 8+4+1;

    /* the IP is in network byte order
       (so 127.0.0.1 is 0100007F)
    */
    ip = strtoul(buffer, NULL, 16);
    port = htons(strtoul(buffer+8+1, NULL, 16));

    if ((ip == 0 || ip == address->sin_addr.s_addr) &&
        port == address->sin_port) {
      /* skip rem_address, st, tx_queue+rx_queue, tr+tm->when, retrnsmt */
      while (line[j] == ' ')
        j++;
      for (k = 0; k < 5; k++) {
        while (line[j] != ' ')
          j++;
        while (line[j] == ' ')
          j++;
      }

      strncpy(buffer, line + j, sizeof(buffer));
      buffer[sizeof(buffer)-1] = 0;
      l = 0;
      while (buffer[l] != ' ' && buffer[l] != 0)
        l++;
      buffer[l] = 0;

      uid = strtoul(buffer, NULL, 0);

      found = TRUE;
      break;
    }

    g_free (line);
    status = g_io_channel_read_line (file, &line, NULL, NULL, NULL);
  }

  if (found)
    ret = uid == getuid();

 out:
  g_io_channel_unref (file);
  return ret;
}

static gboolean
is_our_user_ipv6 (struct sockaddr_in6 *address)
{
  GIOChannel *file;
  gboolean found;
  uid_t uid;
  gboolean ret;
  GIOStatus status;
  gchar *line;

  ret = FALSE;
  file = g_io_channel_new_file ("/proc/net/tcp", "r", NULL);
  if (file == NULL)
    return FALSE;

  found = FALSE;
  /* skip first line, it's the header */
  status = g_io_channel_read_line (file, &line, NULL, NULL, NULL);
  g_free (line);
  if (status != G_IO_STATUS_NORMAL)
    goto out;

  status = g_io_channel_read_line (file, &line, NULL, NULL, NULL);
  while (status == G_IO_STATUS_NORMAL) {
    int j, k, l;
    /* 4*8 for IP, 4 for port, 1 for :, 1 for NUL */
    char buffer[4*8 + 4 + 1 + 1];
    guint32 ip[4];
    guint16 port;
    guint32 all_ipv6[4] = { 0, 0, 0, 0 };

    j = 0;

    /* skip leading space */
    while (line[j] == ' ')
      j++;

    /* skip the first field */
    while (line[j] != ' ' && line[j] != '\0')
      j++;
    if (line[j] == '\0' )
      continue;
    while (line[j] == ' ')
      j++;

    strncpy(buffer, line + j, sizeof(buffer));
    buffer[4*8+4+1] = 0;
    buffer[4*8] = 0;
    j += 4*8+4+1;

    for (k = 0; k < 4; k++) {
      /* the IP is written as 4 uint32 units, each in network
         byte order
      */
      char c;
      c = buffer[8 * k];
      buffer[8 * k] = 0;
      ip[k] = strtoul(buffer, NULL, 16);
      buffer[8 * k] = c;
    }
    port = htons(strtoul(buffer+4*8+1, NULL, 16));

    if ((memcmp (ip, all_ipv6, sizeof(ip)) == 0 ||
         memcmp (ip, address->sin6_addr.s6_addr, sizeof(ip)) == 0) &&
        port == address->sin6_port) {
      /* skip remote_address, st, tx_queue+rx_queue, tr+tm->when, retrnsmt */
      while (line[j] == ' ')
        j++;
      for (k = 0; k < 5; k++) {
        while (line[j] != ' ')
          j++;
        while (line[j] == ' ')
          j++;
      }

      strncpy(buffer, line + j, sizeof(buffer));
      buffer[sizeof(buffer)-1] = 0;
      l = 0;
      while (buffer[l] != ' ' && buffer[l] != 0)
        l++;
      buffer[l] = 0;

      uid = strtoul(buffer, NULL, 0);

      found = TRUE;
      break;
    }

    g_free (line);
    status = g_io_channel_read_line (file, &line, NULL, NULL, NULL);
  }

  if (found)
    ret = uid == getuid();

 out:
  g_io_channel_unref (file);
  return ret;
}

static gboolean
is_our_user (GSocketAddress *sockaddr)
{
  struct sockaddr *native_sockaddr;
  gsize native_len;
  gboolean ret;

  native_len = g_socket_address_get_native_size (sockaddr);
  native_sockaddr = g_alloca (native_len);
  g_socket_address_to_native (sockaddr, native_sockaddr, native_len, NULL);

  if (native_sockaddr->sa_family == AF_INET) {
    ret = is_our_user_ipv4 ((struct sockaddr_in*) native_sockaddr);

    if (!ret) {
      /* try also an ipv6 mapped ipv4 */
      struct sockaddr_in6 ipv6;
      ipv6.sin6_family = AF_INET6;
      ipv6.sin6_port = ((struct sockaddr_in*) native_sockaddr)->sin_port;
      ipv6.sin6_flowinfo = 0;

      memset (ipv6.sin6_addr.s6_addr, 0, 12);
      memcpy (ipv6.sin6_addr.s6_addr + 12, &((struct sockaddr_in*) native_sockaddr)->sin_port, 4);

      return is_our_user_ipv6 (&ipv6);
    } else
      return ret;
  } else
    return is_our_user_ipv6 ((struct sockaddr_in6*) native_sockaddr);
}
#else
static gboolean
is_our_user (GSocketAddress *address)
{
  return FALSE;
}
#endif

void
grl_dleyna_util_uri_is_localhost (const gchar *uri,
                                  gboolean *localuser,
                                  gboolean *localhost)
{
  char hostname_buffer[HOSTNAME_LENGTH+1];
  const char *host;
  GInetAddress *ip_address;
  g_autoptr(GUri) g_uri = NULL;

  g_uri = g_uri_parse (uri,
                       G_URI_FLAGS_HAS_PASSWORD |
                       G_URI_FLAGS_ENCODED_PATH |
                       G_URI_FLAGS_ENCODED_QUERY |
                       G_URI_FLAGS_ENCODED_FRAGMENT |
                       G_URI_FLAGS_SCHEME_NORMALIZE, NULL);

  host = g_uri_get_host (g_uri);
  if (host == NULL) {
    *localhost = FALSE;
    *localuser = FALSE;
    return;
  }

  gethostname (hostname_buffer, sizeof(hostname_buffer));
  if (strcmp (hostname_buffer, host) == 0) {
    GList *addresses;
    GSocketAddress *sockaddr;

    addresses = g_resolver_lookup_by_name (g_resolver_get_default (), host, NULL, NULL);
    if (addresses == NULL) {
      *localhost = FALSE;
      *localuser = FALSE;
      return;
    }

    *localhost = TRUE;

    sockaddr = G_SOCKET_ADDRESS (g_inet_socket_address_new (addresses->data, g_uri_get_port (g_uri)));
    *localuser = is_our_user (sockaddr);

    g_object_unref (sockaddr);
    g_list_free_full (addresses, g_object_unref);
    return;
  }

  ip_address = g_inet_address_new_from_string (host);
  if (ip_address == NULL) {
    *localhost = FALSE;
    *localuser = FALSE;
    return;
  }

  *localhost = is_our_ip_address (ip_address);
  if (*localhost) {
    GSocketAddress *sockaddr;

    sockaddr = G_SOCKET_ADDRESS (g_inet_socket_address_new (ip_address, g_uri_get_port (g_uri)));
    *localuser = is_our_user (sockaddr);

    g_object_unref (sockaddr);
  } else {
    *localuser = FALSE;
  }

  g_object_unref (ip_address);
}

#else

void
grl_dleyna_util_uri_is_localhost (const gchar *uri,
                                  gboolean *localhost,
                                  gboolean *localuser)
{
  *localhost = FALSE;
  *localuser = FALSE;
}

#endif
