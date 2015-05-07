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
	grl-musicbrainz.lua				\
	grl-euronews.gresource				\
	grl-guardianvideos.gresource			\
	grl-radiofrance.gresource			\
	grl-video-title-parsing.lua

lua_sourcesdir = $(datadir)/$(LUA_FACTORY_SOURCE_LOCATION)

grl-euronews.gresource: grl-euronews.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/grl-euronews.gresource.xml

grl-guardianvideos.gresource: grl-guardianvideos.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/grl-guardianvideos.gresource.xml

grl-radiofrance.gresource: grl-radiofrance.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/grl-radiofrance.gresource.xml

CLEANFILES = grl-euronews.gresource grl-guardianvideos.gresource grl-radiofrance.gresource

EXTRA_DIST +=						\
	$(lua_sources_DATA)				\
	grl-euronews.gresource.xml			\
	grl-guardianvideos.gresource.xml		\
	grl-radiofrance.gresource.xml			\
	euronews.svg					\
	guardianvideos.svg				\
	radiofrance.png

MAINTAINERCLEANFILES = *.in *~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)

-include $(top_srcdir)/git.mk
