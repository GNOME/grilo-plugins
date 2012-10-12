#
# Makefile.am
#
# Author: Guillaume Emont <gemont@igalia.com>
#
# Copyright (C) 2010-2011 Igalia S.L. All rights reserved.

ext_LTLIBRARIES		 = libgrllocalmetadata.la

libgrllocalmetadata_la_CFLAGS =	\
	$(DEPS_LOCALMETADATA_CFLAGS)

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

EXTRA_DIST = $(localmetadataxml_DATA)

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
