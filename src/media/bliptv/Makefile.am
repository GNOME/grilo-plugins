#
# Makefile.am
#
# Author: Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
#
# Copyright (C) 2011 Intel Corporation.

lib_LTLIBRARIES		 = libgrlbliptv.la

libgrlbliptv_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(LIBXML_CFLAGS)	\
	$(LIBREST_CFLAGS)

libgrlbliptv_la_LIBADD =	\
	$(DEPS_LIBS)		\
	$(LIBXML_LIBS)		\
	$(LIBREST_LIBS)

libgrlbliptv_la_LDFLAGS = \
	-module		   \
	-avoid-version

libgrlbliptv_la_SOURCES = 		\
	grl-bliptv.c			\
	grl-bliptv.h

libdir = $(GRL_PLUGINS_DIR)
bliptvxmldir = $(GRL_PLUGINS_CONF_DIR)
bliptvxml_DATA	= $(BLIPTV_PLUGIN_ID).xml

EXTRA_DIST = $(bliptvxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
