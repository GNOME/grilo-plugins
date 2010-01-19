#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES = liblastfm-albumart.la

liblastfm_albumart_la_CFLAGS = 	\
	$(DEPS_CFLAGS) 	\
	$(VFS_CFLAGS) 	\
	$(XML_CFLAGS) 	\
	../../src

liblastfm_albumart_la_LIBADD =	\
	$(DEPS_LIBS)	\
	$(VFS_LIBS)	\
	$(XML_LIBS)	\
	../../src/libmediastore.la

liblastfm_albumart_la_SOURCES = ms-lastfm-albumart.c ms-lastfm-albumart.h

MAINTAINERCLEANFILES =	\
	*.in	\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
