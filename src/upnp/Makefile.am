#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES	= libgrlupnp.la

libgrlupnp_la_CFLAGS =	\
	$(DEPS_UPNP_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlUpnp\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrlupnp_la_LIBADD =	\
	$(DEPS_UPNP_LIBS)

libgrlupnp_la_LDFLAGS = \
	-no-undefined   \
	-module		\
	-avoid-version

libgrlupnp_la_CFLAGS +=	\
	$(XML_CFLAGS)

libgrlupnp_la_LIBADD +=	\
	$(XML_LIBS)

libgrlupnp_la_SOURCES = grl-upnp.c grl-upnp.h

extdir			= $(GRL_PLUGINS_DIR)
upnpxmldir		= $(GRL_PLUGINS_DIR)
upnpxml_DATA	= $(UPNP_PLUGIN_ID).xml

EXTRA_DIST = $(upnpxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
