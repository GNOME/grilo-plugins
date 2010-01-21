#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libfakemetadata.la

libfakemetadata_la_CFLAGS = \
	$(DEPS_CFLAGS) 

libfakemetadata_la_LIBADD = \
	$(DEPS_LIBS)

libfakemetadata_la_SOURCES = fake-metadata.c fake-metadata.h

libdir=$(PLUGINS_DIR)

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
