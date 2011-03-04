#
# Makefile.am
#
# Authors: Iago Toral Quiroga <itoral@igalia.com>
#          Guillaume Emont <gemont@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

SUBDIRS =

if FILESYSTEM_PLUGIN
SUBDIRS += filesystem
endif

if JAMENDO_PLUGIN
SUBDIRS += jamendo
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

if VIMEO_PLUGIN
SUBDIRS += vimeo
endif

if TRACKER_PLUGIN
SUBDIRS += tracker
endif

DIST_SUBDIRS =									\
	filesystem jamendo upnp youtube flickr podcasts bookmarks shoutcast	\
	apple-trailers vimeo tracker

MAINTAINERCLEANFILES =	\
        *.in		\
        *~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
