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
	$(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_LIBTOOL) \
	m4/intltool.m4 \
	$(NULL)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = grilo-plugins-${GRL_MAJOR}.${GRL_MINOR}.pc

# Make sure that the documentation generation is enabled
# when creating tarball releases,
# and make sure that we enable all plugins needed by the documentation.
DISTCHECK_CONFIGURE_FLAGS = --enable-bookmarks \
                            --enable-chromaprint \
                            --enable-dleyna \
                            --enable-dmap \
                            --enable-filesystem \
                            --enable-flickr \
                            --enable-freebox \
                            --enable-gravatar \
                            --enable-jamendo \
                            --enable-local-metadata \
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

EXTRA_DIST = \
	$(shell find "$(top_srcdir)" -type f -name meson.build ! -path "$(top_srcdir)/grilo-plugins-*" ) \
	meson_options.txt

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

-include $(top_srcdir)/git.mk
