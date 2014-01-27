#
# Makefile.am
#
# Author: Bastien Nocera <hadess@hadess.net>
#
# Copyright (C) 2014 Bastien Nocera

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES		 = libgrlguardianvideos.la

libgrlguardianvideos_la_CFLAGS =			\
	$(DEPS_GUARDIANVIDEOS_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlFreebox\"	\
	-DLOCALEDIR=\"$(localedir)\"

libgrlguardianvideos_la_LIBADD =	\
	$(DEPS_GUARDIANVIDEOS_LIBS)

libgrlguardianvideos_la_LDFLAGS = \
	-no-undefined	      \
	-module		      \
	-avoid-version

libgrlguardianvideos_la_SOURCES =	\
	grl-guardianvideos.c		\
	grl-guardianvideos.h		\
	guardianvideosresources.h	\
	guardianvideosresources.c

guardianvideosresources.h: guardianvideos.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/guardianvideos.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_guardianvideos --generate-header
guardianvideosresources.c: guardianvideos.gresource.xml guardianvideosresources.h guardian.png
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/guardianvideos.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_guardianvideos --generate-source

extdir		= $(GRL_PLUGINS_DIR)
guardianvideosxmldir	= $(GRL_PLUGINS_DIR)
guardianvideosxml_DATA	= $(GUARDIANVIDEOS_PLUGIN_ID).xml

EXTRA_DIST = $(guardianvideosxml_DATA) guardian.png guardianvideos.gresource.xml

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

CLEANFILES = guardianvideosresources.h guardianvideosresources.c

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
