#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libgrlmetadatastore.la

libgrlmetadatastore_la_CFLAGS =	\
	$(DEPS_CFLAGS) \
	$(SQLITE_CFLAGS)

libgrlmetadatastore_la_LIBADD =	\
	$(DEPS_LIBS) \
	$(SQLITE_LIBS)

libgrlmetadatastore_la_LDFLAGS = \
	-module			 \
	-avoid-version

libgrlmetadatastore_la_SOURCES = grl-metadata-store.c grl-metadata-store.h

libdir						= $(GRL_PLUGINS_DIR)
metadatastorexmldir		= $(GRL_PLUGINS_DIR)
metadatastorexml_DATA	= $(METADATA_STORE_PLUGIN_ID).xml

EXTRA_DIST = $(metadatastorexml_DATA)

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
