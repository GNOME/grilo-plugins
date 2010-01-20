#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES = liblastfm-albumart.la

liblastfm_albumart_la_CFLAGS = 	\
	$(DEPS_CFLAGS) 	\
	$(GIO_CFLAGS) 	\
	$(XML_CFLAGS)

liblastfm_albumart_la_LIBADD =	\
	$(DEPS_LIBS)	\
	$(GIO_LIBS)	\
	$(XML_LIBS)

liblastfm_albumart_la_SOURCES = ms-lastfm-albumart.c ms-lastfm-albumart.h

libdir=$(PLUGINS_DIR)

MAINTAINERCLEANFILES =	\
	*.in	\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
