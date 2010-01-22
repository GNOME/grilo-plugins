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
	$(XML_CFLAGS) 

libmsjamendo_la_LIBADD	 = \
	$(DEPS_LIBS) \
	$(GIO_LIBS) \
	$(XML_LIBS) 

libmsjamendo_la_SOURCES	 = ms-jamendo.c ms-jamendo.h

libdir=$(PLUGINS_DIR)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
