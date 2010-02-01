#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES = libmspodcasts.la

libmspodcasts_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(GIO_CFLAGS)		\
	$(XML_CFLAGS)		\
	$(SQLITE_CFLAGS)

libmspodcasts_la_LIBADD =	\
	$(DEPS_LIBS)		\
	$(GIO_LIBS)		\
	$(XML_LIBS)		\
	$(SQLITE_LIBS)

libmspodcasts_la_SOURCES = ms-podcasts.c ms-podcasts.h

libdir=$(PLUGINS_DIR)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
