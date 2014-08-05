#
# Makefile.am
#
# Author: Bastien Nocera <hadess@hadess.net>
#
# Copyright (C) 2013 Bastien Nocera

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES		 = libgrlfreebox.la

libgrlfreebox_la_CFLAGS =			\
	$(DEPS_FREEBOX_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlFreebox\"	\
	-DLOCALEDIR=\"$(localedir)\"

libgrlfreebox_la_LIBADD =	\
	$(DEPS_FREEBOX_LIBS)

libgrlfreebox_la_LDFLAGS = \
	-no-undefined	      \
	-module		      \
	-avoid-version

libgrlfreebox_la_SOURCES =	\
	grl-freebox.c		\
	grl-freebox.h		\
	freebox-monitor.h	\
	freebox-monitor.c	\
	freeboxresources.h	\
	freeboxresources.c

freeboxresources.h: freebox.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/freebox.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_freebox --generate-header
freeboxresources.c: freebox.gresource.xml freeboxresources.h free.png
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/freebox.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_freebox --generate-source

extdir		= $(GRL_PLUGINS_DIR)
freeboxxmldir	= $(GRL_PLUGINS_DIR)
freeboxxml_DATA	= $(FREEBOX_PLUGIN_ID).xml

EXTRA_DIST += $(freeboxxml_DATA) free.png freebox.gresource.xml

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

CLEANFILES = freeboxresources.h freeboxresources.c

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
