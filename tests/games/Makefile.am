#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2014 Igalia S.L.

include $(top_srcdir)/gtester.mk

TEST_PROGS +=	\
   test_games

test_games_defines =								\
	-DLUA_FACTORY_PLUGIN_PATH=\""$(abs_top_builddir)/src/lua-factory/.libs/"\"	\
	-DLUA_SOURCES_PATH=\""$(abs_top_srcdir)/src/lua-factory/sources/"\"		\
	-DGAMES_DATA_PATH=\""$(abs_top_srcdir)/tests/games/data/"\"

test_games_SOURCES =	\
	test_games.c

test_games_LDADD =	\
	@DEPS_LIBS@

test_games_CFLAGS =	\
	@DEPS_CFLAGS@					\
	$(test_games_defines)

# Distribute the tests data:
dist_noinst_DATA =                    \
   data/network-data.ini              \
   data/resolve-getgame-kirby.data    \
   data/resolve-getgame-strider.data  \
   data/resolve-kirby.data            \
   data/resolve-strider.data          \
   data/resolve-getgame-kirby-gb.data \
   data/resolve-kirby-gb.data         \
   data/resolve-getgame-sonic-sms.data\
   data/resolve-sonic-sms.data        \
   data/resolve-asterix.data          \
   data/resolve-getgame-asterix.data  \
   data/resolve-sonic.data            \
   data/resolve-getgame-sonic.data

noinst_PROGRAMS = $(TEST_PROGS)

-include $(top_srcdir)/git.mk
