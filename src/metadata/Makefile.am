#
# Makefile.am
#
# Authors: Iago Toral Quiroga <itoral@igalia.com>
#          Guillaume Emont <gemont@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

SUBDIRS =

if FAKEMETADATA_PLUGIN
SUBDIRS += fake-metadata
endif

if LASTFM_ALBUMART_PLUGIN
SUBDIRS += lastfm-albumart
endif

if METADATA_STORE_PLUGIN
SUBDIRS += metadata-store
endif

if GRAVATAR_PLUGIN
SUBDIRS += gravatar
endif

if LOCALMETADATA_PLUGIN
SUBDIRS += local-metadata
endif

DIST_SUBDIRS =									\
	fake-metadata lastfm-albumart metadata-store gravatar local-metadata

MAINTAINERCLEANFILES =	\
        *.in		\
        *~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
