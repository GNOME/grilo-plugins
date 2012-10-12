#
# Makefile.am
#
# Author: Jens Georg <jensg@openismus.com>
#
# Copyright (C) 2012 Openismus GmbH.

ext_LTLIBRARIES		 = libgrltmdb.la

libgrltmdb_la_CFLAGS = $(DEPS_TMDB_CFLAGS)

libgrltmdb_la_LIBADD = $(DEPS_TMDB_LIBS)

libgrltmdb_la_LDFLAGS = \
	-no-undefined		\
	-module			\
	-avoid-version

libgrltmdb_la_SOURCES = \
	grl-tmdb.c grl-tmdb.h \
	grl-tmdb-request.c grl-tmdb-request.h

extdir					= $(GRL_PLUGINS_DIR)
tmdbxmldir	= $(GRL_PLUGINS_DIR)
tmdbxml_DATA	= $(TMDB_PLUGIN_ID).xml

EXTRA_DIST = $(tmdbxml_DATA)

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
