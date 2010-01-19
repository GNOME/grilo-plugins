#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libfakemetadata.la

libfakemetadata_la_CFLAGS = \
	$(DEPS_CFLAGS) \
	../../src

libfakemetadata_la_LIBADD = \
	$(DEPS_LIBS) \
	../../src/libmediastore.la

libfakemetadata_la_SOURCES = fake-metadata.c fake-metadata.h

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
