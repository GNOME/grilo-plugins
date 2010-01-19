#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libmsjamendo.la

libmsjamendo_la_CFLAGS	 = \
	$(DEPS_CFLAGS) \
	$(VFS_CFLAGS) \
	$(XML_CFLAGS) \
	../../src

libmsjamendo_la_LIBADD	 = \
	$(DEPS_LIBS) \
	$(VFS_LIBS) \
	$(XML_LIBS) \
	../../src/libmediastore.la

libmsjamendo_la_SOURCES	 = ms-jamendo.c ms-jamendo.h

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
