#
# configure.ac
# 
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L.. All rights reserved.

m4_define([prj_name],    [media-store])
m4_define([prj_version], [0.1.1-0])

AC_INIT([prj_name], [prj_version])

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

AC_CONFIG_SRCDIR([src])

AM_CONFIG_HEADER(src/config.h)

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

AM_PROG_CC_C_O

AC_HEADER_STDC

AM_MAINTAINER_MODE

CFLAGS="$CFLAGS -Wall -Werror -Wmissing-prototypes -Wmissing-declarations -std=c99"

# ----------------------------------------------------------
# DEFINITIONS
# ----------------------------------------------------------

# ABI version

MS_MAJORMINOR=0.1
AC_SUBST(MS_MAJORMINOR)
AC_DEFINE_UNQUOTED(MS_MAJORMINOR, "$MS_MAJORMINOR", [library major.minor version])

# Framework name (contains ABI version)

MS_NAME="${PACKAGE_NAME}-${MS_MAJORMINOR}"
AC_SUBST(MS_NAME)
AC_DEFINE_UNQUOTED(MS_NAME, "$MS_NAME", [framework name])

# Give parameters a default value

if test $prefix = "NONE" ; then
	prefix="/usr/local"
fi

if test $libdir = "\${exec_prefix}/lib" ; then
	libdir="$prefix/lib"
fi

# Plugins directory

PLUGINS_DIR=`pkg-config --variable=plugindir grilo-0.1`
AC_SUBST(PLUGINS_DIR)
AC_DEFINE_UNQUOTED(PLUGINS_DIR, "$PLUGINS_DIR", [Plugins directory])

# ----------------------------------------------------------
# DEPENDENCIES
# ----------------------------------------------------------

PKG_CHECK_MODULES(DEPS, glib-2.0 \
			gobject-2.0 \
			gmodule-2.0 \
			grilo-0.1)
AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

PKG_CHECK_MODULES(XML, libxml-2.0, HAVE_XML=yes, HAVE_XML=no)
AC_SUBST(XML_CFLAGS)
AC_SUBST(XML_LIBS)
AC_SUBST(HAVE_XML)
AM_CONDITIONAL(HAVE_XML, test "x$HAVE_XML" = "xyes")

PKG_CHECK_MODULES(GIO, gio-2.0, HAVE_GIO=yes, HAVE_GIO=no)
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)
AC_SUBST(HAVE_GIO)
AM_CONDITIONAL(HAVE_GIO, test "x$HAVE_GIO" = "xyes")

PKG_CHECK_MODULES(GUPNP, gupnp-1.0, HAVE_GUPNP=yes, HAVE_GUPNP=no)
AC_SUBST(GUPNP_CFLAGS)
AC_SUBST(GUPNP_LIBS)
AC_SUBST(HAVE_GUPNP)
AM_CONDITIONAL(HAVE_GUPNP, test "x$HAVE_GUPNP" = "xyes")

PKG_CHECK_MODULES(GUPNPAV, gupnp-av-1.0, HAVE_GUPNPAV=yes, HAVE_GUPNPAV=no)
AC_SUBST(GUPNPAV_CFLAGS)
AC_SUBST(GUPNPAV_LIBS)
AC_SUBST(HAVE_GUPNPAV)
AM_CONDITIONAL(HAVE_GUPNPAV, test "x$HAVE_GUPNPAV" = "xyes")

PKG_CHECK_MODULES(LIBCURL, libcurl, HAVE_LIBCURL=yes, HAVE_LIBCURL=no)
AC_SUBST(LIBCURL_CFLAGS)
AC_SUBST(LIBCURL_LIBS)
AC_SUBST(HAVE_LIBCURL)
AM_CONDITIONAL(HAVE_LIBCURL, test "x$HAVE_LIBCURL" = "xyes")

PKG_CHECK_MODULES(FLICKCURL, flickcurl >= 1.10, HAVE_FLICKCURL=yes, HAVE_FLICKCURL=no)
AC_SUBST(FLICKCURL_CFLAGS)
AC_SUBST(FLICKCURL_LIBS)
AC_SUBST(HAVE_FLICKCURL)
AM_CONDITIONAL(HAVE_FLICKCURL, test "x$HAVE_FLICKCURL" = "xyes")

# ----------------------------------------------------------
# UNINSTALLED SUPPORT
# ----------------------------------------------------------

AC_ARG_ENABLE(uninstalled,
        AC_HELP_STRING([--enable-uninstalled],
                [run uninstalled (default: no)]),,
        [enable_uninstalled=no])

AM_CONDITIONAL([UNINSTALLED], [test "x$enable_uninstalled" = "xyes"])

# ----------------------------------------------------------
# DEBUG SUPPORT
# ----------------------------------------------------------

AC_ARG_ENABLE(debug,
        AC_HELP_STRING([--enable-debug],
                [include debug symbols (default: no)]),,
        [enable_debug=no])

if test "x$enable_debug" = "xyes"; then
	CFLAGS="$CFLAGS -g3 -O0"
fi

AM_CONDITIONAL([DEBUG], [test "x$enable_debug" = "xyes"])

# ----------------------------------------------------------
# BUILD FAKE METADATA PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(fakemetadata,
        AC_HELP_STRING([--enable-fakemetadata],
                [enable Fake Metadata plugin (default: yes)]),,
        [enable_fakemetadata=yes])

AM_CONDITIONAL([FAKEMETADATA_PLUGIN], [test "x$enable_fakemetadata" = "xyes"])

# ----------------------------------------------------------
# BUILD FILE SYSTEM PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(filesystem,
        AC_HELP_STRING([--enable-filesystem],
                [enable File System plugin (default: yes)]),,
        [enable_filesystem=yes])

AM_CONDITIONAL([FILESYSTEM_PLUGIN], [test "x$enable_filesystem" = "xyes"])

# ----------------------------------------------------------
# BUILD JAMENDO PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(jamendo,
        AC_HELP_STRING([--enable-jamendo],
                [enable Jamendo plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GIO" = "xno"; then
                           AC_MSG_ERROR([GIO not found, install it or use --disable-jamendo])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([xml2 not found, install it or use --disable-jamendo])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GIO" = "xyes" -a "x$HAVE_XML" = "xyes"; then
                   enable_jamendo=yes
                else
                   enable_jamendo=no
                fi
        ])

AM_CONDITIONAL([JAMENDO_PLUGIN], [test "x$enable_jamendo" = "xyes"])

# ----------------------------------------------------------
# BUILD LASTFM ALBUM ART PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(lastfm_albumart,
        AC_HELP_STRING([--enable-lastfm-albumart],
                [enable Last.FM Album Art plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GIO" = "xno"; then
                           AC_MSG_ERROR([GIO not found, install it or use --disable-lastfm-albumart])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([xml2 not found, install it or use --disable-lastfm-albumart])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GIO" = "xyes" -a "x$HAVE_XML" = "xyes"; then
                   enable_lastfm_albumart=yes
                else
                   enable_lastfm_albumart=no
                fi
        ])

AM_CONDITIONAL([LASTFM_ALBUMART_PLUGIN], [test "x$enable_lastfm_albumart" = "xyes"])

# ----------------------------------------------------------
# BUILD UPNP PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(upnp,
        AC_HELP_STRING([--enable-upnp],
                [enable uPnP plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GUPNP" = "xno"; then
                           AC_MSG_ERROR([gupnp not found, install it or use --disable-upnp])
                        fi
                        if test "x$HAVE_GUPNPAV" = "xno"; then
                           AC_MSG_ERROR([gupnp-av not found, install it or use --disable-upnp])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GUPNP" = "xyes" -a "x$HAVE_GUPNPAV" = "xyes"; then
                   enable_upnp=yes
                else
                   enable_upnp=no
                fi
        ])

AM_CONDITIONAL([UPNP_PLUGIN], [test "x$enable_upnp" = "xyes"])

# ----------------------------------------------------------
# BUILD YOUTUBE PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(youtube,
        AC_HELP_STRING([--enable-youtube],
                [enable Youtube plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GIO" = "xno"; then
                           AC_MSG_ERROR([GIO not found, install it or use --disable-youtube])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([xml2 not found, install it or use --disable-youtube])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GIO" = "xyes" -a "x$HAVE_XML" = "xyes"; then
                   enable_youtube=yes
                else
                   enable_youtube=no
                fi
        ])

AM_CONDITIONAL([YOUTUBE_PLUGIN], [test "x$enable_youtube" = "xyes"])

# ----------------------------------------------------------
# BUILD FLICKR PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(flickr,
        AC_HELP_STRING([--flickr],
                [enable Flickr plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_FLICKCURL" = "xno"; then
                           AC_MSG_ERROR([flickcurl >= 1.10  not found, install it or use --disable-flickr])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([xml2 not found, install it or use --disable-flickr])
                        fi
                        if test "x$HAVE_LIBCURL" = "xno"; then
                           AC_MSG_ERROR([libcurl not found, install it or use --disable-flickr])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_FLICKCURL" = "xyes" -a "x$HAVE_XML" = "xyes" -a "x$HAVE_LIBCURL" = "xyes"; then
                   enable_flickr=yes
                else
                   enable_flickr=no
                fi
        ])

AM_CONDITIONAL([FLICKR_PLUGIN], [test "x$enable_flickr" = "xyes"])

# ----------------------------------------------------------
# GETTEXT
# ----------------------------------------------------------

#GETTEXT_PACKAGE=media-store
#AC_SUBST(GETTEXT_PACKAGE)
#AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package])

#AM_GLIB_GNU_GETTEXT

# ----------------------------------------------------------
# DEPENDENCIES
# ----------------------------------------------------------

# Framework

PKG_CHECK_MODULES(DEPS, glib-2.0 \
			gobject-2.0 \
			gmodule-2.0 \
			grilo-0.1)


AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

# Plugins requirements

PKG_CHECK_MODULES(VFS, gnome-vfs-2.0, HAVE_VFS=yes, HAVE_VFS=no)
AC_SUBST(VFS_CFLAGS)
AC_SUBST(VFS_LIBS)
AC_SUBST(HAVE_VFS)
AM_CONDITIONAL(HAVE_VFS, test "x$HAVE_VFS" = "xyes")

PKG_CHECK_MODULES(XML, libxml-2.0, HAVE_XML=yes, HAVE_XML=no)
AC_SUBST(XML_CFLAGS)
AC_SUBST(XML_LIBS)
AC_SUBST(HAVE_XML)
AM_CONDITIONAL(HAVE_XML, test "x$HAVE_XML" = "xyes")

PKG_CHECK_MODULES(GIO, gio-2.0, HAVE_GIO=yes, HAVE_GIO=no)
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)
AC_SUBST(HAVE_GIO)
AM_CONDITIONAL(HAVE_GIO, test "x$HAVE_GIO" = "xyes")

PKG_CHECK_MODULES(GUPNP, gupnp-1.0, HAVE_GUPNP=yes, HAVE_GUPNP=no)
AC_SUBST(GUPNP_CFLAGS)
AC_SUBST(GUPNP_LIBS)
AC_SUBST(HAVE_GUPNP)
AM_CONDITIONAL(HAVE_GUPNP, test "x$HAVE_GUPNP" = "xyes")

PKG_CHECK_MODULES(GUPNPAV, gupnp-av-1.0, HAVE_GUPNPAV=yes, HAVE_GUPNPAV=no)
AC_SUBST(GUPNPAV_CFLAGS)
AC_SUBST(GUPNPAV_LIBS)
AC_SUBST(HAVE_GUPNPAV)
AM_CONDITIONAL(HAVE_GUPNPAV, test "x$HAVE_GUPNPAV" = "xyes")

if test "x$enable_jamendo" = "xyes"; then
   if test "x$HAVE_GIO" = "xno"; then
      AC_MSG_ERROR([GIO not found, install it or use --disable-jamendo])
   fi
   if test "x$HAVE_XML" = "xno"; then
      AC_MSG_ERROR([xml2 not found, install it or use --disable-jamendo])
   fi
fi

if test "x$enable_lastfm_albumart" = "xyes"; then
   if test "x$HAVE_GIO" = "xno"; then
      AC_MSG_ERROR([GIO not found, install it or use --disable-lastfm-albumart])
   fi
   if test "x$HAVE_XML" = "xno"; then
      AC_MSG_ERROR([xml2 not found, install it or use --disable-lastfm-albumart])
   fi
fi

if test "x$enable_upnp" = "xyes"; then
   if test "x$HAVE_GUPNP" = "xno"; then
      AC_MSG_ERROR([gupnp not found, install it or use --disable-upnp])
   fi
   if test "x$HAVE_GUPNPAV" = "xno"; then
      AC_MSG_ERROR([gupnp-av not found, install it or use --disable-upnp])
   fi
fi

if test "x$enable_youtube" = "xyes"; then
   if test "x$HAVE_VFS" = "xno"; then
      AC_MSG_ERROR([gnome-vfs not found, install it or use --disable-youtube])
   fi
   if test "x$HAVE_XML" = "xno"; then
      AC_MSG_ERROR([xml2 not found, install it or use --disable-youtube])
   fi
fi

# ----------------------------------------------------------
# WORKAROUNDS
# ----------------------------------------------------------

AC_SUBST([abs_top_builddir])
AC_SUBST([builddir])

# ----------------------------------------------------------
# OUTPUT
# ----------------------------------------------------------

AC_CONFIG_FILES([
  Makefile
  media-store-0.1-uninstalled.pc
  src/Makefile
  src/youtube/Makefile
  src/fake-metadata/Makefile
  src/filesystem/Makefile
  src/jamendo/Makefile
  src/lastfm-albumart/Makefile
  src/upnp/Makefile
  src/flickr/Makefile
  test/Makefile
])

AC_OUTPUT
