#
# Makefile.am
#
# Author: Xabier Rodríguez Calvar <xrcalvar@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

noinst_LIBRARIES		 = libutil.a

libutil_a_SOURCES	 = gnomevfs.c gnomevfs.h

libutil_a_CFLAGS =		\
	$(DEPS_CFLAGS)		\
	$(GNOMEVFS_CFLAGS)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
