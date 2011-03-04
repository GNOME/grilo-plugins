#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

lib_LTLIBRARIES = libgrllastfm-albumart.la

libgrllastfm_albumart_la_CFLAGS =	\
	$(DEPS_CFLAGS) 			\
	$(GRLNET_CFLAGS) 		\
	$(XML_CFLAGS)

libgrllastfm_albumart_la_LIBADD =	\
	$(DEPS_LIBS)			\
	$(GRLNET_LIBS)			\
	$(XML_LIBS)

libgrllastfm_albumart_la_LDFLAGS = \
	-module			   \
	-avoid-version

libgrllastfm_albumart_la_SOURCES = grl-lastfm-albumart.c grl-lastfm-albumart.h

libdir = $(GRL_PLUGINS_DIR)
lastfmalbumartxmldir	= $(GRL_PLUGINS_CONF_DIR)
lastfmalbumartxml_DATA	= $(LASTFM_ALBUMART_PLUGIN_ID).xml

EXTRA_DIST = $(lastfmalbumartxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in	\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
