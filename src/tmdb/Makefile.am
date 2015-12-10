#
# Makefile.am
#
# Author: Jens Georg <jensg@openismus.com>
#
# Copyright (C) 2012 Openismus GmbH.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES		 = libgrltmdb.la

libgrltmdb_la_CFLAGS =	\
	$(DEPS_TMDB_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlTmdb\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrltmdb_la_LIBADD = $(DEPS_TMDB_LIBS)

libgrltmdb_la_LDFLAGS = \
	-no-undefined		\
	-module			\
	-avoid-version

libgrltmdb_la_SOURCES = \
	grl-tmdb.c grl-tmdb.h \
	grl-tmdb-request.c grl-tmdb-request.h

extdir					= $(GRL_PLUGINS_DIR)

-include $(top_srcdir)/git.mk
