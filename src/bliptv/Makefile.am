#
# Makefile.am
#
# Author: Lionel Landwerlin <lionel.g.landwerlin@linux.intel.com>
#
# Copyright (C) 2011 Intel Corporation.
# Copyright (C) 2011 Igalia S.L.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES		 = libgrlbliptv.la

libgrlbliptv_la_CFLAGS =	\
	$(DEPS_BLIPTV_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlBlipTv\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrlbliptv_la_LIBADD =	\
	$(DEPS_BLIPTV_LIBS)

libgrlbliptv_la_LDFLAGS = \
	-no-undefined	  \
	-module		  \
	-avoid-version

libgrlbliptv_la_SOURCES = 		\
	grl-bliptv.c			\
	grl-bliptv.h

extdir 			= $(GRL_PLUGINS_DIR)
bliptvxmldir 	= $(GRL_PLUGINS_DIR)
bliptvxml_DATA	= $(BLIPTV_PLUGIN_ID).xml

# This lets us test the plugin without installing it,
# because grilo expects the .so and .xml files to be in
# the same directory:
copy-xml-to-libs-dir: libgrlbliptv.la
	cp -f $(srcdir)/$(bliptvxml_DATA) $(builddir)/.libs/

all-local: copy-xml-to-libs-dir

EXTRA_DIST = $(bliptvxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
