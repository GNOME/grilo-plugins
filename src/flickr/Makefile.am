#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

libplugin_LTLIBRARIES	= libgrlflickr.la

libauth_LTLIBRARIES	= libgrlflickrauth.la

libgrlflickr_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(XML_CFLAGS)		\
	$(GIO_CFLAGS)

libgrlflickr_la_LIBADD =	\
	$(DEPS_LIBS)		\
	$(XML_LIBS)		\
	$(GIO_LIBS)

libgrlflickr_la_SOURCES =	\
	grl-flickr.c		\
	grl-flickr.h		\
	gflickr.c		\
	gflickr.h

libgrlflickrauth_la_CFLAGS =	\
	$(libgrlflickr_la_CFLAGS)

libgrlflickrauth_la_LIBADD =	\
	$(libgrlflickr_la_LIBADD)

libgrlflickrauth_la_SOURCES =	\
	grl-flickr-auth.c	\
	grl-flickr-auth.h	\
	gflickr.c		\
	gflickr.h

libplugindir		= $(GRL_PLUGINS_DIR)
libauthdir			= $(libdir)
flickrlibxmldir		= $(GRL_PLUGINS_CONF_DIR)
flickrlibxml_DATA	= $(FLICKR_PLUGIN_ID).xml

EXTRA_DIR = $(flickrxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
