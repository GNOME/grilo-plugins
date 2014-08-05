#
# Makefile.am
#
# Author: Victor Toso <me@victortoso.com>
#
# Copyright (C) 2013 Victor Toso. All rights reserved.

include $(top_srcdir)/gtester.mk

lua_sources_DATA =					\
	grl-metrolyrics.lua				\
	grl-radiofrance.lua				\
	grl-euronews.lua				\
	grl-guardianvideos.lua				\
	grl-musicbrainz.lua

lua_sourcesdir = $(datadir)/$(LUA_FACTORY_SOURCE_LOCATION)

EXTRA_DIST += $(lua_sources_DATA)

MAINTAINERCLEANFILES = *.in *~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
