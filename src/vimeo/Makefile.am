#
# Makefile.am
#
# Author: Joaquim Rocha <jrocha@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

ext_LTLIBRARIES		 = libgrlvimeo.la

libgrlvimeo_la_CFLAGS =	\
	$(DEPS_VIMEO_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlVimeo\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrlvimeo_la_LIBADD =	\
	$(DEPS_VIMEO_LIBS)

libgrlvimeo_la_LDFLAGS = \
	-no-undefined    \
	-module		 \
	-avoid-version

libgrlvimeo_la_SOURCES =	\
	grl-vimeo.c		\
	grl-vimeo.h		\
	gvimeo.c		\
	gvimeo.h

extdir			= $(GRL_PLUGINS_DIR)
vimeoxmldir		= $(GRL_PLUGINS_DIR)
vimeoxml_DATA	= $(VIMEO_PLUGIN_ID).xml

EXTRA_DIST = $(vimeoxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
