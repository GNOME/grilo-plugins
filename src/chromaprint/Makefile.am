#
# Makefile.am
#
# Author: Victor Toso <me@victortoso.com>
#
# Copyright (C) 2016 Grilo Project

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES = libgrlchromaprint.la

libgrlchromaprint_la_CFLAGS =	\
	$(DEPS_CHROMAPRINT_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlChromaprint\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrlchromaprint_la_LIBADD =	\
	$(DEPS_CHROMAPRINT_LIBS)

libgrlchromaprint_la_LDFLAGS = \
	-no-undefined	    \
	-module		    \
	-avoid-version

libgrlchromaprint_la_SOURCES = grl-chromaprint.c grl-chromaprint.h

extdir				= $(GRL_PLUGINS_DIR)

-include $(top_srcdir)/git.mk
