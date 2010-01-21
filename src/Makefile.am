#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

SUBDIRS = fake-metadata filesystem upnp

if HAVE_XML
if HAVE_VFS
SUBDIRS += youtube
endif
if HAVE_GIO
SUBDIRS += jamendo lastfm-albumart
endif
endif

if HAVE_GUPNP
if HAVE_GUPNPAV
SUBDIRS += upnp
endif
endif

DIST_SUBDIRS = youtube fake-metadata filesystem jamendo lastfm-albumart upnp

MAINTAINERCLEANFILES = \
        *.in \
        *~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)




