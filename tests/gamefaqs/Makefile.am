#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2014 Igalia S.L.

include $(top_srcdir)/gtester.mk

TEST_PROGS +=	\
   test_gamefaqs

test_gamefaqs_defines =								\
	-DLUA_FACTORY_PLUGIN_PATH=\""$(abs_top_builddir)/src/lua-factory/.libs/"\"	\
	-DLUA_SOURCES_PATH=\""$(abs_top_srcdir)/src/lua-factory/sources/"\"		\
	-DGAMEFAQS_DATA_PATH=\""$(abs_top_srcdir)/tests/gamefaqs/data/"\"

test_gamefaqs_SOURCES =	\
	test_gamefaqs.c

test_gamefaqs_LDADD =	\
	@DEPS_LIBS@

test_gamefaqs_CFLAGS =	\
	@DEPS_CFLAGS@					\
	$(test_gamefaqs_defines)

# Distribute the tests data:
dist_noinst_DATA =               \
   data/kirby-images-page.data   \
   data/network-data.ini         \
   data/resolve-kirby.data       \
   data/resolve-strider.data     \
   data/strider-game-page.data   \
   data/strider-images-page.data

noinst_PROGRAMS = $(TEST_PROGS)

-include $(top_srcdir)/git.mk
