#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2014 Igalia S.L.

include $(top_srcdir)/gtester.mk

TEST_PROGS +=	\
   test_spotify_cover

test_spotify_cover_defines =								\
	-DLUA_FACTORY_PLUGIN_PATH=\""$(abs_top_builddir)/src/lua-factory/.libs/"\"	\
	-DLUA_SOURCES_PATH=\""$(abs_top_srcdir)/src/lua-factory/sources/"\"		\
	-DSPOTIFY_COVER_DATA_PATH=\""$(abs_top_srcdir)/tests/spotify-cover/data/"\"

test_spotify_cover_SOURCES =	\
	test_spotify_cover.c

test_spotify_cover_LDADD =	\
	@DEPS_LIBS@

test_spotify_cover_CFLAGS =	\
	@DEPS_CFLAGS@					\
	$(test_spotify_cover_defines)

# Distribute the tests data:
dist_noinst_DATA =               \
   data/network-data.ini         \
   data/resolve-bad-found.data   \
   data/resolve-good-found.data

noinst_PROGRAMS = $(TEST_PROGS)

-include $(top_srcdir)/git.mk
