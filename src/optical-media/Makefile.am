#
# Makefile.am
#
# Author: Bastien Nocera <hadess@hadess.net>
#
# Copyright (C) 2012 Bastien Nocera

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES		 = libgrloptical-media.la

libgrloptical_media_la_CFLAGS =			\
	$(DEPS_OPTICAL_MEDIA_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlOpticalMedia\"	\
	-DLOCALEDIR=\"$(localedir)\"

libgrloptical_media_la_LIBADD =	\
	$(DEPS_OPTICAL_MEDIA_LIBS)

libgrloptical_media_la_LDFLAGS = \
	-no-undefined	      \
	-module		      \
	-avoid-version

libgrloptical_media_la_SOURCES =	\
	grl-optical-media.c	\
	grl-optical-media.h

extdir						= $(GRL_PLUGINS_DIR)
optical_mediaxmldir		= $(GRL_PLUGINS_DIR)
optical_mediaxml_DATA	= $(OPTICAL_MEDIA_PLUGIN_ID).xml

EXTRA_DIST += $(optical_mediaxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
