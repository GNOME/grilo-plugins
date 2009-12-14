#
# Makefile.am
#
# Author: Iago Toral <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L.

INCLUDES = @DEPS_CFLAGS@ @VFS_CFLAGS@

bin_PROGRAMS = test testreg

test_SOURCES = \
	main.c fake-source.c fake-metadata-source.c youtube-source.c \
	fake-metadata-source.h  fake-source.h  youtube-source.h

test_CFLAGS =  \
	-DPREFIX=$(prefix)

test_LDADD = -ldl -lpthread @DEPS_LIBS@ @VFS_LIBS@ ../src/.libs/libmediastore.la

################

testreg_SOURCES = \
	mainreg.c

testreg_CFLAGS =  \
	-DPREFIX=$(prefix)

testreg_LDADD = -ldl -lpthread @DEPS_LIBS@ ../src/.libs/libmediastore.la

################

lib_LTLIBRARIES		 = libyoutubesource.la

libyoutubesource_la_CFLAGS	 = $(DEPS_CFLAGS) @VFS_CFLAGS@
libyoutubesource_la_LIBADD	 = $(DEPS_LIBS) @VFS_LIBS@ ../src/.libs/libmediastore.la

libyoutubesource_la_SOURCES = \
	youtube-source.c youtube-source.h

MAINTAINERCLEANFILES = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
