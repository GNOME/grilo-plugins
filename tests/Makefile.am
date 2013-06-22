#
# Makefile.am
#
# Author: Juan A. Suarez Romero <jasuarez@igalia.com>
#
# Copyright (C) 2013 Igalia S.L.

include $(top_srcdir)/gtester.mk

SUBDIRS =

if LOCALMETADATA_PLUGIN
SUBDIRS += local-metadata
endif

if TMDB_PLUGIN
SUBDIRS += tmdb
endif

DIST_SUBDIRS =    \
   local-metadata \
   tmdb

MAINTAINERCLEANFILES =  \
   *.in                 \
   *~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
