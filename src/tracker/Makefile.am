#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2011 Igalia S.L. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES		 = libgrltracker.la

libgrltracker_la_CFLAGS =	\
	$(DEPS_TRACKER_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlTracker\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrltracker_la_LIBADD =	\
	$(DEPS_TRACKER_LIBS)

libgrltracker_la_LDFLAGS = \
	-no-undefined	   \
	-module		   \
	-avoid-version

libgrltracker_la_SOURCES =	 		\
	grl-tracker.c						\
	grl-tracker.h						\
	grl-tracker-source.c				\
	grl-tracker-source.h				\
	grl-tracker-source-api.c		\
	grl-tracker-source-api.h		\
	grl-tracker-source-cache.c		\
	grl-tracker-source-cache.h		\
	grl-tracker-source-notif.c		\
	grl-tracker-source-notif.h		\
	grl-tracker-source-priv.h		\
	grl-tracker-request-queue.c	\
	grl-tracker-request-queue.h	\
	grl-tracker-utils.c				\
	grl-tracker-utils.h

extdir				= $(GRL_PLUGINS_DIR)
trackerxmldir		= $(GRL_PLUGINS_DIR)
trackerxml_DATA	= $(TRACKER_PLUGIN_ID).xml

EXTRA_DIST += $(trackerxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
