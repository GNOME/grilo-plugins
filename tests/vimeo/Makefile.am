#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2013 Igalia S.L.

include $(top_srcdir)/gtester.mk

TEST_PROGS +=	\
   test_vimeo

test_vimeo_defines =                                              \
   -DVIMEO_PLUGIN_PATH=\""$(abs_top_builddir)/src/vimeo/.libs/"\" \
   -DVIMEO_DATA_PATH=\""$(abs_top_srcdir)/tests/vimeo/data/"\"

test_vimeo_SOURCES =	\
	test_vimeo.c

test_vimeo_LDADD =	\
	@DEPS_LIBS@

test_vimeo_CFLAGS =	\
	@DEPS_CFLAGS@		\
	$(test_vimeo_defines)

# Distribute the tests data:
dist_noinst_DATA =         \
   data/network-data.ini   \
   data/empty.data         \
   data/results.data

noinst_PROGRAMS = $(TEST_PROGS)

MAINTAINERCLEANFILES =  \
   *.in                 \
   *~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
