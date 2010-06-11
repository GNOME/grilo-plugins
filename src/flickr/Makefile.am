#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libgrlflickr.la

libgrlflickr_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(XML_CFLAGS)		\
	$(GIO_CFLAGS)

libgrlflickr_la_LIBADD =	\
	$(DEPS_LIBS)		\
	$(XML_LIBS)		\
	$(GIO_LIBS)

libgrlflickr_la_SOURCES =	\
	grl-flickr.c		\
	grl-flickr.h		\
	gflickr.c		\
	gflickr.h

libdir=$(GRL_PLUGINS_DIR)

flickrlibxmldir		= $(libdir)
flickrlibxml_DATA	= libgrlflickr.xml

EXTRA_DIR = $(flickrlibdata_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
