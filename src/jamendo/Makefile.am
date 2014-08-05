#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES		 = libgrljamendo.la

libgrljamendo_la_CFLAGS =	\
	$(DEPS_JAMENDO_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlJamendo\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrljamendo_la_LIBADD =	\
	$(DEPS_JAMENDO_LIBS)

libgrljamendo_la_LDFLAGS = \
	-no-undefined	   \
	-module		   \
	-avoid-version

libgrljamendo_la_SOURCES = 	\
	grl-jamendo.c 		\
	grl-jamendo.h

extdir				= $(GRL_PLUGINS_DIR)
jamendoxmldir		= $(GRL_PLUGINS_DIR)
jamendoxml_DATA	= $(JAMENDO_PLUGIN_ID).xml

EXTRA_DIST += $(jamendoxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
