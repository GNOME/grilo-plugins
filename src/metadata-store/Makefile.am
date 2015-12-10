#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES		 = libgrlmetadatastore.la

libgrlmetadatastore_la_CFLAGS =	\
	$(DEPS_METADATA_STORE_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlMetadataStore\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrlmetadatastore_la_LIBADD =	\
	$(DEPS_METADATA_STORE_LIBS)

libgrlmetadatastore_la_LDFLAGS = \
	-no-undefined		 \
	-module			 \
	-avoid-version

libgrlmetadatastore_la_SOURCES = grl-metadata-store.c grl-metadata-store.h

extdir						= $(GRL_PLUGINS_DIR)

-include $(top_srcdir)/git.mk
