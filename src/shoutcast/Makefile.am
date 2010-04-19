#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libgrlshoutcast.la

libgrlshoutcast_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(GIO_CFLAGS)		\
	$(XML_CFLAGS)

libgrlshoutcast_la_LIBADD =	\
	${top_builddir}/src/util/libutil.a		\
	$(DEPS_LIBS)		\
	$(GIO_LIBS)		\
	$(GNOMEVFS_LIBS)	\
	$(XML_LIBS)

libgrlshoutcast_la_SOURCES	 = grl-shoutcast.c grl-shoutcast.h

libdir=$(GRL_PLUGINS_DIR)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
