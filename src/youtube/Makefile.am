#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES = libgrlyoutube.la

libgrlyoutube_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(GIO_CFLAGS)		\
	$(GNOMEVFS_CFLAGS)	\
	$(XML_CFLAGS)

libgrlyoutube_la_LIBADD =	\
	${top_builddir}/src/util/libutil.a		\
	$(DEPS_LIBS)		\
	$(GIO_LIBS)		\
	$(GNOMEVFS_LIBS)	\
	$(XML_LIBS)

libgrlyoutube_la_SOURCES = grl-youtube.c grl-youtube.h

libdir=$(GRL_PLUGINS_DIR)

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
