#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libmsyoutube.la

libmsyoutube_la_CFLAGS	 = \
	$(DEPS_CFLAGS) \
	$(VFS_CFLAGS) \
	$(XML_CFLAGS) \
	../../src

libmsyoutube_la_LIBADD	 = \
	$(DEPS_LIBS) \
	$(VFS_LIBS) \
	$(XML_LIBS) \
	../../src/libmediastore.la

libmsyoutube_la_SOURCES = ms-youtube.c ms-youtube.h

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
