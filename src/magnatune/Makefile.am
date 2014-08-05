#
# Makefile.am
#
# Author: Victor Toso <me@victortoso.com>
#
# Copyright (C) 2013 Victor Toso. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES = libgrlmagnatune.la

libgrlmagnatune_la_CFLAGS =	\
	$(DEPS_MAGNATUNE_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlMagnatune\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrlmagnatune_la_LIBADD =	\
	$(DEPS_MAGNATUNE_LIBS)

libgrlmagnatune_la_LDFLAGS = \
	-no-undefined	    \
	-module		    \
	-avoid-version

libgrlmagnatune_la_SOURCES = grl-magnatune.c grl-magnatune.h

extdir				= $(GRL_PLUGINS_DIR)
magnatunexmldir	= $(GRL_PLUGINS_DIR)
magnatunexml_DATA	= $(MAGNATUNE_PLUGIN_ID).xml

EXTRA_DIST += $(magnatunexml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
