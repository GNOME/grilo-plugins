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

if APPLE_TRAILERS_PLUGIN
SUBDIRS += apple-trailers
endif

if METADATA_STORE_PLUGIN
SUBDIRS += metadata-store
endif

if VIMEO_PLUGIN
SUBDIRS += vimeo
endif

DIST_SUBDIRS = youtube fake-metadata filesystem jamendo lastfm-albumart upnp flickr podcasts bookmarks shoutcast apple-trailers metadata-store vimeo

MAINTAINERCLEANFILES = \
        *.in \
        *~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)

