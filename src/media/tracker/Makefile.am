#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2011 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libgrltracker.la

libgrltracker_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(TRACKER_SPARQL_CFLAGS)

libgrltracker_la_LIBADD =	\
	$(DEPS_LIBS)		\
	$(TRACKER_SPARQL_LIBS)

libgrltracker_la_LDFLAGS = \
	-no-undefined	   \
	-module		   \
	-avoid-version

libgrltracker_la_SOURCES = 		\
	grl-tracker.c			\
	grl-tracker.h			\
	grl-tracker-media.c		\
	grl-tracker-media.h		\
	grl-tracker-media-api.c		\
	grl-tracker-media-api.h		\
	grl-tracker-media-cache.c	\
	grl-tracker-media-cache.h	\
	grl-tracker-media-notif.c	\
	grl-tracker-media-notif.h	\
	grl-tracker-media-priv.h	\
	grl-tracker-metadata.c		\
	grl-tracker-metadata.h		\
	grl-tracker-request-queue.c	\
	grl-tracker-request-queue.h	\
	grl-tracker-utils.c		\
	grl-tracker-utils.h

libdir				= $(GRL_PLUGINS_DIR)
trackerxmldir		= $(GRL_PLUGINS_DIR)
trackerxml_DATA	= $(TRACKER_PLUGIN_ID).xml

EXTRA_DIST = $(trackerxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
