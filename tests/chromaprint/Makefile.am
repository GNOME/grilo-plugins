#
# Makefile.am
#
# Author: Victor Toso <me@victortoso.com>
#
# Copyright (C) 2016 Grilo Project

include $(top_srcdir)/gtester.mk

TEST_PROGS +=	\
	test_chromaprint_resolve

test_chromaprint_defines =	\
	-DCHROMAPRINT_PLUGIN_PATH=\""$(abs_top_builddir)/src/chromaprint/.libs/"\"	\
	-DCHROMAPRINT_PLUGIN_TEST_DATA_PATH=\""$(abs_top_srcdir)/tests/chromaprint/data/"\"

test_chromaprint_resolve_SOURCES =	\
	test_chromaprint_resolve.c	\
	test_chromaprint_utils.h test_chromaprint_utils.c

test_chromaprint_resolve_LDADD =	\
	@DEPS_LIBS@

test_chromaprint_resolve_CFLAGS =	\
	@DEPS_CFLAGS@							\
	$(test_chromaprint_defines)

dist_noinst_DATA =		\
	data/sample.flac \
	data/sample.ogg

noinst_PROGRAMS = $(TEST_PROGS)

-include $(top_srcdir)/git.mk
