#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libgrlfakemetadata.la

libgrlfakemetadata_la_CFLAGS =	\
	$(DEPS_CFLAGS)

libgrlfakemetadata_la_LIBADD =	\
	$(DEPS_LIBS)

libgrlfakemetadata_la_LDFLAGS = \
	-no-undefined		\
	-module			\
	-avoid-version

libgrlfakemetadata_la_SOURCES = grl-fake-metadata.c grl-fake-metadata.h

libdir					= $(GRL_PLUGINS_DIR)
fakemetadataxmldir	= $(GRL_PLUGINS_DIR)
fakemetadataxml_DATA	= $(FAKEMETADATA_PLUGIN_ID).xml

EXTRA_DIST = $(fakemetadataxml_DATA)

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
