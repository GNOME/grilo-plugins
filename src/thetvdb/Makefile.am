#
# Makefile.am
#
# Author: Victor Toso <me@victortoso.com>
#
# Copyright (C) 2014 Victor Toso. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES = libgrlthetvdb.la

libgrlthetvdb_la_CFLAGS =	\
	$(DEPS_THETVDB_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlTheTVDB\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrlthetvdb_la_LIBADD =	\
	$(DEPS_THETVDB_LIBS)

libgrlthetvdb_la_LDFLAGS = \
	-no-undefined	    \
	-module		    \
	-avoid-version

libgrlthetvdb_la_SOURCES = \
	 grl-thetvdb.c \
	 grl-thetvdb.h \
	 thetvdb-resources-series.c \
	 thetvdb-resources-episodes.c \
	 thetvdb-resources-fuzzy-names.c \
	 thetvdb-resources.h

extdir				= $(GRL_PLUGINS_DIR)

-include $(top_srcdir)/git.mk
