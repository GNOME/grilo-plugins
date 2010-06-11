#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libgrlappletrailers.la

libgrlappletrailers_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(GIO_CFLAGS)		\
	$(XML_CFLAGS)

libgrlappletrailers_la_LIBADD =	\
	$(DEPS_LIBS)		\
	$(GIO_LIBS)		\
	$(XML_LIBS)

libgrlappletrailers_la_SOURCES =	\
	grl-apple-trailers.c		\
	grl-apple-trailers.h

libdir=$(GRL_PLUGINS_DIR)

appletrailerslibxmldir		= $(libdir)
appletrailerslibxml_DATA	= libgrlappletrailers.xml

EXTRA_DIR = $(appletrailerslibdata_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
