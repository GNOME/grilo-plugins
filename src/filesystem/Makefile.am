#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES		 = libgrlfilesystem.la

libgrlfilesystem_la_CFLAGS =	\
	$(DEPS_FILESYSTEM_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlFilesystem\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrlfilesystem_la_LIBADD =	\
	$(DEPS_FILESYSTEM_LIBS)

libgrlfilesystem_la_LDFLAGS = \
	-no-undefined	      \
	-module		      \
	-avoid-version

libgrlfilesystem_la_SOURCES = grl-filesystem.c grl-filesystem.h

extdir					= $(GRL_PLUGINS_DIR)
filesystemxmldir		= $(GRL_PLUGINS_DIR)
filesystemxml_DATA	= $(FILESYSTEM_PLUGIN_ID).xml

EXTRA_DIST += $(filesystemxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
