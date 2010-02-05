#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES	= libgrlupnp.la

libgrlupnp_la_CFLAGS =		\
	$(DEPS_CFLAGS)		\
	$(GUPNP_CFLAGS)		\
	$(GUPNPAV_CFLAGS)

libgrlupnp_la_LIBADD =	\
	$(DEPS_LIBS)	\
	$(GUPNP_LIBS)	\
	$(GUPNPAV_LIBS)

libgrlupnp_la_SOURCES = grl-upnp.c grl-upnp.h

libdir = $(GRL_PLUGINS_DIR)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
