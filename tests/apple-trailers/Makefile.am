#
# Makefile.am
#
# Author: Iago Toral <itoral@igalia.com>
#
# Copyright (C) 2013 Igalia S.L.

include $(top_srcdir)/gtester.mk

TEST_PROGS +=	\
   test_apple_trailers

test_apple_trailers_defines =                                                       \
   -DAPPLE_TRAILERS_PLUGIN_PATH=\""$(abs_top_builddir)/src/apple-trailers/.libs/"\"	\
   -DAPPLE_TRAILERS_DATA_PATH=\""$(abs_top_srcdir)/tests/apple-trailers/data/"\"

test_apple_trailers_SOURCES =	\
	test_apple_trailers.c

test_apple_trailers_LDADD =	\
	@DEPS_LIBS@

test_apple_trailers_CFLAGS =	\
	@DEPS_CFLAGS@					\
	$(test_apple_trailers_defines)

# Distribute the tests data:
dist_noinst_DATA =         \
   data/network-data.ini   \
   data/apple-trailers.data

noinst_PROGRAMS = $(TEST_PROGS)

MAINTAINERCLEANFILES =  \
   *.in                 \
   *~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
