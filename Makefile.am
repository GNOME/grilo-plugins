#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

include $(top_srcdir)/gtester.mk
include $(top_srcdir)/release.mk

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src help po tests

MAINTAINERCLEANFILES = \
        aclocal.m4 \
        compile \
	config.guess \
        config.sub \
        configure \
	depcomp \
	install-sh \
	ltmain.sh \
	missing \
	libtool \
	stamp-h1 \
	Makefile.in \
	*.make \
	mkinstalldirs \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)

EXTRA_DIST += ./m4/grilo.m4

# Make sure that the documentation generation is enabled
# when creating tarball releases,
# and make sure that we enable all plugins needed by the documentation.
DISTCHECK_CONFIGURE_FLAGS = --enable-apple-trailers \
                            --enable-bliptv \
                            --enable-bookmarks \
                            --enable-dleyna \
                            --enable-dmap \
                            --enable-filesystem \
                            --enable-flickr \
                            --enable-freebox \
                            --enable-gravatar \
                            --enable-jamendo \
                            --enable-lastfm-albumart \
                            --enable-localmetadata \
                            --enable-lua-factory \
                            --enable-magnatune \
                            --enable-metadata-store \
                            --enable-optical-media \
                            --enable-pocket \
                            --enable-podcasts \
                            --enable-thetvdb \
                            --enable-tmdb \
                            --enable-tracker \
                            --enable-vimeo \
                            --enable-youtube

distclean-local:
	-rm -rf autom4te.cache

dist-hook:

if GCOV_ENABLED
lcov-clean:
	@rm -fr coverage
	@find . -name "*.gcda" -exec rm {} \;
	@lcov --directory . --zerocounters

lcov:
	@mkdir -p coverage
	@lcov --compat-libtool --directory src --capture --output-file coverage/grilo.info
	@genhtml -o coverage/ coverage/grilo.info
endif
