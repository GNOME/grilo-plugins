#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libmsyoutube.la

libmsyoutube_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(VFS_CFLAGS)		\
	$(GIO_CFLAGS)		\
	$(XML_CFLAGS)

libmsyoutube_la_LIBADD =	\
	$(DEPS_LIBS)		\
	$(VFS_LIBS)		\
	$(GIO_LIBS)		\
	$(XML_LIBS)

libmsyoutube_la_SOURCES = ms-youtube.c ms-youtube.h

libdir=$(PLUGINS_DIR)

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
