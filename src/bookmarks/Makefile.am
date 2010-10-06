#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES = libgrlbookmarks.la

libgrlbookmarks_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(SQLITE_CFLAGS)

libgrlbookmarks_la_LIBADD =	\
	$(DEPS_LIBS)		\
	$(SQLITE_LIBS)

libgrlbookmarks_la_LDFLAGS = \
	-module		     \
	-avoid-version

libgrlbookmarks_la_SOURCES = grl-bookmarks.c grl-bookmarks.h

libdir=$(GRL_PLUGINS_DIR)
bookmarksxmldir		= $(GRL_PLUGINS_CONF_DIR)
bookmarksxml_DATA	= $(BOOKMARKS_PLUGIN_ID).xml

EXTRA_DIR = $(bookmarksxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
