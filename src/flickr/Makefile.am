#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libgrlflickr.la

libgrlflickr_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(FLICKCURL_CFLAGS)	\
	$(XML_CFLAGS)		\
	$(LIBCURL_CFLAGS)	\
	$(GTHREAD_CFLAGS)

libgrlflickr_la_LIBADD =	\
	$(DEPS_LIBS)		\
	$(FLICKCURL_LIBS)	\
	$(XML_LIBS)		\
	$(LIBCURL_LIBS)		\
	$(GTHREAD_LIBS)

libgrlflickr_la_SOURCES =	\
	grl-flickr.c		\
	grl-flickr.h

libdir=$(GRL_PLUGINS_DIR)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
