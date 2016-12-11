#
# Makefile.am
#
# Author: Guillaume Emont <gemont@igalia.com>
#
# Copyright (C) 2010-2011 Igalia S.L. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES		 = libgrllocalmetadata.la

libgrllocalmetadata_la_CFLAGS =	\
	$(DEPS_LOCAL_METADATA_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlLocalMetadata\" \
	-DLOCALEDIR=\"$(localedir)\"


libgrllocalmetadata_la_LIBADD =	\
	$(DEPS_LOCAL_METADATA_LIBS)

libgrllocalmetadata_la_LDFLAGS = \
	-no-undefined		 \
	-module			 \
	-avoid-version

libgrllocalmetadata_la_SOURCES = grl-local-metadata.c grl-local-metadata.h

extdir						= $(GRL_PLUGINS_DIR)

-include $(top_srcdir)/git.mk
