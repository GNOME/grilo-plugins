#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES = libgrlpodcasts.la

libgrlpodcasts_la_CFLAGS =	\
	$(DEPS_PODCASTS_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlPodcasts\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrlpodcasts_la_LIBADD =	\
	$(DEPS_PODCASTS_LIBS)

libgrlpodcasts_la_LDFLAGS = \
	-no-undefined	    \
	-module		    \
	-avoid-version

libgrlpodcasts_la_SOURCES = grl-podcasts.c grl-podcasts.h

extdir				= $(GRL_PLUGINS_DIR)
podcastsxmldir		= $(GRL_PLUGINS_DIR)
podcastsxml_DATA	= $(PODCASTS_PLUGIN_ID).xml

EXTRA_DIST += $(podcastsxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
