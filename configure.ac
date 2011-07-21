#
# configure.ac
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L.. All rights reserved.

m4_define([prj_name],    [grilo-plugins])
m4_define([prj_version], [0.2.0])

AC_INIT([prj_name], [prj_version])

AM_INIT_AUTOMAKE([foreign])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src])

AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_INSTALL

PKG_CONFIG="pkg-config"
if test -n "$host" ; then
	PKG_CONFIG=$host-$PKG_CONFIG
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

PKG_CHECK_MODULES([DEPS], glib-2.0      \
			  gobject-2.0   \
			  gmodule-2.0   \
			  ${GRL_NAME} = 0.2.0)

# ----------------------------------------------------------
# DEFINITIONS
# ----------------------------------------------------------

# Plugins directory

GRL_PLUGINS_DIR=`${PKG_CONFIG} --variable=plugindir ${GRL_NAME}`
AC_SUBST(GRL_PLUGINS_DIR)
AC_DEFINE_UNQUOTED(GRL_PLUGINS_DIR, "$GRL_PLUGINS_DIR", [Plugins directory])

# Plugins configuration directory

GRL_PLUGINS_CONF_DIR="${datadir}/${GRL_NAME}/plugins"
AC_SUBST(GRL_PLUGINS_CONF_DIR)

# ----------------------------------------------------------
# EXTERNAL/OPTIONAL DEPENDENCIES
# ----------------------------------------------------------

PKG_CHECK_MODULES([GRLNET], [${GRLNET_NAME} >= 0.2.0],
			    HAVE_GRLNET=yes,
			    HAVE_GRLNET=no)

PKG_CHECK_MODULES(XML, libxml-2.0, HAVE_XML=yes, HAVE_XML=no)

PKG_CHECK_MODULES(GIO, gio-2.0, HAVE_GIO=yes, HAVE_GIO=no)

PKG_CHECK_MODULES(GUPNP, gupnp-1.0 >= 0.13, HAVE_GUPNP=yes, HAVE_GUPNP=no)

PKG_CHECK_MODULES(GUPNPAV, gupnp-av-1.0 >= 0.5, HAVE_GUPNPAV=yes, HAVE_GUPNPAV=no)

PKG_CHECK_MODULES(SQLITE, sqlite3, HAVE_SQLITE=yes, HAVE_SQLITE=no)

PKG_CHECK_MODULES(GDATA, libgdata >= 0.7.0, HAVE_GDATA=yes, HAVE_GDATA=no)

PKG_CHECK_MODULES(QUVI, libquvi >= 0.2.15, HAVE_QUVI=yes, HAVE_QUVI=no)

PKG_CHECK_MODULES(LIBSOUP, libsoup-2.4, HAVE_LIBSOUP=yes, HAVE_LIBSOUP=no)

AC_CHECK_HEADER([gcrypt.h], HAVE_GCRYPT=yes, HAVE_GCRYPT=no)

PKG_CHECK_MODULES(GTHREAD, gthread-2.0, HAVE_GTHREAD=yes, HAVE_GTHREAD=no)

PKG_CHECK_MODULES(GMIME, gmime-2.6,
                         HAVE_GMIME=yes,
                         [PKG_CHECK_MODULES(GMIME, gmime-2.4,
                                                   HAVE_GMIME=yes,
                                                   HAVE_GMIME=no)])

PKG_CHECK_MODULES(TRACKER_SPARQL, tracker-sparql-0.11,
                                  HAVE_TRACKER_SPARQL=yes,
                                  [PKG_CHECK_MODULES(TRACKER_SPARQL, tracker-sparql-0.10 >= 0.10.5,
                                                                     HAVE_TRACKER_SPARQL=yes,
                                                                     HAVE_TRACKER_SPARQL=no)])

PKG_CHECK_MODULES(GLIB_2_26, glib-2.0 >= 2.26.0,
                             HAVE_GLIB_2_26=yes,
                             HAVE_GLIB_2_26=no)

# ----------------------------------------------------------
# GDATA VERSION
# ----------------------------------------------------------

if test "x$HAVE_GDATA" = "xyes"; then
   if `$PKG_CONFIG --atleast-version=0.9.1 libgdata`; then
      AC_DEFINE(HAVE_LIBGDATA_0_9, 1, [libgdata is 0.9.1 or higher])
   fi
fi

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

FAKEMETADATA_PLUGIN_ID="grl-fake-metadata"
AC_SUBST(FAKEMETADATA_PLUGIN_ID)
AC_DEFINE_UNQUOTED([FAKEMETADATA_PLUGIN_ID], ["$FAKEMETADATA_PLUGIN_ID"], [Fake metadata plugin ID])

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

FILESYSTEM_PLUGIN_ID="grl-filesystem"
AC_SUBST(FILESYSTEM_PLUGIN_ID)
AC_DEFINE_UNQUOTED([FILESYSTEM_PLUGIN_ID], ["$FILESYSTEM_PLUGIN_ID"], [Filesystem plugin ID])

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
                           AC_MSG_ERROR([grilo-net not found, install it or use --disable-jamendo])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([xml2 not found, install it or use --disable-jamendo])
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
                           AC_MSG_ERROR([grilo-net not found, install it or use --disable-lastfm-albumart])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([xml2 not found, install it or use --disable-lastfm-albumart])
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
                        if test "$xHAVE_XML" = "xno"; then
                           AC_MSG_ERROR([xml2 not found, install it or use --disable-upnp])
                        fi
                esac
        ],
        [
                if test "x$HAVE_GUPNP" = "xyes" -a "x$HAVE_GUPNPAV" = "xyes" -a "x$HAVE_XML" = "xyes"; then
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

# ----------------------------------------------------------
# BUILD YOUTUBE PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(youtube,
        AC_HELP_STRING([--enable-youtube],
                [enable Youtube plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GRLNET" = "xno"; then
                           AC_MSG_ERROR([grilo-net not found, install it or use --disable-youtube])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([xml2 not found, install it or use --disable-youtube])
                        fi
                        if test "x$HAVE_GDATA" = "xno"; then
                           AC_MSG_ERROR([libgdata not found, install it or use --disable-youtube])
                        fi
                        if test "x$HAVE_QUVI" = "xno"; then
                           AC_MSG_ERROR([libquvi not found, install it or use --disable-youtube])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GRLNET" = "xyes" -a "x$HAVE_XML" = "xyes" -a "x$HAVE_GDATA" = "xyes" -a "x$HAVE_QUVI" = "xyes"; then
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
                           AC_MSG_ERROR([grilo-net not found, install it or use --disable-flickr])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([xml2 not found, install it or use --disable-flickr])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GRLNET" = "xyes" -a "x$HAVE_XML" = "xyes"; then
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
                           AC_MSG_ERROR([grilo-net not found, install it or use --disable-podcasts])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([xml2 not found, install it or use --disable-podcasts])
                        fi
                        if test "x$HAVE_SQLITE" = "xno"; then
                           AC_MSG_ERROR([sqlite3 not found, install it or use --disable-podcasts])
                        fi
                        if test "x$HAVE_GMIME" = "xno"; then
                           AC_MSG_ERROR([gmime not found, install it or use --disable-podcasts])
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

# ----------------------------------------------------------
# BUILD SHOUTCAST PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(shoutcast,
        AC_HELP_STRING([--enable-shoutcast],
                [enable SHOUTcast plugin (default: no)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_GRLNET" = "xno"; then
                           AC_MSG_ERROR([grilo-net not found, install it or use --disable-shoutcast])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([xml2 not found, install it or use --disable-shoutcast])
                        fi
                        ;;
                esac
        ],
        [
                 enable_shoutcast=no
#                Do not build SHOUTCast by default; it is broken
#                if test "x$HAVE_GRLNET" = "xyes" -a "x$HAVE_XML" = "xyes"; then
#                   enable_shoutcast=yes
#                else
#                   enable_shoutcast=no
#                fi
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
                           AC_MSG_ERROR([grilo-net not found, install it or use --disable-apple-trailers])
                        fi
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([xml2 not found, install it or use --disable-apple-trailers])
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

# ----------------------------------------------------------
# BUILD VIMEO PLUGIN
# ----------------------------------------------------------

AC_ARG_ENABLE(vimeo,
        AC_HELP_STRING([--enable-vimeo],
                [enable Vimeo plugin (default: auto)]),
        [
                case "$enableval" in
                     yes)
                        if test "x$HAVE_XML" = "xno"; then
                           AC_MSG_ERROR([xml2 not found, install it or use --disable-vimeo])
                        fi
                        if test "x$HAVE_LIBSOUP" = "xno"; then
                           AC_MSG_ERROR([libsoup not found, install it or use --disable-vimeo])
                        fi
			if test "x$HAVE_GCRYPT" = "xno"; then
                           AC_MSG_ERROR([libgcrypt not found, install it or use --disable-vimeo])
                        fi
			if test "x$HAVE_GTHREAD" = "xno"; then
                           AC_MSG_ERROR([gthread not found, install it or use --disable-vimeo])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_XML" = "xyes" -a "x$HAVE_LIBSOUP" = "xyes" -a "x$HAVE_GCRYPT" = "xyes" -a "x$HAVE_GTHREAD" = "xyes"; then
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
                        if test "x$HAVE_GLIB_2_26" = "xno"; then
                           AC_MSG_ERROR([glib-2.26.0 or above not found, install it or use --disable-tracker])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_TRACKER_SPARQL" = "xyes" -a "x$HAVE_GLIB_2_26" = "xyes"; then
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
                        if test "x$HAVE_GLIB_2_26" = "xno"; then
                           AC_MSG_ERROR([glib-2.26.0 or above not found, install it or use --disable-localmetadata])
                        fi
                        ;;
                esac
        ],
        [
                if test "x$HAVE_GIO" = "xyes" -a "x$HAVE_GLIB_2_26" = "xyes"; then
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
  src/media/upnp/Makefile
  src/media/vimeo/Makefile
  src/media/bookmarks/Makefile
  src/media/tracker/Makefile
  src/media/podcasts/Makefile
  src/media/jamendo/Makefile
  src/media/youtube/Makefile
  src/media/shoutcast/Makefile
  src/media/flickr/Makefile
  src/media/filesystem/Makefile
  src/media/Makefile
  src/media/apple-trailers/Makefile
  src/metadata/metadata-store/Makefile
  src/metadata/gravatar/Makefile
  src/metadata/lastfm-albumart/Makefile
  src/metadata/fake-metadata/Makefile
  src/metadata/Makefile
  src/metadata/local-metadata/Makefile
  test/Makefile
])

AC_OUTPUT

AG_GRL_OUTPUT_PLUGINS
