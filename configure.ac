#
# configure.ac
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L.. All rights reserved.

m4_define([prj_name],    [grilo-plugins])
m4_define([prj_version], [0.2.10])

AC_INIT([prj_name],
        [prj_version],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=grilo;component=plugins],
        [prj_name],
        [https://wiki.gnome.org/Projects/Grilo])

AM_INIT_AUTOMAKE([foreign])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src])

AC_CONFIG_HEADERS(config.h)

AC_PROG_CC
AC_PROG_INSTALL
AC_DISABLE_STATIC

if test -z "$PKG_CONFIG" ; then
	PKG_CONFIG="pkg-config"
	if test -n "$host" ; then
		PKG_CONFIG=$host-$PKG_CONFIG
	fi
fi

LT_PREREQ([2.2.6])
LT_INIT

AM_PROG_CC_C_O

AC_HEADER_STDC

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# ----------------------------------------------------------
# ENVIRONMENT CONFIGURATION
# ----------------------------------------------------------

GNOME_COMPILE_WARNINGS([maximum])
CFLAGS+=" ${WARN_CFLAGS} -Wmissing-declarations -std=c99 "

GNOME_MAINTAINER_MODE_DEFINES

# ----------------------------------------------------------
# UNINSTALLED SUPPORT
# ----------------------------------------------------------

AC_ARG_ENABLE(uninstalled,
        AC_HELP_STRING([--enable-uninstalled],
                [run uninstalled (default: no)]),,
        [enable_uninstalled=no])

if test "x$enable_uninstalled" = "xyes"; then
        AC_MSG_WARN("Configuring for running uninstalled!")
	GRL_VERSION=uninstalled
else
	GRL_VERSION=0.2
fi

GRL_NAME=grilo-${GRL_VERSION}
GRLNET_NAME=grilo-net-${GRL_VERSION}
GRLPLS_NAME=grilo-pls-${GRL_VERSION}

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

# ----------------------------------------------------------
# HARD DEPENDENCIES
# ----------------------------------------------------------

PKG_CHECK_MODULES([DEPS], glib-2.0 >= 2.32      \
			  gobject-2.0   \
			  gmodule-2.0   \
			  ${GRL_NAME} >= 0.2.8)

GLIB_COMPILE_RESOURCES=`$PKG_CONFIG --variable glib_compile_resources gio-2.0`
AC_SUBST(GLIB_COMPILE_RESOURCES)

# ----------------------------------------------------------
# DEFINITIONS
# ----------------------------------------------------------

# Plugins directory

GRL_PLUGINS_DIR=`${PKG_CONFIG} --variable=plugindir ${GRL_NAME}`
AC_SUBST(GRL_PLUGINS_DIR)
AC_DEFINE_UNQUOTED(GRL_PLUGINS_DIR, "$GRL_PLUGINS_DIR", [Plugins directory])

# ----------------------------------------------------------
# EXTERNAL/OPTIONAL DEPENDENCIES
# ----------------------------------------------------------

PKG_CHECK_MODULES([GRLNET], [${GRLNET_NAME} >= 0.2.2],
			    HAVE_GRLNET=yes,
			    HAVE_GRLNET=no)
PKG_CHECK_MODULES([GRLPLS], [${GRLPLS_NAME} >= 0.2.0],
			    HAVE_GRLPLS=yes,
			    HAVE_GRLPLS=no)

PKG_CHECK_MODULES([GRLPLS], [${GRLPLS_NAME} >= 0.2.0],
			    HAVE_GRLPLS=yes,
			    HAVE_GRLPLS=no)

PKG_CHECK_MODULES(XML, libxml-2.0, HAVE_XML=yes, HAVE_XML=no)

PKG_CHECK_MODULES(GIO, gio-2.0, HAVE_GIO=yes, HAVE_GIO=no)

PKG_CHECK_MODULES(GSSDP, gssdp-1.0, HAVE_GSSDP=yes, HAVE_GSSDP=no)

PKG_CHECK_MODULES(GUPNP, gupnp-1.0 >= 0.13, HAVE_GUPNP=yes, HAVE_GUPNP=no)

PKG_CHECK_MODULES(GUPNPAV, gupnp-av-1.0 >= 0.5, HAVE_GUPNPAV=yes, HAVE_GUPNPAV=no)

PKG_CHECK_MODULES(DMAP, libdmapsharing-3.0 >= 2.9.12, HAVE_DMAP=yes, HAVE_DMAP=no)

PKG_CHECK_MODULES(SQLITE, sqlite3, HAVE_SQLITE=yes, HAVE_SQLITE=no)

PKG_CHECK_MODULES(GDATA, libgdata >= 0.7.0, HAVE_GDATA=yes, HAVE_GDATA=no)

PKG_CHECK_MODULES(LIBSOUP, libsoup-2.4, HAVE_LIBSOUP=yes, HAVE_LIBSOUP=no)

PKG_CHECK_MODULES(GTHREAD, gthread-2.0, HAVE_GTHREAD=yes, HAVE_GTHREAD=no)

PKG_CHECK_MODULES(OAUTH, oauth, HAVE_OAUTH=yes, HAVE_OAUTH=no)

PKG_CHECK_MODULES(GOA, [goa-1.0 >= 3.7.1], HAVE_GOA=yes, HAVE_GOA=no)

PKG_CHECK_MODULES(TOTEM_PL_PARSER, totem-plparser >= 3.4.1, HAVE_TOTEM_PL_PARSER=yes, HAVE_TOTEM_PL_PARSER=no)

PKG_CHECK_MODULES([JSON], [json-glib-1.0], HAVE_JSON_GLIB=yes, HAVE_JSON_GLIB=no)

PKG_CHECK_MODULES(GMIME, gmime-2.6,
                         HAVE_GMIME=yes,
                         [PKG_CHECK_MODULES(GMIME, gmime-2.4,
                                                   HAVE_GMIME=yes,
                                                   HAVE_GMIME=no)])

PKG_CHECK_MODULES(AVAHI, avahi-gobject, HAVE_AVAHI=yes, HAVE_AVAHI=no)

# Search available Tracker version
trackerpkg="tracker-sparql-0.10"
m4_foreach([VERSION], [[0.10], [0.11], [0.12], [0.14], [0.15], [0.16], [0.17], [0.18]],
                      [PKG_CHECK_EXISTS([tracker-sparql-VERSION >= 0.10.5],
                                        [ trackerpkg="tracker-sparql-VERSION" ])
                      ])

PKG_CHECK_MODULES([TRACKER_SPARQL], [ ${trackerpkg} ],
                                    HAVE_TRACKER_SPARQL=yes,
                                    HAVE_TRACKER_SPARQL=no)


AM_PATH_LIBGCRYPT(,HAVE_GCRYPT=yes, HAVE_GCRYPT=no)


# ----------------------------------------------------------
# CHECK GOA
# ----------------------------------------------------------
AC_ARG_ENABLE(goa,
        AC_HELP_STRING([--enable-goa],
                [enable Gnome Online Accounts support (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GOA" = "xno"; then
                           AC_MSG_ERROR([goa-1.0 >= 3.7.1 not found, install it or use --disable-goa])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GOA" = "xyes"; then
                   enable_goa=yes
                else
                   enable_goa=no
                fi
        ])

if test "x$enable_goa" = "xyes"; then
        AC_DEFINE(GOA_ENABLED, 1, [Let plugins use goa])
fi

# ----------------------------------------------------------
# GDATA VERSION
# ----------------------------------------------------------

if test "x$HAVE_GDATA" = "xyes"; then
   if `$PKG_CONFIG --atleast-version=0.9.1 libgdata`; then
      AC_DEFINE(HAVE_LIBGDATA_0_9, 1, [libgdata is 0.9.1 or higher])
   fi
fi

# ----------------------------------------------------------
# TESTS COVERAGE
# ----------------------------------------------------------

AC_ARG_ENABLE(gcov,
        AC_HELP_STRING([--enable-gcov],
                [enable coverage testing (default: no)]),
        [enable_gcov=$enableval],
        [enable_gcov=no])

if test "x$enable_gcov" = "xyes"; then
   AC_PATH_PROG([enable_gcov], [lcov], [no])
   if test "x$enable_gcov" = "xno"; then
      AC_MSG_ERROR([You need lcov to build with coverage testing])
   elif test "x$GCC" = "xno"; then
      AC_MSG_ERROR([You need to use gcc to build with coverage testing])
   else
      enable_gcov=yes
      CFLAGS="$CFLAGS -g -O0 -fprofile-arcs -ftest-coverage"
      LDFLAGS="$LDFLAGS -lgcov"
   fi
fi

AM_CONDITIONAL([GCOV_ENABLED], [test "x$enable_gcov" = "xyes"])

# ----------------------------------------------------------
# BUILD FILE SYSTEM PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(filesystem,
        AC_HELP_STRING([--enable-filesystem],
                [enable File System plugin (default: yes)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GRLPLS" = "xno"; then
                           AC_MSG_ERROR([grilo-pls not found, install it or use --disable-filesystem])
                        fi
                        ;;
                esac
        ],
        [enable_filesystem=yes])

AM_CONDITIONAL([FILESYSTEM_PLUGIN], [test "x$enable_filesystem" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL filesystem"
if test "x$enable_filesystem" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED filesystem"
fi

FILESYSTEM_PLUGIN_ID="grl-filesystem"
AC_SUBST(FILESYSTEM_PLUGIN_ID)
AC_DEFINE_UNQUOTED([FILESYSTEM_PLUGIN_ID], ["$FILESYSTEM_PLUGIN_ID"], [Filesystem plugin ID])

DEPS_FILESYSTEM_CFLAGS="$DEPS_CFLAGS $GRLPLS_CFLAGS $GIO_CFLAGS"
AC_SUBST(DEPS_FILESYSTEM_CFLAGS)
DEPS_FILESYSTEM_LIBS="$DEPS_LIBS $GRLPLS_LIBS $GIO_LIBS"
AC_SUBST(DEPS_FILESYSTEM_LIBS)

# ----------------------------------------------------------
# BUILD OPTICAL MEDIA PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(optical-media,
        AC_HELP_STRING([--enable-optical-media],
                [enable Optical Media plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_TOTEM_PL_PARSER" = "xno"; then
                           AC_MSG_ERROR([totem-plparser >= 3.4.1 not found, install it or use --disable-optical-media])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_TOTEM_PL_PARSER" = "xyes"; then
                   enable_optical_media=yes
                else
                   enable_optical_media=no
                fi
        ])

AM_CONDITIONAL([OPTICAL_MEDIA_PLUGIN], [test "x$enable_optical_media" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL optical-media"
if test "x$enable_optical_media" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED optical-media"
fi

OPTICAL_MEDIA_PLUGIN_ID="grl-optical-media"
AC_SUBST(OPTICAL_MEDIA_PLUGIN_ID)
AC_DEFINE_UNQUOTED([OPTICAL_MEDIA_PLUGIN_ID], ["$OPTICAL_MEDIA_PLUGIN_ID"], [Optical Media plugin ID])

DEPS_OPTICAL_MEDIA_CFLAGS="$DEPS_CFLAGS $GIO_CFLAGS $GTK_CFLAGS $TOTEM_PL_PARSER_CFLAGS"
AC_SUBST(DEPS_OPTICAL_MEDIA_CFLAGS)
DEPS_OPTICAL_MEDIA_LIBS="$DEPS_LIBS $GIO_LIBS $GTK_LIBS $TOTEM_PL_PARSER_LIBS"
AC_SUBST(DEPS_OPTICAL_MEDIA_LIBS)

# ----------------------------------------------------------
# BUILD JAMENDO PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(jamendo,
        AC_HELP_STRING([--enable-jamendo],
                [enable Jamendo plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GRLNET" = "xno"; then
                           AC_MSG_ERROR([grilo-net-0.2 >= 0.2.2 not found, install it or use --disable-jamendo])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([libxml-2.0 not found, install it or use --disable-jamendo])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GRLNET" = "xyes" -a "x$HAVE_XML" = "xyes"; then
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

JAMENDO_PLUGIN_ID="grl-jamendo"
AC_SUBST(JAMENDO_PLUGIN_ID)
AC_DEFINE_UNQUOTED([JAMENDO_PLUGIN_ID], ["$JAMENDO_PLUGIN_ID"], [Jamendo plugin ID])

DEPS_JAMENDO_CFLAGS="$DEPS_CFLAGS $GRLNET_CFLAGS $XML_CFLAGS"
AC_SUBST(DEPS_JAMENDO_CFLAGS)
DEPS_JAMENDO_LIBS="$DEPS_LIBS $GRLNET_LIBS $XML_LIBS"
AC_SUBST(DEPS_JAMENDO_LIBS)

# ----------------------------------------------------------
# BUILD LASTFM ALBUM ART PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(lastfm_albumart,
        AC_HELP_STRING([--enable-lastfm-albumart],
                [enable Last.FM Album Art plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GRLNET" = "xno"; then
                           AC_MSG_ERROR([grilo-net-0.2 >= 0.2.2 not found, install it or use --disable-lastfm-albumart])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([libxml-2.0 not found, install it or use --disable-lastfm-albumart])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GRLNET" = "xyes" -a "x$HAVE_XML" = "xyes"; then
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

LASTFM_ALBUMART_PLUGIN_ID="grl-lastfm-albumart"
AC_SUBST(LASTFM_ALBUMART_PLUGIN_ID)
AC_DEFINE_UNQUOTED([LASTFM_ALBUMART_PLUGIN_ID], ["$LASTFM_ALBUMART_PLUGIN_ID"], [Last.fm album art plugin ID])

DEPS_LASTFM_ALBUMART_CFLAGS="$DEPS_CFLAGS $GRLNET_CFLAGS $XML_CFLAGS"
AC_SUBST(DEPS_LASTFM_ALBUMART_CFLAGS)
DEPS_LASTFM_ALBUMART_LIBS="$DEPS_LIBS $GRLNET_LIBS $XML_LIBS"
AC_SUBST(DEPS_LASTFM_ALBUMART_LIBS)

# ----------------------------------------------------------
# BUILD UPNP PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(upnp,
        AC_HELP_STRING([--enable-upnp],
                [enable uPnP plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GSSDP" = "xno"; then
                           AC_MSG_ERROR([gssdp-1.0 not found, install it or use --disable-upnp])
                        fi
                        if test "x$HAVE_GUPNP" = "xno"; then
                           AC_MSG_ERROR([gupnp-1.0 >= 0.13 not found, install it or use --disable-upnp])
                        fi
                        if test "x$HAVE_GUPNPAV" = "xno"; then
                           AC_MSG_ERROR([gupnp-av-1.0 >= 0.5 not found, install it or use --disable-upnp])
                        fi
                        if test "$xHAVE_XML" = "xno"; then
                           AC_MSG_ERROR([libxml-2.0 not found, install it or use --disable-upnp])
                        fi
                esac
        ],
        [
                if test "x$HAVE_GSSDP" = "xyes" -a "x$HAVE_GUPNP" = "xyes" -a "x$HAVE_GUPNPAV" = "xyes" -a "x$HAVE_XML" = "xyes"; then
                        enable_upnp=yes
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

UPNP_PLUGIN_ID="grl-upnp"
AC_SUBST(UPNP_PLUGIN_ID)
AC_DEFINE_UNQUOTED([UPNP_PLUGIN_ID], ["$UPNP_PLUGIN_ID"], [UPnP plugin ID])

DEPS_UPNP_CFLAGS="$DEPS_CFLAGS $GSSDP_CFLAGS $GUPNP_CFLAGS $GUPNPAV_CFLAGS $GTHREAD_CFLAGS $XML_CFLAGS"
AC_SUBST(DEPS_UPNP_CFLAGS)
DEPS_UPNP_LIBS="$DEPS_LIBS $GSSDP_LIBS $GUPNP_LIBS $GUPNPAV_LIBS $GTHREAD_LIBS $XML_LIBS"
AC_SUBST(DEPS_UPNP_LIBS)

# ----------------------------------------------------------
# BUILD YOUTUBE PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(youtube,
        AC_HELP_STRING([--enable-youtube],
                [enable YouTube plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GRLNET" = "xno"; then
                           AC_MSG_ERROR([grilo-net-0.2 >= 0.2.2  not found, install it or use --disable-youtube])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([libxml-2.0 not found, install it or use --disable-youtube])
                        fi
                        if test "x$HAVE_GDATA" = "xno"; then
                           AC_MSG_ERROR([libgdata >= 0.7.0 not found, install it or use --disable-youtube])
                        fi
                        if test "x$HAVE_TOTEM_PL_PARSER" = "xno"; then
                           AC_MSG_ERROR([totem-plparser >= 3.4.1 not found, install it or use --disable-youtube])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GRLNET" = "xyes" -a "x$HAVE_XML" = "xyes" -a "x$HAVE_GDATA" = "xyes" -a "x$HAVE_TOTEM_PL_PARSER" = "xyes"; then
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

YOUTUBE_PLUGIN_ID="grl-youtube"
AC_SUBST(YOUTUBE_PLUGIN_ID)
AC_DEFINE_UNQUOTED([YOUTUBE_PLUGIN_ID], ["$YOUTUBE_PLUGIN_ID"], [YouTube plugin ID])

DEPS_YOUTUBE_CFLAGS="$DEPS_CFLAGS $GRLNET_CFLAGS $XML_CFLAGS $GTHREAD_CFLAGS $GDATA_CFLAGS $TOTEM_PL_PARSER_CFLAGS"
AC_SUBST(DEPS_YOUTUBE_CFLAGS)
DEPS_YOUTUBE_LIBS="$DEPS_LIBS $GRLNET_LIBS $XML_LIBS $GTHREAD_LIBS $GDATA_LIBS $TOTEM_PL_PARSER_LIBS"
AC_SUBST(DEPS_YOUTUBE_LIBS)

# ----------------------------------------------------------
# BUILD FLICKR PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(flickr,
        AC_HELP_STRING([--enable-flickr],
                [enable Flickr plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GRLNET" = "xno"; then
                           AC_MSG_ERROR([grilo-net-0.2 >= 0.2.2 not found, install it or use --disable-flickr])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([libxml-2.0 not found, install it or use --disable-flickr])
                        fi
                        if test "x$HAVE_OAUTH" = "xno"; then
                           AC_MSG_ERROR([oauth not found, install it or use --disable-flickr])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GRLNET" = "xyes" -a "x$HAVE_XML" = "xyes" -a "x$HAVE_OAUTH" = "xyes"; then
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

FLICKR_PLUGIN_ID="grl-flickr"
AC_SUBST(FLICKR_PLUGIN_ID)
AC_DEFINE_UNQUOTED([FLICKR_PLUGIN_ID], ["$FLICKR_PLUGIN_ID"], [Flickr plugin ID])

DEPS_FLICKR_CFLAGS="$DEPS_CFLAGS $XML_CFLAGS $GRLNET_CFLAGS $OAUTH_CFLAGS"
if test "x$enable_goa" = "xyes"; then
	DEPS_FLICKR_CFLAGS="$DEPS_FLICKR_CFLAGS $GOA_CFLAGS"
fi
AC_SUBST(DEPS_FLICKR_CFLAGS)


DEPS_FLICKR_LIBS="$DEPS_LIBS $XML_LIBS $GRLNET_LIBS $OAUTH_LIBS"
if test "x$enable_goa" = "xyes"; then
	DEPS_FLICKR_LIBS="$DEPS_FLICKR_LIBS $GOA_LIBS"
fi
AC_SUBST(DEPS_FLICKR_LIBS)

# ----------------------------------------------------------
# BUILD PODCASTS PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(podcasts,
        AC_HELP_STRING([--enable-podcasts],
                [enable Podcasts plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GRLNET" = "xno"; then
                           AC_MSG_ERROR([grilo-net-0.2 >= 0.2.2 not found, install it or use --disable-podcasts])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([libxml-2.0 not found, install it or use --disable-podcasts])
                        fi
                        if test "x$HAVE_SQLITE" = "xno"; then
                           AC_MSG_ERROR([sqlite3 not found, install it or use --disable-podcasts])
                        fi
                        if test "x$HAVE_GMIME" = "xno"; then
                           AC_MSG_ERROR([gmime-2.4 or gmime-2.6 not found, install it or use --disable-podcasts])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GRLNET" = "xyes" -a \
                        "x$HAVE_XML" = "xyes" -a \
                        "x$HAVE_SQLITE" = "xyes" -a \
                        "x$HAVE_GMIME" = "xyes"; then
                   enable_podcasts=yes
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

PODCASTS_PLUGIN_ID="grl-podcasts"
AC_SUBST(PODCASTS_PLUGIN_ID)
AC_DEFINE_UNQUOTED([PODCASTS_PLUGIN_ID], ["$PODCASTS_PLUGIN_ID"], [Podcasts plugin ID])

DEPS_PODCASTS_CFLAGS="$DEPS_CFLAGS $GRLNET_CFLAGS $XML_CFLAGS $SQLITE_CFLAGS $GMIME_CFLAGS"
AC_SUBST(DEPS_PODCASTS_CFLAGS)
DEPS_PODCASTS_LIBS="$DEPS_LIBS $GRLNET_LIBS $XML_LIBS $SQLITE_LIBS $GMIME_LIBS"
AC_SUBST(DEPS_PODCASTS_LIBS)

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
                           AC_MSG_ERROR([gio-2.0 not found, install it or use --disable-bookmarks])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([libxml-2.0 not found, install it or use --disable-bookmarks])
                        fi
                        if test "x$HAVE_SQLITE" = "xno"; then
                           AC_MSG_ERROR([sqlite3 not found, install it or use --disable-bookmarks])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GIO" = "xyes" -a "x$HAVE_XML" = "xyes" -a "x$HAVE_SQLITE" = "xyes"; then
                   enable_bookmarks=yes
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

BOOKMARKS_PLUGIN_ID="grl-bookmarks"
AC_SUBST(BOOKMARKS_PLUGIN_ID)
AC_DEFINE_UNQUOTED([BOOKMARKS_PLUGIN_ID], ["$BOOKMARKS_PLUGIN_ID"], [Bookmarks plugin ID])

DEPS_BOOKMARKS_CFLAGS="$DEPS_CFLAGS $SQLITE_CFLAGS"
AC_SUBST(DEPS_BOOKMARKS_CFLAGS)
DEPS_BOOKMARKS_LIBS="$DEPS_LIBS $SQLITE_LIBS"
AC_SUBST(DEPS_BOOKMARKS_LIBS)

# ----------------------------------------------------------
# BUILD SHOUTCAST PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(shoutcast,
        AC_HELP_STRING([--enable-shoutcast],
                [enable SHOUTcast plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GRLNET" = "xno"; then
                           AC_MSG_ERROR([grilo-net-0.2 >= 0.2.2 not found, install it or use --disable-shoutcast])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([libxml-2.0 not found, install it or use --disable-shoutcast])
                        fi
                        ;;
                esac
        ],
        [
               if test "x$HAVE_GRLNET" = "xyes" -a "x$HAVE_XML" = "xyes"; then
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

SHOUTCAST_PLUGIN_ID="grl-shoutcast"
AC_SUBST(SHOUTCAST_PLUGIN_ID)
AC_DEFINE_UNQUOTED([SHOUTCAST_PLUGIN_ID], ["$SHOUTCAST_PLUGIN_ID"], [Shoutcast plugin ID])

DEPS_SHOUTCAST_CFLAGS="$DEPS_CFLAGS $GRLNET_CFLAGS $XML_CFLAGS"
AC_SUBST(DEPS_SHOUTCAST_CFLAGS)
DEPS_SHOUTCAST_LIBS="$DEPS_LIBS $GRLNET_LIBS $XML_LIBS"
AC_SUBST(DEPS_SHOUTCAST_LIBS)

# ----------------------------------------------------------
# BUILD APPLE TRAILERS PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(apple_trailers,
        AC_HELP_STRING([--enable-apple-trailers],
                [enable Apple Movie Trailers plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GRLNET" = "xno"; then
                           AC_MSG_ERROR([grilo-net-0.2 >= 0.2.2 not found, install it or use --disable-apple-trailers])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([libxml-2.0 not found, install it or use --disable-apple-trailers])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GRLNET" = "xyes" -a "x$HAVE_XML" = "xyes"; then
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

APPLE_TRAILERS_PLUGIN_ID="grl-apple-trailers"
AC_SUBST(APPLE_TRAILERS_PLUGIN_ID)
AC_DEFINE_UNQUOTED([APPLE_TRAILERS_PLUGIN_ID], ["$APPLE_TRAILERS_PLUGIN_ID"], [Apple Trailers plugin ID])

DEPS_APPLE_TRAILERS_CFLAGS="$DEPS_CFLAGS $GRLNET_CFLAGS $XML_CFLAGS"
AC_SUBST(DEPS_APPLE_TRAILERS_CFLAGS)
DEPS_APPLE_TRAILERS_LIBS="$DEPS_LIBS $GRLNET_LIBS $XML_LIBS"
AC_SUBST(DEPS_APPLE_TRAILERS_LIBS)

# ----------------------------------------------------------
# BUILD MAGNATUNE PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(magnatune,
        AC_HELP_STRING([--enable-magnatune],
                [enable Magnatune plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_SQLITE" = "xno"; then
                           AC_MSG_ERROR([sqlite3 not found, install it or use --disable-magnatune])
                        fi
                        if test "x$HAVE_GRLNET" = "xno"; then
                           AC_MSG_ERROR([grilo-net-0.2 >= 0.2.2 not found, install it or use --disable-magnatune])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_SQLITE" = "xyes"; then
                  enable_magnatune=yes
                else
                  enable_magnatune=no
                fi
        ])

AM_CONDITIONAL([MAGNATUNE_PLUGIN], [test "x$enable_magnatune" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL magnatune"
if test "x$enable_magnatune" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED magnatune"
fi

MAGNATUNE_PLUGIN_ID="grl-magnatune"
AC_SUBST(MAGNATUNE_PLUGIN_ID)
AC_DEFINE_UNQUOTED([MAGNATUNE_PLUGIN_ID], ["$MAGNATUNE_PLUGIN_ID"], [Magnatune plugin ID])

DEPS_MAGNATUNE_CFLAGS="$DEPS_CFLAGS $GRLNET_CFLAGS $SQLITE_CFLAGS"
AC_SUBST(DEPS_MAGNATUNE_CFLAGS)
DEPS_MAGNATUNE_LIBS="$DEPS_LIBS $GRLNET_LIBS $SQLITE_LIBS"
AC_SUBST(DEPS_MAGNATUNE_LIBS)

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

METADATA_STORE_PLUGIN_ID="grl-metadata-store"
AC_SUBST(METADATA_STORE_PLUGIN_ID)
AC_DEFINE_UNQUOTED([METADATA_STORE_PLUGIN_ID], ["$METADATA_STORE_PLUGIN_ID"], [Metadata Store plugin ID])

DEPS_METADATA_STORE_CFLAGS="$DEPS_CFLAGS $SQLITE_CFLAGS"
AC_SUBST(DEPS_METADATA_STORE_CFLAGS)
DEPS_METADATA_STORE_LIBS="$DEPS_LIBS $SQLITE_LIBS"
AC_SUBST(DEPS_METADATA_STORE_LIBS)

# ----------------------------------------------------------
# BUILD VIMEO PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(vimeo,
        AC_HELP_STRING([--enable-vimeo],
                [enable Vimeo plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GRLNET" = "xno"; then
                           AC_MSG_ERROR([grilo-net-0.2 >= 0.2.2 not found, install it or use --disable-vimeo])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([libxml-2.0 not found, install it or use --disable-vimeo])
                        fi
			               if test "x$HAVE_GCRYPT" = "xno"; then
                           AC_MSG_ERROR([libgcrypt not found, install it or use --disable-vimeo])
                        fi
                        if test "x$HAVE_TOTEM_PL_PARSER" = "xno"; then
                           AC_MSG_ERROR([totem-plparser >= 3.4.1 not found, install it or use --disable-vimeo])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GRLNET" = "xyes" -a "x$HAVE_XML" = "xyes" -a "x$HAVE_GCRYPT" = "xyes" -a "x$HAVE_TOTEM_PL_PARSER" = "xyes"; then
                   enable_vimeo=yes
                else
                   enable_vimeo=no
                fi
        ])

AM_CONDITIONAL([VIMEO_PLUGIN], [test "x$enable_vimeo" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL vimeo"
if test "x$enable_vimeo" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED vimeo"
fi

VIMEO_PLUGIN_ID="grl-vimeo"
AC_SUBST(VIMEO_PLUGIN_ID)
AC_DEFINE_UNQUOTED([VIMEO_PLUGIN_ID], ["$VIMEO_PLUGIN_ID"], [Vimeo plugin ID])

DEPS_VIMEO_CFLAGS="$DEPS_CFLAGS $GRLNET_CFLAGS $XML_CFLAGS $LIBGCRYPT_CFLAGS $TOTEM_PL_PARSER_CFLAGS"
AC_SUBST(DEPS_VIMEO_CFLAGS)
DEPS_VIMEO_LIBS="$DEPS_LIBS $GRLNET_LIBS $XML_LIBS $LIBGCRYPT_LIBS $TOTEM_PL_PARSER_LIBS"
AC_SUBST(DEPS_VIMEO_LIBS)

# ----------------------------------------------------------
# BUILD GRAVATAR PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(gravatar,
        AC_HELP_STRING([--enable-gravatar],
                [enable Gravatar plugin (default: auto)]),
        [
        ],
        [
                enable_gravatar=yes
        ])

AM_CONDITIONAL([GRAVATAR_PLUGIN], [test "x$enable_gravatar" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL gravatar"
if test "x$enable_gravatar" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED gravatar"
fi

GRAVATAR_PLUGIN_ID="grl-gravatar"
AC_SUBST(GRAVATAR_PLUGIN_ID)
AC_DEFINE_UNQUOTED([GRAVATAR_PLUGIN_ID], ["$GRAVATAR_PLUGIN_ID"], [Gravatar plugin ID])

DEPS_GRAVATAR_CFLAGS="$DEPS_CFLAGS"
AC_SUBST(DEPS_GRAVATAR_CFLAGS)
DEPS_GRAVATAR_LIBS="$DEPS_LIBS"
AC_SUBST(DEPS_GRAVATAR_LIBS)

# ----------------------------------------------------------
# BUILD TRACKER PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(tracker,
        AC_HELP_STRING([--enable-tracker],
                [enable Tracker plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_TRACKER_SPARQL" = "xno"; then
                           AC_MSG_ERROR([tracker-sparql-0.10.5 or higher not found, install it or use --disable-tracker])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_TRACKER_SPARQL" = "xyes"; then
                   enable_tracker=yes
                else
                   enable_tracker=no
                fi
        ])

AM_CONDITIONAL([TRACKER_PLUGIN], [test "x$enable_tracker" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL tracker"
if test "x$enable_tracker" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED tracker"
fi

TRACKER_PLUGIN_ID="grl-tracker"
AC_SUBST(TRACKER_PLUGIN_ID)
AC_DEFINE_UNQUOTED([TRACKER_PLUGIN_ID], ["$TRACKER_PLUGIN_ID"], [Tracker plugin ID])

DEPS_TRACKER_CFLAGS="$DEPS_CFLAGS $TRACKER_SPARQL_CFLAGS"
AC_SUBST(DEPS_TRACKER_CFLAGS)
DEPS_TRACKER_LIBS="$DEPS_LIBS $TRACKER_SPARQL_LIBS"
AC_SUBST(DEPS_TRACKER_LIBS)

# ----------------------------------------------------------
# BUILD BLIP.TV PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(bliptv,
        AC_HELP_STRING([--enable-bliptv],
                [enable Blip.tv plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GRLNET" = "xno"; then
                           AC_MSG_ERROR([grilo-net-0.2 >= 0.2.2 not found, install it or use --disable-bliptv])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([libxml-2.0 not found, install it or use --disable-bliptv])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GRLNET" = "xyes" -a "x$HAVE_XML" = "xyes"; then
                   enable_bliptv=yes
                else
                   enable_bliptv=no
                fi
        ])

AM_CONDITIONAL([BLIPTV_PLUGIN], [test "x$enable_bliptv" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL blip.tv"
if test "x$enable_bliptv" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED blip.tv"
fi

BLIPTV_PLUGIN_ID="grl-bliptv"
AC_SUBST(BLIPTV_PLUGIN_ID)
AC_DEFINE_UNQUOTED([BLIPTV_PLUGIN_ID], ["$BLIPTV_PLUGIN_ID"], [Blip.tv plugin ID])

DEPS_BLIPTV_CFLAGS="$DEPS_CFLAGS $GRLNET_CFLAGS $XML_CFLAGS"
AC_SUBST(DEPS_BLIPTV_CFLAGS)
DEPS_BLIPTV_LIBS="$DEPS_LIBS $GRLNET_LIBS $XML_LIBS"
AC_SUBST(DEPS_BLIPTV_LIBS)

# ----------------------------------------------------------
# BUILD RAI.TV PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(raitv,
        AC_HELP_STRING([--enable-raitv],
                [enable Rai.tv plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GRLNET" = "xno"; then
                           AC_MSG_ERROR([grilo-net-0.2 >= 0.2.2 not found, install it or use --disable-raitv])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([libxml-2.0 not found, install it or use --disable-raitv])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GRLNET" = "xyes" -a "x$HAVE_XML" = "xyes"; then
                   enable_raitv=yes
                else
                   enable_raitv=no
                fi
        ])

AM_CONDITIONAL([RAITV_PLUGIN], [test "x$enable_raitv" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL rai.tv"
if test "x$enable_raitv" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED rai.tv"
fi

RAITV_PLUGIN_ID="grl-raitv"
AC_SUBST(RAITV_PLUGIN_ID)
AC_DEFINE_UNQUOTED([RAITV_PLUGIN_ID], ["$RAITV_PLUGIN_ID"], [Rai.tv plugin ID])

DEPS_RAITV_CFLAGS="$DEPS_CFLAGS $LIBXML_CFLAGS $GRLNET_CFLAGS"
AC_SUBST(DEPS_RAITV_CFLAGS)
DEPS_RAITV_LIBS="$DEPS_LIBS $LIBXML_LIBS $GRLNET_LIBS"
AC_SUBST(DEPS_RAITV_LIBS)

# ----------------------------------------------------------
# BUILD LOCAL METADATA PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(localmetadata,
        AC_HELP_STRING([--enable-localmetadata],
                [enable local-metadata plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GIO" = "xno"; then
                           AC_MSG_ERROR([gio-2.0 not found, install it or use --disable-localmetadata])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GIO" = "xyes"; then
                   enable_localmetadata=yes
                else
                   enable_localmetadata=no
                fi
        ])

AM_CONDITIONAL([LOCALMETADATA_PLUGIN], [test "x$enable_localmetadata" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL localmetadata"
if test "x$enable_localmetadata" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED localmetadata"
fi

LOCALMETADATA_PLUGIN_ID="grl-local-metadata"
AC_SUBST(LOCALMETADATA_PLUGIN_ID)
AC_DEFINE_UNQUOTED([LOCALMETADATA_PLUGIN_ID], ["$LOCALMETADATA_PLUGIN_ID"], [Local metadata plugin ID])

DEPS_LOCALMETADATA_CFLAGS="$DEPS_CFLAGS $GIO_CFLAGS"
AC_SUBST(DEPS_LOCALMETADATA_CFLAGS)
DEPS_LOCALMETADATA_LIBS="$DEPS_LIBS $GIO_LIBS"
AC_SUBST(DEPS_LOCALMETADATA_LIBS)

# ----------------------------------------------------------
# BUILD DMAP PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(dmap,
        AC_HELP_STRING([--enable-dmap],
                [enable DMAP plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_DMAP" = "xno"; then
                           AC_MSG_ERROR([libdmapsharing-3.0 >= 2.9.12 not found, install it or use --disable-dmap])
                        fi
                        if test "$xHAVE_XML" = "xno"; then
                           AC_MSG_ERROR([libxml-2.0 not found, install it or use --disable-dmap])
                        fi
                esac
        ],
        [
                if test "x$HAVE_DMAP" = "xyes" -a "x$HAVE_XML" = "xyes"; then
                        enable_dmap=yes
                else
                        enable_dmap=no
                fi
        ])

AM_CONDITIONAL([DMAP_PLUGIN], [test "x$enable_dmap" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL dmap"
if test "x$enable_dmap" = "xyes"
then
       GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED dmap"
fi

DMAP_PLUGIN_ID="grl-dmap"
AC_SUBST(DMAP_PLUGIN_ID)
AC_DEFINE_UNQUOTED([DMAP_PLUGIN_ID], ["$DMAP_PLUGIN_ID"], [DMAP plugin ID])

DEPS_DMAP_CFLAGS="$DEPS_CFLAGS $DMAP_CFLAGS $XML_CFLAGS"
AC_SUBST(DEPS_DMAP_CFLAGS)
DEPS_DMAP_LIBS="$DEPS_LIBS $DMAP_LIBS $XML_LIBS"
AC_SUBST(DEPS_DMAP_LIBS)

# ----------------------------------------------------------
# BUILD TMDB PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE([tmdb],
        AC_HELP_STRING([--enable-tmdb],
                       [enable TMDb metadata plugin (default: auto)]),
        [
         AS_IF([test "x$enableval" = "xyes"],
               [AS_IF([test "x$HAVE_JSON_GLIB" = "xno"],
                      [AC_MSG_ERROR([json-glib-1.0 not found, install it or use --disable-tmdb])
                      ])
               ])],
        [
         AS_IF([test "x$HAVE_JSON_GLIB" = "xyes"],
               [enable_tmdb=yes],
               [enable_tmdb=no])
        ]
)

AM_CONDITIONAL([TMDB_PLUGIN], [test "x$enable_tmdb" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL tmdb"
AS_IF([test "x$enable_tmdb" = "xyes"],
      [GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED tmdb"])

TMDB_PLUGIN_ID="grl-tmdb"
AC_SUBST(TMDB_PLUGIN_ID)
AC_DEFINE_UNQUOTED([TMDB_PLUGIN_ID], ["$TMDB_PLUGIN_ID"], [TMDb plugin ID])

DEPS_TMDB_CFLAGS="$DEPS_CFLAGS $LIBSOUP_CFLAGS $GRLNET_CFLAGS $JSON_CFLAGS"
AC_SUBST(DEPS_TMDB_CFLAGS)
DEPS_TMDB_LIBS="$DEPS_LIBS $LIBSOUP_LIBS $GRLNET_LIBS $JSON_LIBS"
AC_SUBST(DEPS_TMDB_LIBS)

# ----------------------------------------------------------
# BUILD FREEBOX PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(freebox,
        AC_HELP_STRING([--enable-freebox],
                [enable Freebox plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GRLPLS" = "xno"; then
                           AC_MSG_ERROR([grilo-pls-0.2 >= 0.2.0 not found, install it or use --disable-freebox])
                        fi
                        if test "x$HAVE_AVAHI" = "xno"; then
                           AC_MSG_ERROR([avahi not found, install it or use --disable-freebox])
                        fi
                        ;;
                esac
        ],
        [
               if test "x$HAVE_GRLPLS" = "xyes" -a "x$HAVE_AVAHI" = "xyes"; then
                  enable_freebox=yes
               else
                  enable_freebox=no
               fi
        ])

AM_CONDITIONAL([FREEBOX_PLUGIN], [test "x$enable_freebox" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL freebox"
if test "x$enable_freebox" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED freebox"
fi

FREEBOX_PLUGIN_ID="grl-freebox"
AC_SUBST(FREEBOX_PLUGIN_ID)
AC_DEFINE_UNQUOTED([FREEBOX_PLUGIN_ID], ["$FREEBOX_PLUGIN_ID"], [Freebox plugin ID])

DEPS_FREEBOX_CFLAGS="$DEPS_CFLAGS $GRLPLS_CFLAGS $AVAHI_CFLAGS"
AC_SUBST(DEPS_FREEBOX_CFLAGS)
DEPS_FREEBOX_LIBS="$DEPS_LIBS $GRLPLS_LIBS $AVAHI_LIBS"
AC_SUBST(DEPS_FREEBOX_LIBS)

# ----------------------------------------------------------
# BUILD GUARDIANVIDEOS PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(guardianvideos,
        AC_HELP_STRING([--enable-guardianvideos],
                [enable Guardian Videos plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GRLPLS" = "xno"; then
                           AC_MSG_ERROR([grilo-pls-0.2 >= 0.2.0 not found, install it or use --disable-guardianvideos])
                        fi
                        ;;
                esac
        ],
        [
               if test "x$HAVE_GRLPLS" = "xyes"; then
                  enable_guardianvideos=yes
               else
                  enable_guardianvideos=no
               fi
        ])

AM_CONDITIONAL([GUARDIANVIDEOS_PLUGIN], [test "x$enable_guardianvideos" = "xyes"])
GRL_PLUGINS_ALL="$GRL_PLUGINS_ALL guardianvideos"
if test "x$enable_guardianvideos" = "xyes"
then
	GRL_PLUGINS_ENABLED="$GRL_PLUGINS_ENABLED guardianvideos"
fi

GUARDIANVIDEOS_PLUGIN_ID="grl-guardianvideos"
AC_SUBST(GUARDIANVIDEOS_PLUGIN_ID)
AC_DEFINE_UNQUOTED([GUARDIANVIDEOS_PLUGIN_ID], ["$GUARDIANVIDEOS_PLUGIN_ID"], [Guardian Videos plugin ID])

DEPS_GUARDIANVIDEOS_CFLAGS="$DEPS_CFLAGS $GRLPLS_CFLAGS"
AC_SUBST(DEPS_GUARDIANVIDEOS_CFLAGS)
DEPS_GUARDIANVIDEOS_LIBS="$DEPS_LIBS $GRLPLS_LIBS"
AC_SUBST(DEPS_GUARDIANVIDEOS_LIBS)

# ----------------------------------------------------------
# GETTEXT/INTLTOOL
# ----------------------------------------------------------

IT_PROG_INTLTOOL([0.40.0])
GETTEXT_PACKAGE=grilo-plugins
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [The domain to use with gettext])

#AM_GLIB_GNU_GETTEXT

# ----------------------------------------------------------
# WORKAROUNDS
# ----------------------------------------------------------

AC_SUBST([abs_top_builddir])
AC_SUBST([builddir])

# ----------------------------------------------------------
# DOCUMENTATION
# ----------------------------------------------------------

YELP_HELP_INIT

# ----------------------------------------------------------
# OUTPUT
# ----------------------------------------------------------

AC_CONFIG_FILES([
  Makefile
  help/Makefile
  help/examples/Makefile
  po/Makefile.in
  po/Makefile
  src/Makefile
  src/apple-trailers/Makefile
  src/bliptv/Makefile
  src/bookmarks/Makefile
  src/dmap/Makefile
  src/filesystem/Makefile
  src/flickr/Makefile
  src/freebox/Makefile
  src/gravatar/Makefile
  src/guardian-videos/Makefile
  src/jamendo/Makefile
  src/lastfm-albumart/Makefile
  src/local-metadata/Makefile
  src/magnatune/Makefile
  src/metadata-store/Makefile
  src/optical-media/Makefile
  src/podcasts/Makefile
  src/raitv/Makefile
  src/shoutcast/Makefile
  src/tmdb/Makefile
  src/tracker/Makefile
  src/upnp/Makefile
  src/vimeo/Makefile
  src/youtube/Makefile
  tests/Makefile
  tests/apple-trailers/Makefile
  tests/bliptv/Makefile
  tests/tmdb/Makefile
  tests/vimeo/Makefile
])

AC_OUTPUT

AG_GRL_OUTPUT_PLUGINS
