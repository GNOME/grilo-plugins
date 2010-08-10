#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES = libgrlyoutube.la

libgrlyoutube_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(GIO_CFLAGS)		\
	$(XML_CFLAGS) 		\
	$(GTHREAD_CFLAGS)	\
	$(GDATA_CFLAGS)

libgrlyoutube_la_LIBADD =	\
	$(DEPS_LIBS)		\
	$(GIO_LIBS)		\
	$(XML_LIBS) 		\
	$(GTHREAD_LIBS)		\
	$(GDATA_LIBS)

libgrlyoutube_la_SOURCES = grl-youtube.c grl-youtube.h

libdir=$(GRL_PLUGINS_DIR)
youtubexmldir	= $(GRL_PLUGINS_CONF_DIR)
youtubexml_DATA	= $(YOUTUBE_PLUGIN_ID).xml

EXTRA_DIR = $(youtubexml_DATA)

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
