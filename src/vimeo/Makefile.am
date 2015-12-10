#
# Makefile.am
#
# Author: Joaquim Rocha <jrocha@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES		 = libgrlvimeo.la

libgrlvimeo_la_CFLAGS =	\
	$(DEPS_VIMEO_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlVimeo\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrlvimeo_la_LIBADD =	\
	$(DEPS_VIMEO_LIBS)

libgrlvimeo_la_LDFLAGS = \
	-no-undefined    \
	-module		 \
	-avoid-version

libgrlvimeo_la_SOURCES =	\
	grl-vimeo.c		\
	grl-vimeo.h		\
	gvimeo.c		\
	gvimeo.h

nodist_libgrlvimeo_la_SOURCES = \
	$(resource_sources)

extdir			= $(GRL_PLUGINS_DIR)

vimeoresources.h: vimeo.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/vimeo.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_vimeo --generate-header
vimeoresources.c: vimeo.gresource.xml vimeoresources.h channel-vimeo.svg
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/vimeo.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_vimeo --generate-source

resource_sources = \
	vimeoresources.c \
	vimeoresources.h \
	$(NULL)
CLEANFILES = $(resource_sources)

EXTRA_DIST += channel-vimeo.svg vimeo.gresource.xml

-include $(top_srcdir)/git.mk
