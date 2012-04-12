#
# Makefile.am
#
# Author: Bastien Nocera <hadess@hadess.net>
#
# Copyright (C) 2012 Bastien Nocera

ext_LTLIBRARIES		 = libgrloptical-media.la

libgrloptical_media_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(GIO_CFLAGS)		\
	$(GTK_CFLAGS)		\
	$(TOTEM_PL_PARSER_CFLAGS)

libgrloptical_media_la_LIBADD =	\
	$(DEPS_LIBS)	\
	$(GIO_LIBS)		\
	$(GTK_LIBS)		\
	$(TOTEM_PL_PARSER_LIBS)

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

EXTRA_DIST = $(optical_mediaxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
