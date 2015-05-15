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

# This lets us test the plugin without installing it,
# because grilo expects the .so and .xml files to be in
# the same directory:
copy-xml-to-libs-dir: libgrlfilesystem.la
	cp -f $(srcdir)/$(filesystemxml_DATA) $(builddir)/.libs/

all-local: copy-xml-to-libs-dir

EXTRA_DIST += $(filesystemxml_DATA)

-include $(top_srcdir)/git.mk
