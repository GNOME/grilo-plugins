#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES = libgrlpodcasts.la

libgrlpodcasts_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(GIO_CFLAGS)		\
	$(XML_CFLAGS)		\
	$(SQLITE_CFLAGS)

libgrlpodcasts_la_LIBADD =	\
	$(DEPS_LIBS)		\
	$(GIO_LIBS)		\
	$(XML_LIBS)		\
	$(SQLITE_LIBS)

libgrlpodcasts_la_SOURCES = grl-podcasts.c grl-podcasts.h

libdir=$(GRL_PLUGINS_DIR)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
