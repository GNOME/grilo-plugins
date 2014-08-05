#
# Makefile.am
#
# Author: Guillaume Emont <gemont@igalia.com>
#
# Copyright (C) 2010-2011 Igalia S.L. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES		 = libgrllocalmetadata.la

libgrllocalmetadata_la_CFLAGS =	\
	$(DEPS_LOCALMETADATA_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlLocalMetadata\" \
	-DLOCALEDIR=\"$(localedir)\"


libgrllocalmetadata_la_LIBADD =	\
	$(DEPS_LOCALMETADATA_LIBS)

libgrllocalmetadata_la_LDFLAGS = \
	-no-undefined		 \
	-module			 \
	-avoid-version

libgrllocalmetadata_la_SOURCES = grl-local-metadata.c grl-local-metadata.h

extdir						= $(GRL_PLUGINS_DIR)
localmetadataxmldir		= $(GRL_PLUGINS_DIR)
localmetadataxml_DATA	= $(LOCALMETADATA_PLUGIN_ID).xml

# This lets us test the plugin without installing it,
# because grilo expects the .so and .xml files to be in 
# the same directory:
copy-xml-to-libs-dir: libgrllocalmetadata.la
	cp -f $(srcdir)/$(localmetadataxml_DATA) $(builddir)/.libs/

all-local: copy-xml-to-libs-dir

EXTRA_DIST += $(localmetadataxml_DATA)

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
