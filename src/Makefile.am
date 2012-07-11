#
# Makefile.am
#
# Authors: Iago Toral Quiroga <itoral@igalia.com>
#          Guillaume Emont <gemont@igalia.com>
#
# Copyright (C) 2010-2012 Igalia S.L. All rights reserved.

SUBDIRS =

if APPLE_TRAILERS_PLUGIN
SUBDIRS += apple-trailers
endif

if BLIPTV_PLUGIN
SUBDIRS += bliptv
endif

if BOOKMARKS_PLUGIN
SUBDIRS += bookmarks
endif

if DMAP_PLUGIN
SUBDIRS += dmap
endif

if FAKEMETADATA_PLUGIN
SUBDIRS += fake-metadata
endif

if FILESYSTEM_PLUGIN
SUBDIRS += filesystem
endif

if FLICKR_PLUGIN
SUBDIRS += flickr
endif

if GRAVATAR_PLUGIN
SUBDIRS += gravatar
endif

if JAMENDO_PLUGIN
SUBDIRS += jamendo
endif

if LASTFM_ALBUMART_PLUGIN
SUBDIRS += lastfm-albumart
endif

if LOCALMETADATA_PLUGIN
SUBDIRS += local-metadata
endif

if METADATA_STORE_PLUGIN
SUBDIRS += metadata-store
endif

if OPTICAL_MEDIA_PLUGIN
SUBDIRS += optical-media
endif

if PODCASTS_PLUGIN
SUBDIRS += podcasts
endif

if SHOUTCAST_PLUGIN
SUBDIRS += shoutcast
endif

if TRACKER_PLUGIN
SUBDIRS += tracker
endif

if UPNP_PLUGIN
SUBDIRS += upnp
endif

if VIMEO_PLUGIN
SUBDIRS += vimeo
endif

if YOUTUBE_PLUGIN
SUBDIRS += youtube
endif

DIST_SUBDIRS = \
	apple-trailers bliptv bookmarks dmap fake-metadata filesystem flickr	\
	gravatar jamendo lastfm-albumart local-metadata metadata-store			\
	optical-media podcasts shoutcast tracker upnp vimeo youtube

MAINTAINERCLEANFILES =	\
        *.in		\
        *~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
