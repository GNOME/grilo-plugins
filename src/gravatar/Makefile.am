#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES = libgrlgravatar.la

libgrlgravatar_la_CFLAGS =	\
	$(DEPS_CFLAGS)

libgrlgravatar_la_LIBADD =	\
	$(DEPS_LIBS)

libgrlgravatar_la_SOURCES = grl-gravatar.c grl-gravatar.h

libdir=$(GRL_PLUGINS_DIR)

gravatarlibxmldir	= $(libdir)
gravatarlibxml_DATA	= libgrlgravatar.xml

EXTRA_DIR = $(gravatarlibdata_DATA)

MAINTAINERCLEANFILES =	\
	*.in	\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
