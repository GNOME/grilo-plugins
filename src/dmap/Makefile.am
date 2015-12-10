#
# Makefile.am
#
# Author: W. Michael Petullo <mike@flyn.org>
#
# Copyright (C) 2011 W. Michael Petullo. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES	= libgrldaap.la libgrldpap.la
extdir			= $(GRL_PLUGINS_DIR)

libgrldaap_la_CFLAGS =	\
	$(DEPS_DMAP_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlDmap\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrldaap_la_LIBADD =	\
	$(DEPS_DMAP_LIBS)

libgrldaap_la_LDFLAGS =	\
	-no-undefined			\
	-module					\
	-avoid-version

libgrldaap_la_SOURCES =				\
	grl-common.c						\
	grl-common.h						\
	grl-daap.c							\
	grl-daap.h							\
	grl-daap-record.c				\
	grl-daap-record.h				\
	grl-daap-record-factory.c	\
	grl-daap-record-factory.h	\
	grl-daap-db.c					\
	grl-daap-db.h

libgrldpap_la_CFLAGS =	\
	$(DEPS_DMAP_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlDmap\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrldpap_la_LIBADD =	\
	$(DEPS_DMAP_LIBS)

libgrldpap_la_LDFLAGS =	\
	-no-undefined			\
	-module					\
	-avoid-version

libgrldpap_la_SOURCES =				\
	grl-common.c						\
	grl-common.h						\
	grl-dpap.c							\
	grl-dpap.h							\
	grl-dpap-record.c				\
	grl-dpap-record.h				\
	grl-dpap-record-factory.c	\
	grl-dpap-record-factory.h	\
	grl-dpap-db.c					\
	grl-dpap-db.h

-include $(top_srcdir)/git.mk
