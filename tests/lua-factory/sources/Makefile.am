#
# Makefile.am
#
# Author: Victor Toso <me@victortoso.com>
#
# Copyright (C) 2015 Victor Toso. All rights reserved.

include $(top_srcdir)/gtester.mk

noinst_PROGS =	\
	test_lua_metrolyrics

test_lua_factory_defines =	\
	-DLUA_FACTORY_PLUGIN_PATH=\""$(abs_top_builddir)/src/lua-factory/.libs/"\"	\
	-DCHROMAPRINT_PLUGIN_PATH=\""$(abs_top_builddir)/src/chromaprint/.libs/"\"	\
	-DLUA_FACTORY_SOURCES_PATH=\""$(abs_top_srcdir)/src/lua-factory/sources/"\" \
	-DLUA_FACTORY_SOURCES_DATA_PATH=\""$(abs_top_srcdir)/tests/lua-factory/sources/data/"\" \
	-DLUA_FACTORY_PLUGIN_TEST_DATA_PATH=\""$(abs_top_builddir)/tests/lua-factory/sources/"\"

if CHROMAPRINT_PLUGIN
TEST_PROGS += test_lua_acoustid
test_lua_acoustid_SOURCES =	\
	test_lua_acoustid.c	\
	acoustidresources.h acoustidresources.c	\
	test_lua_factory_utils.h test_lua_factory_utils.c

test_lua_acoustid_LDADD =	\
	@DEPS_LIBS@

test_lua_acoustid_CFLAGS =	\
	@DEPS_CFLAGS@								\
	$(test_lua_factory_defines)
else
EXTRA_DIST +=						\
	test_lua_acoustid.c				\
	acoustidresources.h acoustidresources.c		\
	test_lua_factory_utils.h test_lua_factory_utils.c
endif

acoustid_resources_files =	\
  data/acoustid_ludovico_einaudi_primavera.txt   	\
  data/acoustid_norah_jones_chasing_pirates.txt	        \
  data/acoustid_philip_glass_the_passion_of.txt	        \
  data/acoustid_trombone_shorty_buckjump.txt            \
  data/acoustid_radiohead_paranoid_android.txt		\
  data/chromaprint_ludovico_einaudi_primavera.txt	\
  data/chromaprint_norah_jones_chasing_pirates.txt	\
  data/chromaprint_philip_glass_the_passion_of.txt	\
  data/chromaprint_trombone_shorty_buckjump.txt		\
  data/chromaprint_radiohead_paranoid_android.txt

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

acoustidresources.h: test_lua_acoustid.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/test_lua_acoustid.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_lua_acoustid --generate-header
acoustidresources.c: test_lua_acoustid.gresource.xml acoustidresources.h $(acoustid_resources_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/test_lua_acoustid.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_lua_acoustid --generate-source

metrolyricsresources.h: test_lua_metrolyrics.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/test_lua_metrolyrics.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_lua_metrolyrics --generate-header
metrolyricsresources.c: test_lua_metrolyrics.gresource.xml metrolyricsresources.h $(metrolyrics_resources_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/test_lua_metrolyrics.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_lua_metrolyrics --generate-source

CLEANFILES = \
	acoustidresources.h \
	acoustidresources.c \
	metrolyricsresources.h \
	metrolyricsresources.c

noinst_PROGRAMS = $(TEST_PROGS)

EXTRA_DIST += \
	data/config.ini			\
	$(acoustid_resources_files)	\
	test_lua_acoustid.gresource.xml	\
	$(metrolyrics_resources_files)	\
	test_lua_metrolyrics.gresource.xml

-include $(top_srcdir)/git.mk
