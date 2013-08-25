#
# Makefile.am
#
# Author: Joaquim Rocha <jrocha@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

include $(top_srcdir)/gtester.mk

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

# This lets us test the plugin without installing it,
# because grilo expects the .so and .xml files to be in
# the same directory:
copy-xml-to-libs-dir: libgrlvimeo.la
	cp -f $(srcdir)/$(vimeoxml_DATA) $(builddir)/.libs/

all-local: copy-xml-to-libs-dir

EXTRA_DIST = $(vimeoxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
