#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2013 Igalia S.L.

include $(top_srcdir)/gtester.mk

SUBDIRS =

if DLEYNA_PLUGIN
SUBDIRS += dleyna
endif

if LOCALMETADATA_PLUGIN
if LUA_FACTORY_PLUGIN
SUBDIRS += local-metadata
endif
endif

if LUA_FACTORY_PLUGIN
SUBDIRS += lua-factory spotify-cover games
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
   dleyna         \
   games          \
   spotify-cover  \
   local-metadata \
   lua-factory    \
   tmdb           \
   thetvdb        \
   vimeo

-include $(top_srcdir)/git.mk
