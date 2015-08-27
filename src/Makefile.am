#
# Makefile.am
#
# Authors: Iago Toral Quiroga <itoral@igalia.com>
#          Guillaume Emont <gemont@igalia.com>
#
# Copyright (C) 2010-2012 Igalia S.L. All rights reserved.

include $(top_srcdir)/gtester.mk

SUBDIRS =

if BOOKMARKS_PLUGIN
SUBDIRS += bookmarks
endif

if DLEYNA_PLUGIN
SUBDIRS += dleyna
endif

if DMAP_PLUGIN
SUBDIRS += dmap
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

if LUA_FACTORY_PLUGIN
SUBDIRS += lua-factory
endif

if MAGNATUNE_PLUGIN
SUBDIRS += magnatune 
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

if RAITV_PLUGIN
SUBDIRS += raitv
endif

if SHOUTCAST_PLUGIN
SUBDIRS += shoutcast
endif

if THETVDB_PLUGIN
SUBDIRS += thetvdb
endif

if TMDB_PLUGIN
SUBDIRS += tmdb
endif

if TRACKER_PLUGIN
SUBDIRS += tracker
endif

if VIMEO_PLUGIN
SUBDIRS += vimeo
endif

if YOUTUBE_PLUGIN
SUBDIRS += youtube
endif

if FREEBOX_PLUGIN
SUBDIRS += freebox
endif

if OPENSUBTITLES_PLUGIN
SUBDIRS += opensubtitles
endif

DIST_SUBDIRS = \
   bookmarks dleyna dmap filesystem flickr freebox gravatar jamendo \
   lastfm-albumart local-metadata lua-factory magnatune metadata-store opensubtitles \
   optical-media podcasts raitv shoutcast thetvdb tmdb tracker vimeo youtube

-include $(top_srcdir)/git.mk
