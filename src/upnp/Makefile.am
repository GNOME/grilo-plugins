#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES	= libmsupnp.la

libmsupnp_la_CFLAGS =		\
	$(DEPS_CFLAGS)		\
	$(GUPNP_CFLAGS)		\
	$(GUPNPAV_CFLAGS)

libmsupnp_la_LIBADD =	\
	$(DEPS_LIBS)	\
	$(GUPNP_LIBS)	\
	$(GUPNPAV_LIBS)

libmsupnp_la_SOURCES = ms-upnp.c ms-upnp.h

libdir = $(PLUGINS_DIR)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
