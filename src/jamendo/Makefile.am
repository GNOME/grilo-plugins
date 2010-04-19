#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libgrljamendo.la

libgrljamendo_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(GIO_CFLAGS)		\
	$(XML_CFLAGS)

libgrljamendo_la_LIBADD =	\
	${top_builddir}/src/util/libutil.a		\
	$(DEPS_LIBS)		\
	$(GIO_LIBS)		\
	$(GNOMEVFS_LIBS)	\
	$(XML_LIBS)

libgrljamendo_la_SOURCES	 = grl-jamendo.c grl-jamendo.h

libdir=$(GRL_PLUGINS_DIR)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
