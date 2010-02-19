#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L. All rights reserved.

SUBDIRS =

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

if FLICKR_PLUGIN
SUBDIRS += flickr
endif

if PODCASTS_PLUGIN
SUBDIRS += podcasts
endif

if BOOKMARKS_PLUGIN
SUBDIRS += bookmarks
endif

if SHOUTCAST_PLUGIN
SUBDIRS += shoutcast
endif

DIST_SUBDIRS = youtube fake-metadata filesystem jamendo lastfm-albumart upnp flickr podcasts bookmarks shoutcast

MAINTAINERCLEANFILES = \
        *.in \
        *~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)

