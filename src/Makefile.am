#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

SUBDIRS = flickr

if FAKEMETADATA_PLUGIN
SUBDIRS += fake-metadata
endif

if FILESYSTEM_PLUGIN
SUBDIRS += filesystem
endif

if JAMENDO_PLUGIN
SUBDIRS += jamendo
endif

if LASTFM_ALBUMART_PLUGIN
SUBDIRS += lastfm-albumart
endif

if UPNP_PLUGIN
SUBDIRS += upnp
endif

if YOUTUBE_PLUGIN
SUBDIRS += youtube
endif

DIST_SUBDIRS = youtube fake-metadata filesystem jamendo lastfm-albumart upnp flickr

MAINTAINERCLEANFILES = \
        *.in \
        *~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)




