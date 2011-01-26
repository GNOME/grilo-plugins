#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

lib_LTLIBRARIES		 = libgrlfilesystem.la

libgrlfilesystem_la_CFLAGS =	\
	$(DEPS_CFLAGS)		\
	$(GIO_CFLAGS)

libgrlfilesystem_la_LIBADD =	\
	$(DEPS_LIBS)		\
	$(GIO_LIBS)

libgrlfilesystem_la_LDFLAGS = \
	-module		      \
	-avoid-version

libgrlfilesystem_la_SOURCES = grl-filesystem.c grl-filesystem.h

libdir=$(GRL_PLUGINS_DIR)
filesystemxmldir	= $(GRL_PLUGINS_CONF_DIR)
filesystemxml_DATA	= $(FILESYSTEM_PLUGIN_ID).xml

EXTRA_DIST = $(filesystemxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
