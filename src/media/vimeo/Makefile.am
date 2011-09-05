#
# Makefile.am
#
# Author: Joaquim Rocha <jrocha@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libgrlvimeo.la

libgrlvimeo_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(XML_CFLAGS)		\
	$(GTHREAD_CFLAGS)	\
	$(LIBSOUP_CFLAGS)	\
	$(LIBGCRYPT_CFLAGS)

libgrlvimeo_la_LIBADD =	\
	$(DEPS_LIBS)		\
	$(XML_LIBS)		\
	$(GTHREAD_LIBS)		\
	$(LIBSOUP_LIBS)		\
	$(LIBGCRYPT_LIBS)

libgrlvimeo_la_LDFLAGS = \
	-module		 \
	-avoid-version

libgrlvimeo_la_SOURCES =	\
	grl-vimeo.c		\
	grl-vimeo.h		\
	gvimeo.c		\
	gvimeo.h

libdir			= $(GRL_PLUGINS_DIR)
vimeoxmldir		= $(GRL_PLUGINS_DIR)
vimeoxml_DATA	= $(VIMEO_PLUGIN_ID).xml

EXTRA_DIST = $(vimeoxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
