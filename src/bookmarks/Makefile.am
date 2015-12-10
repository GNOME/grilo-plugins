#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES = libgrlbookmarks.la

libgrlbookmarks_la_CFLAGS =	\
	$(DEPS_BOOKMARKS_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlBookmarks\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrlbookmarks_la_LIBADD =	\
	$(DEPS_BOOKMARKS_LIBS)

libgrlbookmarks_la_LDFLAGS = \
	-no-undefined	     \
	-module		     \
	-avoid-version

libgrlbookmarks_la_SOURCES =		\
	grl-bookmarks.c			\
	grl-bookmarks.h			\
	bookmarks-resource.c		\
	bookmarks-resource.h

extdir				= $(GRL_PLUGINS_DIR)

-include $(top_srcdir)/git.mk
