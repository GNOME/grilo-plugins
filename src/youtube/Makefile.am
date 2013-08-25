#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES = libgrlyoutube.la

libgrlyoutube_la_CFLAGS =	\
	$(DEPS_YOUTUBE_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlYoutube\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrlyoutube_la_LIBADD =	\
	$(DEPS_YOUTUBE_LIBS)

libgrlyoutube_la_LDFLAGS = \
	-no-undefined      \
	-module		   \
	-avoid-version

libgrlyoutube_la_SOURCES = grl-youtube.c grl-youtube.h

extdir				= $(GRL_PLUGINS_DIR)
youtubexmldir		= $(GRL_PLUGINS_DIR)
youtubexml_DATA	= $(YOUTUBE_PLUGIN_ID).xml

EXTRA_DIST = $(youtubexml_DATA)

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
