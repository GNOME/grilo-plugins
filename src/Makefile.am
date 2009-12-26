#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

SUBDIRS = fake-metadata

if HAVE_VFS
if HAVE_XML
SUBDIRS += youtube
endif
endif

DIST_SUBDIRS = youtube fake-metadata

MAINTAINERCLEANFILES = \
        *.in \
        *~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)




