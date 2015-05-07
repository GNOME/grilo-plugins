#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2013 Igalia S.L.

include $(top_srcdir)/gtester.mk

SUBDIRS =

if APPLE_TRAILERS_PLUGIN
SUBDIRS += apple-trailers
endif

if BLIPTV_PLUGIN
SUBDIRS += bliptv
endif

if DLEYNA_PLUGIN
SUBDIRS += dleyna
endif

if LASTFM_ALBUMART_PLUGIN
SUBDIRS += lastfm-albumart
endif

if LOCALMETADATA_PLUGIN
SUBDIRS += local-metadata
endif

if LUA_FACTORY_PLUGIN
SUBDIRS += lua-factory
endif

if TMDB_PLUGIN
SUBDIRS += tmdb
endif

if THETVDB_PLUGIN
SUBDIRS += thetvdb
endif

if VIMEO_PLUGIN
SUBDIRS += vimeo
endif

DIST_SUBDIRS =    \
   apple-trailers \
   bliptv         \
   dleyna         \
   lastfm-albumart\
   local-metadata \
   lua-factory    \
   tmdb           \
   thetvdb        \
   vimeo

MAINTAINERCLEANFILES =  \
   *.in                 \
   *~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)

-include $(top_srcdir)/git.mk
