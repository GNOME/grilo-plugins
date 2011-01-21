#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libgrlshoutcast.la

libgrlshoutcast_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(GRLNET_CFLAGS)	\
	$(XML_CFLAGS)

libgrlshoutcast_la_LIBADD =	\
	$(DEPS_LIBS)		\
	$(GRLNET_LIBS)		\
	$(XML_LIBS)

libgrlshoutcast_la_LDFLAGS = \
	-module		     \
	-avoid-version

libgrlshoutcast_la_SOURCES	 = grl-shoutcast.c grl-shoutcast.h

libdir=$(GRL_PLUGINS_DIR)

shoutcastxmldir		= $(GRL_PLUGINS_CONF_DIR)
shoutcastxml_DATA	= $(SHOUTCAST_PLUGIN_ID).xml

EXTRA_DIST = $(shoutcastxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
