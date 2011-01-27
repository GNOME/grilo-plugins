#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libgrljamendo.la

libgrljamendo_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(GRLNET_CFLAGS)	\
	$(XML_CFLAGS)

libgrljamendo_la_LIBADD =	\
	$(DEPS_LIBS)		\
	$(GRLNET_LIBS)		\
	$(XML_LIBS)

libgrljamendo_la_LDFLAGS = \
	-module		   \
	-avoid-version

libgrljamendo_la_SOURCES = 	\
	grl-jamendo.c 		\
	grl-jamendo.h

libdir = $(GRL_PLUGINS_DIR)
jamendoxmldir = $(GRL_PLUGINS_CONF_DIR)
jamendoxml_DATA	= $(JAMENDO_PLUGIN_ID).xml

EXTRA_DIST = $(jamendoxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
