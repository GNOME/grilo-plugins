#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES		 = libgrlappletrailers.la

libgrlappletrailers_la_CFLAGS =	\
	$(DEPS_APPLE_TRAILERS_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlAppleTrailers\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrlappletrailers_la_LIBADD =	\
	$(DEPS_APPLE_TRAILERS_LIBS)

libgrlappletrailers_la_LDFLAGS = \
	-no-undefined		 \
	-module			 \
	-avoid-version

libgrlappletrailers_la_SOURCES =	\
	grl-apple-trailers.c		\
	grl-apple-trailers.h

extdir						= $(GRL_PLUGINS_DIR)
appletrailersxmldir		= $(GRL_PLUGINS_DIR)
appletrailersxml_DATA	= $(APPLE_TRAILERS_PLUGIN_ID).xml

# This lets us test the plugin without installing it,
# because grilo expects the .so and .xml files to be in
# the same directory:
copy-xml-to-libs-dir: libgrlappletrailers.la
	cp -f $(srcdir)/$(appletrailersxml_DATA) $(builddir)/.libs/

all-local: copy-xml-to-libs-dir

EXTRA_DIST = $(appletrailersxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
