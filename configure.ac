#
# configure.ac
# 
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L.. All rights reserved.

m4_define([prj_name],    [grilo-plugins])
m4_define([prj_version], [0.1.3])

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

# ----------------------------------------------------------
# ENVIRONMENT CONFIGURATION
# ----------------------------------------------------------

CFLAGS="$CFLAGS -Wall -Werror -Wmissing-prototypes -Wmissing-declarations -std=c99"

# ----------------------------------------------------------
# DEFINITIONS
# ----------------------------------------------------------

# ABI version

GRL_MAJORMINOR=0.1
AC_SUBST(GRL_MAJORMINOR)
AC_DEFINE_UNQUOTED(GRL_MAJORMINOR, "$GRL_MAJORMINOR", [library major.minor version])

# Give parameters a default value

if test $prefix = "NONE" ; then
	prefix="/usr/local"
fi

if test $libdir = "\${exec_prefix}/lib" ; then
	libdir="$prefix/lib"
fi

# Plugins directory

GRL_PLUGINS_DIR=`pkg-config --variable=plugindir grilo-0.1`
AC_SUBST(GRL_PLUGINS_DIR)
AC_DEFINE_UNQUOTED(GRL_PLUGINS_DIR, "$GRL_PLUGINS_DIR", [Plugins directory])

# ----------------------------------------------------------
# DEPENDENCIES
# ----------------------------------------------------------

PKG_CHECK_MODULES(DEPS, glib-2.0 \
			gobject-2.0 \
			gmodule-2.0 \
			grilo-0.1 >= 0.1.3)
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

PKG_CHECK_MODULES(SQLITE, sqlite3, HAVE_SQLITE=yes, HAVE_SQLITE=no)
AC_SUBST(SQLITE_CFLAGS)
AC_SUBST(SQLITE_LIBS)
AC_SUBST(HAVE_SQLITE)
AM_CONDITIONAL(HAVE_SQLITE, test "x$HAVE_SQLITE" = "xyes")

# ----------------------------------------------------------
# GUPNP-AV VERSION
# ----------------------------------------------------------

GUPNPAV_OLD_VERSION=no
AC_SUBST(GUPNPAV_OLD_VERSION)
if test "x$HAVE_GUPNPAV" = "xyes"; then
        gupnpav_version=`pkg-config --modversion gupnp-av-1.0`
        gupnpav_version_major=`echo $gupnpav_version | awk -F. '{ print $1 }'`
        gupnpav_version_minor=`echo $gupnpav_version | awk -F. '{ print $2 }'`
        if test $gupnpav_version_major -eq 0 -a $gupnpav_version_minor -lt 5; then
           GUPNPAV_OLD_VERSION=yes
        fi
        AC_SUBST(gupnpav_version_major)
        AC_SUBST(gupnpav_version_minor)
fi
AM_CONDITIONAL(GUPNPAV_OLD_VERSION, test "x$GUPNPAV_OLD_VERSION" = "xyes")

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
                [enable Fake Metadata plugin (default: no)]),,
        [enable_fakemetadata=no])

AM_CONDITIONAL([FAKEMETADATA_PLUGIN], [test "x$enable_fakemetadata" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL fakemetadata"
if test "x$enable_fakemetadata" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED fakemetadata"
fi

# ----------------------------------------------------------
# BUILD FILE SYSTEM PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(filesystem,
        AC_HELP_STRING([--enable-filesystem],
                [enable File System plugin (default: yes)]),,
        [enable_filesystem=yes])

AM_CONDITIONAL([FILESYSTEM_PLUGIN], [test "x$enable_filesystem" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL filesystem"
if test "x$enable_filesystem" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED filesystem"
fi

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
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL jamendo"
if test "x$enable_jamendo" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED jamendo"
fi

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
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL lastfm-albumart"
if test "x$enable_lastfm_albumart" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED lastfm-albumart"
fi

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
                        elif test "x$GUPNPAV_OLD_VERSION" = "xno" -a "$xHAVE_XML" = "xno"; then
                           AC_MSG_ERROR([xml2 not found, install it or use --disable-upnp])
                        fi
                esac
        ],
        [
                if test "x$HAVE_GUPNP" = "xyes" -a "x$HAVE_GUPNPAV" = "xyes"; then
                   if test "x$GUPNPAV_OLD_VERSION" = "xyes" -o "x$HAVE_XML" = "xyes"; then
                        enable_upnp=yes
                   else
                        enable_upnp=no
                   fi
                else
                   enable_upnp=no
                fi
        ])

AM_CONDITIONAL([UPNP_PLUGIN], [test "x$enable_upnp" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL upnp"
if test "x$enable_upnp" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED upnp"
fi

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
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL youtube"
if test "x$enable_youtube" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED youtube"
fi

# ----------------------------------------------------------
# BUILD FLICKR PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(flickr,
        AC_HELP_STRING([--enable-flickr],
                [enable Flickr plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GIO" = "xno"; then
                           AC_MSG_ERROR([GIO  not found, install it or use --disable-flickr])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([xml2 not found, install it or use --disable-flickr])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GIO" = "xyes" -a "x$HAVE_XML" = "xyes"; then
                   enable_flickr=yes
                else
                   enable_flickr=no
                fi
        ])

AM_CONDITIONAL([FLICKR_PLUGIN], [test "x$enable_flickr" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL flickr"
if test "x$enable_flickr" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED flickr"
fi

# ----------------------------------------------------------
# BUILD PODCASTS PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(podcasts,
        AC_HELP_STRING([--enable-podcasts],
                [enable Podcasts plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GIO" = "xno"; then
                           AC_MSG_ERROR([GIO not found, install it or use --disable-podcasts])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([xml2 not found, install it or use --disable-podcasts])
                        fi
                        if test "x$HAVE_SQLITE" = "xno"; then
                           AC_MSG_ERROR([sqlite3 not found, install it or use --disable-podcasts])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GIO" = "xyes" -a "x$HAVE_XML" = "xyes"; then
                   if test "x$HAVE_SQLITE" = "xyes"; then
                     enable_podcasts=yes
                   else
                     enable_podcasts=no
                   fi
                else
                   enable_podcasts=no
                fi
        ])

AM_CONDITIONAL([PODCASTS_PLUGIN], [test "x$enable_podcasts" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL podcasts"
if test "x$enable_podcasts" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED podcasts"
fi


# ----------------------------------------------------------
# BUILD BOOKMARKS PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(bookmarks,
        AC_HELP_STRING([--enable-bookmarks],
                [enable Bookmarks plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GIO" = "xno"; then
                           AC_MSG_ERROR([GIO not found, install it or use --disable-bookmarks])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([xml2 not found, install it or use --disable-bookmarks])
                        fi
                        if test "x$HAVE_SQLITE" = "xno"; then
                           AC_MSG_ERROR([sqlite3 not found, install it or use --disable-bookmarks])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GIO" = "xyes" -a "x$HAVE_XML" = "xyes"; then
                   if test "x$HAVE_SQLITE" = "xyes"; then
                     enable_bookmarks=yes
                   else
                     enable_bookmarks=no
                   fi
                else
                   enable_bookmarks=no
                fi
        ])

AM_CONDITIONAL([BOOKMARKS_PLUGIN], [test "x$enable_bookmarks" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL bookmarks"
if test "x$enable_bookmarks" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED bookmarks"
fi

# ----------------------------------------------------------
# BUILD SHOUTCAST PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(shoutcast,
        AC_HELP_STRING([--enable-shoutcast],
                [enable SHOUTcast plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GIO" = "xno"; then
                           AC_MSG_ERROR([GIO not found, install it or use --disable-shoutcast])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([xml2 not found, install it or use --disable-shoutcast])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GIO" = "xyes" -a "x$HAVE_XML" = "xyes"; then
                   enable_shoutcast=yes
                else
                   enable_shoutcast=no
                fi
        ])

AM_CONDITIONAL([SHOUTCAST_PLUGIN], [test "x$enable_shoutcast" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL shoutcast"
if test "x$enable_shoutcast" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED shoutcast"
fi

# ----------------------------------------------------------
# BUILD APPLE TRAILERS PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(apple_trailers,
        AC_HELP_STRING([--enable-apple-trailers],
                [enable Apple Movie Trailers plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GIO" = "xno"; then
                           AC_MSG_ERROR([GIO not found, install it or use --disable-apple-trailers])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([xml2 not found, install it or use --disable-apple-trailers])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GIO" = "xyes" -a "x$HAVE_XML" = "xyes"; then
                   enable_apple_trailers=yes
                else
                   enable_apple_trailers=no
                fi
        ])

AM_CONDITIONAL([APPLE_TRAILERS_PLUGIN], [test "x$enable_apple_trailers" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL apple-trailers"
if test "x$enable_apple_trailers" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED apple-trailers"
fi

# ----------------------------------------------------------
# BUILD METADATA-STORE PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(metadata-store,
        AC_HELP_STRING([--enable-metadata-store],
                [enable Metadata Store plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_SQLITE" = "xno"; then
                           AC_MSG_ERROR([sqlite3 not found, install it or use --disable-metadata-store])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_SQLITE" = "xyes"; then
                  enable_metadata_store=yes
                else
                  enable_metadata_store=no
                fi
        ])

AM_CONDITIONAL([METADATA_STORE_PLUGIN], [test "x$enable_metadata_store" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL metadata-store"
if test "x$enable_metadata_store" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED metadata-store"
fi

# ----------------------------------------------------------
# GETTEXT
# ----------------------------------------------------------

#GETTEXT_PACKAGE=grilo
#AC_SUBST(GETTEXT_PACKAGE)
#AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package])

#AM_GLIB_GNU_GETTEXT

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
  src/Makefile
  src/youtube/Makefile
  src/fake-metadata/Makefile
  src/filesystem/Makefile
  src/jamendo/Makefile
  src/lastfm-albumart/Makefile
  src/upnp/Makefile
  src/flickr/Makefile
  src/podcasts/Makefile
  src/bookmarks/Makefile
  src/shoutcast/Makefile
  src/apple-trailers/Makefile
  src/metadata-store/Makefile
  test/Makefile
])

AC_OUTPUT

AG_GRL_OUTPUT_PLUGINS
