#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

lib_LTLIBRARIES = libgrlyoutube.la

libgrlyoutube_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(GRLNET_CFLAGS)	\
	$(XML_CFLAGS) 		\
	$(GTHREAD_CFLAGS)	\
	$(GDATA_CFLAGS)	\
	$(QUVI_CFLAGS)

libgrlyoutube_la_LIBADD =	\
	$(DEPS_LIBS)		\
	$(GRLNET_LIBS)		\
	$(XML_LIBS) 		\
	$(GTHREAD_LIBS)	\
	$(GDATA_LIBS)		\
	$(QUVI_LIBS)

libgrlyoutube_la_LDFLAGS = \
	-no-undefined      \
	-module		   \
	-avoid-version

libgrlyoutube_la_SOURCES = grl-youtube.c grl-youtube.h

libdir				= $(GRL_PLUGINS_DIR)
youtubexmldir		= $(GRL_PLUGINS_DIR)
youtubexml_DATA	= $(YOUTUBE_PLUGIN_ID).xml

EXTRA_DIST = $(youtubexml_DATA)

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
