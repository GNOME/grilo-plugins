#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libgrlmetadatastore.la

libgrlmetadatastore_la_CFLAGS =	\
	$(DEPS_CFLAGS) \
	$(SQLITE_CFLAGS)

libgrlmetadatastore_la_LIBADD =	\
	$(DEPS_LIBS) \
	$(SQLITE_LIBS)

libgrlmetadatastore_la_SOURCES = grl-metadata-store.c grl-metadata-store.h

libdir=$(GRL_PLUGINS_DIR)

metadatastorelibxmldir		= $(libdir)
metadatastorelibxml_DATA	= libgrlmetadatastore.xml

EXTRA_DIR = $(metadatastorelibdata_DATA)

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
