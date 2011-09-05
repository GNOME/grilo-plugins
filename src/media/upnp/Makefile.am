#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

lib_LTLIBRARIES	= libgrlupnp.la

libgrlupnp_la_CFLAGS =		\
	$(DEPS_CFLAGS)		\
	$(GUPNP_CFLAGS)		\
	$(GUPNPAV_CFLAGS)	\
	$(GTHREAD_CFLAGS)	\
	$(XML_CFLAGS)

libgrlupnp_la_LIBADD =	\
	$(DEPS_LIBS)	\
	$(GUPNP_LIBS)	\
	$(GUPNPAV_LIBS)	\
	$(GTHREAD_LIBS)	\
	$(XML_LIBS)

libgrlupnp_la_LDFLAGS = \
	-module		\
	-avoid-version

libgrlupnp_la_CFLAGS +=	\
	$(XML_CFLAGS)

libgrlupnp_la_LIBADD +=	\
	$(XML_LIBS)

libgrlupnp_la_SOURCES = grl-upnp.c grl-upnp.h

libdir			= $(GRL_PLUGINS_DIR)
upnpxmldir		= $(GRL_PLUGINS_DIR)
upnpxml_DATA	= $(UPNP_PLUGIN_ID).xml

EXTRA_DIST = $(upnpxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
