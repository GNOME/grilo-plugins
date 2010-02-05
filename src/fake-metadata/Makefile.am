#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libgrlfakemetadata.la

libgrlfakemetadata_la_CFLAGS =	\
	$(DEPS_CFLAGS)

libgrlfakemetadata_la_LIBADD =	\
	$(DEPS_LIBS)

libgrlfakemetadata_la_SOURCES = grl-fake-metadata.c grl-fake-metadata.h

libdir=$(GRL_PLUGINS_DIR)

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
