#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libmsfilesystem.la

libmsfilesystem_la_CFLAGS	 = \
	$(DEPS_CFLAGS) \
	$(GIO_CFLAGS) \
	../../src

libmsfilesystem_la_LIBADD	 = \
	$(DEPS_LIBS) \
	$(GIO_LIBS) \
	../../src/libmediastore.la

libmsfilesystem_la_SOURCES = ms-filesystem.c ms-filesystem.h

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
