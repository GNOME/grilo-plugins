#
# Makefile.am
#
# Author: Victor Toso <me@victortoso.com>
#
# Copyright (C) 2015 Victor Toso. All rights reserved.

include $(top_srcdir)/gtester.mk

TEST_PROGS +=	\
	test_lua_metrolyrics

test_lua_factory_defines =	\
	-DLUA_FACTORY_PLUGIN_PATH=\""$(abs_top_builddir)/src/lua-factory/.libs/"\"	\
	-DLUA_FACTORY_SOURCES_PATH=\""$(abs_top_srcdir)/src/lua-factory/sources/"\" \
	-DLUA_FACTORY_PLUGIN_TEST_DATA_PATH=\""$(abs_top_builddir)/tests/lua-factory/sources/"\"

test_lua_metrolyrics_SOURCES =	\
	test_lua_metrolyrics.c	\
	metrolyricsresources.h metrolyricsresources.c	\
	test_lua_factory_utils.h test_lua_factory_utils.c

test_lua_metrolyrics_LDADD =	\
	@DEPS_LIBS@

test_lua_metrolyrics_CFLAGS =	\
	@DEPS_CFLAGS@								\
	$(test_lua_factory_defines)

metrolyrics_resources_files =	\
  data/lyrics_back_it_up.txt	\
  data/lyrics_bohemian_rhapsody.txt	\
  data/lyrics_nobodys_perfect.txt	\
  data/lyrics_ring_of_fire.txt

metrolyricsresources.h: test_lua_metrolyrics.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/test_lua_metrolyrics.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_lua_metrolyrics --generate-header
metrolyricsresources.c: test_lua_metrolyrics.gresource.xml metrolyricsresources.h $(metrolyrics_resources_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/test_lua_metrolyrics.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_lua_metrolyrics --generate-source

CLEANFILES = \
	metrolyricsresources.h \
	metrolyricsresources.c

noinst_PROGRAMS = $(TEST_PROGS)

MAINTAINERCLEANFILES = 	\
   *.in		\
   *~

EXTRA_DIST = $(metrolyrics_resources_files) test_lua_metrolyrics.gresource.xml

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
