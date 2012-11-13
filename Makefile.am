#
# Makefile.am
#
# Author: Iago Toral Quiroga <itoral@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src test

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

EXTRA_DIST = ./m4/grilo.m4

distclean-local:
	-rm -rf autom4te.cache

dist-hook:
	@if test -d "$(srcdir)/.git"; \
	then \
		echo Creating ChangeLog && \
		( cd "$(top_srcdir)" && \
		  $(top_srcdir)/missing --run git log --stat ) > ChangeLog.tmp \
		&& mv -f ChangeLog.tmp $(top_distdir)/ChangeLog \
		|| ( rm -f ChangeLog.tmp ; \
		     echo Failed to generate ChangeLog >&2 ); \
	else \
		echo A git clone is required to generate a ChangeLog >&2; \
	fi
