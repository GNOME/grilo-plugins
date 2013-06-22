#
# Makefile.am
#
# Author: Iago Toral <itoral@igalia.com>
#
# Copyright (C) 2010-2013 Igalia S.L.

include $(top_srcdir)/gtester.mk

TEST_PROGS +=  \
   test_local_metadata

# Let the .c source code know about these paths,
# even when sourcedir!=builddir, such as during make distcheck:
#
# We could set GRL_PLUGIN_PATH here in TESTS_ENVIRONMENT, specifying all the paths (separated by :) to all the plugins,
# but it is cleaner to restrict the test to just one plugin by doing it in the code.

test_local_metadata_CFLAGS =	\
   -DTESTS_PLUGIN_PATH=\""$(abs_top_builddir)/src/local-metadata/.libs/"\"	\
   -DPREFIX=$(prefix)               \
   @GTHREAD_CFLAGS@                 \
	@DEPS_CFLAGS@		\
   @GIO_CFLAGS@

test_local_metadata_LDADD =	\
   @DEPS_LIBS@                \
   @GTHREAD_LIBS@             \
   @GIO_LIBS@

test_local_metadata_SOURCES = test_local_metadata.c

# Distribute the tests data:
dist_noinst_DATA =   \
   data/image1.png   \
   data/image2.png   \
   data/thumbnail.png

noinst_PROGRAMS = $(TEST_PROGS)

MAINTAINERCLEANFILES =  \
   *.in                 \
   *~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
