#
# Makefile.am
#
# Author: W. Michael Petullo <mike@flyn.org>
#
# Copyright (C) 2011 W. Michael Petullo. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES	= libgrldmap.la

libgrldmap_la_CFLAGS =	\
	$(DEPS_DMAP_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlDmap\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrldmap_la_LIBADD =	\
	$(DEPS_DMAP_LIBS)

libgrldmap_la_LDFLAGS =	\
	-no-undefined			\
	-module					\
	-avoid-version

libgrldmap_la_SOURCES =				\
	grl-dmap.c							\
	grl-dmap.h							\
	simple-daap-record.c				\
	simple-daap-record.h				\
	simple-daap-record-factory.c	\
	simple-daap-record-factory.h	\
	simple-dmap-db.c					\
	simple-dmap-db.h

extdir			= $(GRL_PLUGINS_DIR)
dmapxmldir		= $(GRL_PLUGINS_DIR)
dmapxml_DATA	= $(DMAP_PLUGIN_ID).xml

EXTRA_DIST    += $(dmapxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in						\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
