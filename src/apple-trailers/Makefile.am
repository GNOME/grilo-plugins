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

libgrlappletrailers_la_LDFLAGS = \
	-module			 \
	-avoid-version

libgrlappletrailers_la_SOURCES =	\
	grl-apple-trailers.c		\
	grl-apple-trailers.h

libdir=$(GRL_PLUGINS_DIR)
appletrailersxmldir	= $(GRL_PLUGINS_CONF_DIR)
appletrailersxml_DATA	= $(APPLE_TRAILERS_PLUGIN_ID).xml

EXTRA_DIR = $(appletrailersxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
