#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES		 = libgrlfakemetadata.la

libgrlfakemetadata_la_CFLAGS =	\
	-DG_LOG_DOMAIN=\"GrlFakeMetadata\" \
	$(DEPS_FAKEMETADATA_CFLAGS)

libgrlfakemetadata_la_LIBADD =	\
	$(DEPS_FAKEMETADATA_LIBS)

libgrlfakemetadata_la_LDFLAGS = \
	-no-undefined		\
	-module			\
	-avoid-version

libgrlfakemetadata_la_SOURCES = grl-fake-metadata.c grl-fake-metadata.h

extdir					= $(GRL_PLUGINS_DIR)
fakemetadataxmldir	= $(GRL_PLUGINS_DIR)
fakemetadataxml_DATA	= $(FAKEMETADATA_PLUGIN_ID).xml

EXTRA_DIST = $(fakemetadataxml_DATA)

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
