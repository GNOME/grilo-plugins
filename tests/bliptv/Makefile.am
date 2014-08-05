#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2013 Igalia S.L.

include $(top_srcdir)/gtester.mk

TEST_PROGS +=	\
   test_bliptv

test_bliptv_defines =                                                \
   -DBLIPTV_PLUGIN_PATH=\""$(abs_top_builddir)/src/bliptv/.libs/"\"  \
   -DBLIPTV_DATA_PATH=\""$(abs_top_srcdir)/tests/bliptv/data/"\"

test_bliptv_SOURCES =	\
	test_bliptv.c

test_bliptv_LDADD =	\
	@DEPS_LIBS@

test_bliptv_CFLAGS =	\
	@DEPS_CFLAGS@		\
	$(test_bliptv_defines)

# Distribute the tests data:
dist_noinst_DATA =         \
   data/network-data.ini   \
   data/gnome.data         \
   data/grilo.data         \
   data/page1.data         \
   data/page2.data

noinst_PROGRAMS = $(TEST_PROGS)

MAINTAINERCLEANFILES =  \
   *.in                 \
   *~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
