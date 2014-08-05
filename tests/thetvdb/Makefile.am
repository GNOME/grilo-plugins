#
# Makefile.am
#
# Author: Victor Toso <me@victortoso.com>
#
# Copyright (C) 2014 Victor Toso. All rights reserved.

include $(top_srcdir)/gtester.mk

TEST_PROGS +=	\
	test_thetvdb_resolve_shows	\
	test_thetvdb_resolve_episodes \
	test_thetvdb_resolve_stress

test_thetvdb_defines =	\
	-DTHETVDB_PLUGIN_PATH=\""$(abs_top_builddir)/src/thetvdb/.libs/"\"	\
	-DTHETVDB_PLUGIN_TEST_DATA_PATH=\""$(abs_top_builddir)/tests/thetvdb/data/"\"

test_thetvdb_resolve_shows_SOURCES =	\
	test_thetvdb_resolve_shows.c	\
	test_thetvdb_utils.h test_thetvdb_utils.c

test_thetvdb_resolve_shows_LDADD =	\
	@DEPS_LIBS@

test_thetvdb_resolve_shows_CFLAGS =	\
	@DEPS_CFLAGS@							\
	$(test_thetvdb_defines)

test_thetvdb_resolve_episodes_SOURCES =	\
	test_thetvdb_resolve_episodes.c	\
	test_thetvdb_utils.h test_thetvdb_utils.c

test_thetvdb_resolve_episodes_LDADD =	\
	@DEPS_LIBS@

test_thetvdb_resolve_episodes_CFLAGS =	\
	@DEPS_CFLAGS@								\
	$(test_thetvdb_defines)

test_thetvdb_resolve_stress_SOURCES =	\
	test_thetvdb_resolve_stress.c	\
	test_thetvdb_utils.h test_thetvdb_utils.c

test_thetvdb_resolve_stress_LDADD =	\
	@DEPS_LIBS@

test_thetvdb_resolve_stress_CFLAGS =	\
	@DEPS_CFLAGS@								\
	$(test_thetvdb_defines)

dist_noinst_DATA =		\
	data/config.ini		\
	data/adventure_time_get_series.txt		\
	data/adventure_time_series_all_en.zip		\
	data/boardwalk_empire_get_series.txt		\
	data/boardwalk_empire_series_all_en.zip		\
	data/felicity_get_series.txt		\
	data/felicity_series_all_en.zip		\
	data/house_get_series.txt		\
	data/house_series_all_en.zip		\
	data/naruto_get_series.txt		\
	data/naruto_series_all_en.zip

noinst_PROGRAMS = $(TEST_PROGS)

MAINTAINERCLEANFILES = 	\
   *.in		\
   *~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
