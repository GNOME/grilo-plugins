#
# Makefile.am
#
# Author: Bastien Nocera <hadess@hadess.net>
#
# Copyright (C) 2013 Bastien Nocera

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES		 = libgrlpocket.la

libgrlpocket_la_CFLAGS =			\
	$(DEPS_POCKET_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlPocket\"	\
	-DLOCALEDIR=\"$(localedir)\"

libgrlpocket_la_LIBADD =	\
	$(DEPS_POCKET_LIBS)

libgrlpocket_la_LDFLAGS = \
	-no-undefined	      \
	-module		      \
	-avoid-version

libgrlpocket_la_SOURCES =	\
	grl-pocket.c		\
	grl-pocket.h		\
	gnome-pocket.h		\
	gnome-pocket.c		\
	pocketresources.h	\
	pocketresources.c

pocketresources.h: pocket.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/pocket.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_pocket --generate-header
pocketresources.c: pocket.gresource.xml pocketresources.h channel-pocket.svg
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/pocket.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_pocket --generate-source

extdir		= $(GRL_PLUGINS_DIR)
pocketxmldir	= $(GRL_PLUGINS_DIR)
pocketxml_DATA	= $(POCKET_PLUGIN_ID).xml

EXTRA_DIST += $(pocketxml_DATA) channel-pocket.svg pocket.gresource.xml

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

CLEANFILES = pocketresources.h pocketresources.c

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
