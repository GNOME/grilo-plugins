#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES		 = libgrlshoutcast.la

libgrlshoutcast_la_CFLAGS =	\
	$(DEPS_SHOUTCAST_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlShoutcast\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrlshoutcast_la_LIBADD =	\
	$(DEPS_SHOUTCAST_LIBS)

libgrlshoutcast_la_LDFLAGS = \
	-no-undefined	     \
	-module		     \
	-avoid-version

libgrlshoutcast_la_SOURCES	 = grl-shoutcast.c grl-shoutcast.h

extdir				= $(GRL_PLUGINS_DIR)
shoutcastxmldir	= $(GRL_PLUGINS_DIR)
shoutcastxml_DATA	= $(SHOUTCAST_PLUGIN_ID).xml

EXTRA_DIST += $(shoutcastxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
