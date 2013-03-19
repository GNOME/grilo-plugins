#
# Makefile.am
#
# Author: Iago Toral <itoral@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L.

INCLUDES = @DEPS_CFLAGS@

check_tests = test_tmdb_preconditions \
	test_tmdb_missing_configuration \
	test_tmdb_fast_resolution \
	test_tmdb_fast_resolution_by_id \
	test_tmdb_full_resolution

# TODO: Add test_local_metadata to check_tests when it runs without errors.
check_PROGRAMS = test_local_metadata \
	$(check_tests)
TESTS =	$(check_tests)

#noinst_PROGRAMS += test

#test_SOURCES =	\
#	main.c
#
#test_CFLAGS =	\
#	-DPREFIX=$(prefix)
#
#test_LDADD =		\
#	@DEPS_LIBS@
#

test_local_metadata_defines = \
	-DGRILO_PLUGINS_TESTS_LOCAL_METADATA_PLUGIN_PATH=\""$(abs_top_builddir)/src/local-metadata/.libs/"\"

test_local_metadata_SOURCES =  \
	test_local_metadata.c

test_local_metadata_LDADD = \
	@DEPS_LIBS@ \
	@GTHREAD_LIBS@ \
	@GIO_LIBS@

test_local_metadata_CFLAGS = \
	-DPREFIX=$(prefix) $(test_local_metadata_defines) \
	@GTHREAD_CFLAGS@ \
	@GIO_CFLAGS@

# Let the .c source code know about these paths,
# even when sourcedir!=builddir, such as during make distcheck:
#
# We could set GRL_PLUGIN_PATH here in TESTS_ENVIRONMENT, specifying all the paths (separated by :) to all the plugins,
# but it is cleaner to restrict the test to just one plugin by doing it in the code.
# Note that the tmdb plugin Makefile.am copies the .xml file into .libs so we can use the plugin.
test_tmdb_defines = \
	-DGRILO_PLUGINS_TESTS_TMDB_PLUGIN_PATH=\""$(abs_top_builddir)/src/tmdb/.libs/"\" \
	-DGRILO_PLUGINS_TESTS_TMDB_DATA_PATH=\""$(abs_top_srcdir)/test/test_data/tmdb/"\"

test_tmdb_preconditions_SOURCES = \
	test_tmdb_preconditions.c \
	test_tmdb_utils.h test_tmdb_utils.c
test_tmdb_preconditions_LDADD = \
	@DEPS_LIBS@
test_tmdb_preconditions_CFLAGS = \
	-DPREFIX=$(prefix) $(test_tmdb_defines)

test_tmdb_missing_configuration_SOURCES = \
	test_tmdb_missing_configuration.c \
	test_tmdb_utils.h test_tmdb_utils.c
test_tmdb_missing_configuration_LDADD = \
	@DEPS_LIBS@
test_tmdb_missing_configuration_CFLAGS = \
	-DPREFIX=$(prefix) $(test_tmdb_defines)

test_tmdb_fast_resolution_SOURCES = \
	test_tmdb_fast_resolution.c \
	test_tmdb_utils.h test_tmdb_utils.c
test_tmdb_fast_resolution_LDADD = \
	@DEPS_LIBS@
test_tmdb_fast_resolution_CFLAGS = \
	-DPREFIX=$(prefix) $(test_tmdb_defines)

test_tmdb_fast_resolution_by_id_SOURCES = \
	test_tmdb_fast_resolution_by_id.c \
	test_tmdb_utils.h test_tmdb_utils.c
test_tmdb_fast_resolution_by_id_LDADD = \
	@DEPS_LIBS@
test_tmdb_fast_resolution_by_id_CFLAGS = \
	-DPREFIX=$(prefix) $(test_tmdb_defines)

test_tmdb_full_resolution_SOURCES = \
	test_tmdb_full_resolution.c \
	test_tmdb_utils.h test_tmdb_utils.c
test_tmdb_full_resolution_LDADD = \
	@DEPS_LIBS@
test_tmdb_full_resolution_CFLAGS = \
	-DPREFIX=$(prefix) $(test_tmdb_defines)

# Distribute the tests data:
dist_noinst_DATA = \
	test_data/tmdb/casts.txt \
	test_data/tmdb/configuration.txt \
	test_data/tmdb/details.txt \
	test_data/tmdb/empty-data.ini \
	test_data/tmdb/fast-by-id.ini \
	test_data/tmdb/images.txt \
	test_data/tmdb/keywords.txt \
	test_data/tmdb/no-details.ini \
	test_data/tmdb/releases.txt \
	test_data/tmdb/search.txt \
	test_data/tmdb/sherlock.ini

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
