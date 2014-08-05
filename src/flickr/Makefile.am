#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

include $(top_srcdir)/gtester.mk

libplugin_LTLIBRARIES	= libgrlflickr.la

libgrlflickr_la_CFLAGS =	\
	$(DEPS_FLICKR_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlFlickr\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrlflickr_la_LIBADD =	\
	$(DEPS_FLICKR_LIBS)

libgrlflickr_la_LDFLAGS = \
	-no-undefined	  \
	-module		  \
	-avoid-version

libgrlflickr_la_SOURCES =	\
	grl-flickr.c		\
	grl-flickr.h		\
	gflickr.c		\
	gflickr.h   \
	flickr-oauth.c\
	flickr-oauth.h

libplugindir		= $(GRL_PLUGINS_DIR)
flickrlibxmldir	= $(GRL_PLUGINS_DIR)
flickrlibxml_DATA = $(FLICKR_PLUGIN_ID).xml

EXTRA_DIST += $(flickrlibxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
