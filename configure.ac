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
# UNINSTALLED SUPPORT
# ----------------------------------------------------------

AC_ARG_ENABLE(uninstalled,
        [  --enable-uninstalled run uninstalled],,
        enable_uninstalled=no)
if test "x$enable_uninstalled" = "xyes"; then
        AC_MSG_WARN("Configuring for running uninstalled!")
fi

AM_CONDITIONAL([UNINSTALLED], [test "x$enable_uninstalled" = "xyes"])

# ----------------------------------------------------------
# DEBUG SUPPORT
# ----------------------------------------------------------

AC_ARG_ENABLE(debug,
        [  --enable-debug include debug symbols],,
        enable_debug=yes)
if test "x$enable_debug" = "xyes"; then
        AC_MSG_WARN("Configuring with debugging options enabled!")
	CFLAGS="$CFLAGS -g3 -O0"
fi

AM_CONDITIONAL([DEBUG], [test "x$enable_debug" = "xyes"])

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
  test/Makefile
])

AC_OUTPUT
