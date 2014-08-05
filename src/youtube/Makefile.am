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

libgrlyoutube_la_SOURCES = grl-youtube.c grl-youtube.h youtuberesources.h youtuberesources.c

extdir				= $(GRL_PLUGINS_DIR)
youtubexmldir		= $(GRL_PLUGINS_DIR)
youtubexml_DATA	= $(YOUTUBE_PLUGIN_ID).xml

youtuberesources.h: youtube.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/youtube.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_youtube --generate-header
youtuberesources.c: youtube.gresource.xml youtuberesources.h channel-youtube.svg
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/youtube.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_youtube --generate-source

EXTRA_DIST += $(youtubexml_DATA) channel-youtube.svg youtube.gresource.xml

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
