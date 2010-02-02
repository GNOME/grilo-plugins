#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libmsflickr.la

libmsflickr_la_CFLAGS =		\
	$(DEPS_CFLAGS)		\
	$(FLICKCURL_CFLAGS)	\
	$(XML_CFLAGS)		\
	$(LIBCURL_CFLAGS)	\
	$(GTHREAD_CFLAGS)

libmsflickr_la_LIBADD =		\
	$(DEPS_LIBS)		\
	$(FLICKCURL_LIBS)	\
	$(XML_LIBS)		\
	$(LIBCURL_LIBS)		\
	$(GTHREAD_LIBS)

libmsflickr_la_SOURCES =	\
	ms-flickr.c		\
	ms-flickr.h

libdir=$(PLUGINS_DIR)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
