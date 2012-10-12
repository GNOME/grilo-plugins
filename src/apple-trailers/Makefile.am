#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

ext_LTLIBRARIES		 = libgrlappletrailers.la

libgrlappletrailers_la_CFLAGS =	\
	$(DEPS_APPLE_TRAILERS_CFLAGS)

libgrlappletrailers_la_LIBADD =	\
	$(DEPS_APPLE_TRAILERS_LIBS)

libgrlappletrailers_la_LDFLAGS = \
	-no-undefined		 \
	-module			 \
	-avoid-version

libgrlappletrailers_la_SOURCES =	\
	grl-apple-trailers.c		\
	grl-apple-trailers.h

extdir						= $(GRL_PLUGINS_DIR)
appletrailersxmldir		= $(GRL_PLUGINS_DIR)
appletrailersxml_DATA	= $(APPLE_TRAILERS_PLUGIN_ID).xml

EXTRA_DIST = $(appletrailersxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
