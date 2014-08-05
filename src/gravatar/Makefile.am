#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES = libgrlgravatar.la

libgrlgravatar_la_CFLAGS =	\
	$(DEPS_GRAVATAR_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlGravatar\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrlgravatar_la_LIBADD =	\
	$(DEPS_GRAVATAR_LIBS)

libgrlgravatar_la_LDFLAGS = \
	-no-undefined       \
	-module		    \
	-avoid-version

libgrlgravatar_la_SOURCES = grl-gravatar.c grl-gravatar.h

extdir				= $(GRL_PLUGINS_DIR)
gravatarxmldir		= $(GRL_PLUGINS_DIR)
gravatarxml_DATA	= $(GRAVATAR_PLUGIN_ID).xml

EXTRA_DIST += $(gravatarxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in	\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
