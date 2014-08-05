#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2014 Igalia S.L.

include $(top_srcdir)/gtester.mk

TEST_PROGS +=	\
   test_lastfm_albumart

test_lastfm_albumart_defines =                                                       	\
   -DLASTFM_ALBUMART_PLUGIN_PATH=\""$(abs_top_builddir)/src/lastfm-albumart/.libs/"\" 	\
	-DLASTFM_ALBUMART_DATA_PATH=\""$(abs_top_srcdir)/tests/lastfm-albumart/data/"\"

test_lastfm_albumart_SOURCES =	\
	test_lastfm_albumart.c

test_lastfm_albumart_LDADD =	\
	@DEPS_LIBS@

test_lastfm_albumart_CFLAGS =	\
	@DEPS_CFLAGS@					\
	$(test_lastfm_albumart_defines)

# Distribute the tests data:
dist_noinst_DATA =               \
   data/network-data.ini         \
   data/resolve-good-found.data  \
   data/resolve-good-found-default.data

noinst_PROGRAMS = $(TEST_PROGS)

MAINTAINERCLEANFILES =  \
   *.in                 \
   *~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
