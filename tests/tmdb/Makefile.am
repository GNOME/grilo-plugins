#
# Makefile.am
#
# Author: Iago Toral <itoral@igalia.com>
#
# Copyright (C) 2010-2013 Igalia S.L.

include $(top_srcdir)/gtester.mk

TEST_PROGS +=								\
	test_tmdb_fast_resolution			\
	test_tmdb_fast_resolution_by_id	\
	test_tmdb_full_resolution			\
	test_tmdb_missing_configuration	\
	test_tmdb_preconditions

#check_PROGRAMS = $(TESTS)

# Let the .c source code know about these paths,
# even when sourcedir!=builddir, such as during make distcheck:
#
# We could set GRL_PLUGIN_PATH here in TESTS_ENVIRONMENT, specifying all the paths (separated by :) to all the plugins,
# but it is cleaner to restrict the test to just one plugin by doing it in the code.
# Note that the tmdb plugin Makefile.am copies the .xml file into .libs so we can use the plugin.

test_tmdb_defines =																							\
	-DGRILO_PLUGINS_TESTS_TMDB_PLUGIN_PATH=\""$(abs_top_builddir)/src/tmdb/.libs/"\"		\
	-DGRILO_PLUGINS_TESTS_TMDB_DATA_PATH=\""$(abs_top_srcdir)/tests/tmdb/data/"\"

test_tmdb_preconditions_SOURCES =	\
	test_tmdb_preconditions.c			\
	test_tmdb_utils.h test_tmdb_utils.c

test_tmdb_preconditions_LDADD =	\
	@DEPS_LIBS@

test_tmdb_preconditions_CFLAGS =	\
	@DEPS_CFLAGS@						\
	-DPREFIX=$(prefix)				\
	$(test_tmdb_defines)


test_tmdb_missing_configuration_SOURCES =	\
	test_tmdb_missing_configuration.c		\
	test_tmdb_utils.h test_tmdb_utils.c

test_tmdb_missing_configuration_LDADD =	\
	@DEPS_LIBS@

test_tmdb_missing_configuration_CFLAGS =	\
	@DEPS_CFLAGS@									\
	-DPREFIX=$(prefix)							\
	$(test_tmdb_defines)

test_tmdb_fast_resolution_SOURCES =	\
	test_tmdb_fast_resolution.c		\
	test_tmdb_utils.h test_tmdb_utils.c

test_tmdb_fast_resolution_LDADD =	\
	@DEPS_LIBS@

test_tmdb_fast_resolution_CFLAGS =	\
	@DEPS_CFLAGS@							\
	-DPREFIX=$(prefix)					\
	$(test_tmdb_defines)

test_tmdb_fast_resolution_by_id_SOURCES = \
	test_tmdb_fast_resolution_by_id.c		\
	test_tmdb_utils.h test_tmdb_utils.c

test_tmdb_fast_resolution_by_id_LDADD =	\
	@DEPS_LIBS@

test_tmdb_fast_resolution_by_id_CFLAGS =	\
	@DEPS_CFLAGS@									\
	-DPREFIX=$(prefix)							\
	$(test_tmdb_defines)

test_tmdb_full_resolution_SOURCES =	\
	test_tmdb_full_resolution.c		\
	test_tmdb_utils.h test_tmdb_utils.c

test_tmdb_full_resolution_LDADD =	\
	@DEPS_LIBS@

test_tmdb_full_resolution_CFLAGS =	\
	@DEPS_CFLAGS@							\
	-DPREFIX=$(prefix)					\
	$(test_tmdb_defines)

# Distribute the tests data:
dist_noinst_DATA =			\
	data/configuration.txt	\
	data/details.txt			\
	data/empty-data.ini		\
	data/fast-by-id.ini		\
	data/multiple.txt		\
	data/no-details.ini		\
	data/search.txt			\
	data/sherlock.ini

noinst_PROGRAMS = $(TEST_PROGS)

MAINTAINERCLEANFILES =		\
	*.in							\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
