#
# Makefile.am
#
# Author: Iago Toral <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L.

INCLUDES = @DEPS_CFLAGS@ @VFS_CFLAGS@

bin_PROGRAMS = test testreg

test_SOURCES = \
	main.c

test_CFLAGS =  \
	-DPREFIX=$(prefix)

test_LDADD = -ldl -lpthread @DEPS_LIBS@ @VFS_LIBS@ 

################

testreg_SOURCES = \
	mainreg.c

testreg_CFLAGS =  \
	-DPREFIX=$(prefix)

testreg_LDADD = -ldl -lpthread @DEPS_LIBS@ 

################

lib_LTLIBRARIES		 = libyoutubesource.la libfakesource.la libfakemetadatasource.la

libyoutubesource_la_CFLAGS	 = $(DEPS_CFLAGS) @VFS_CFLAGS@
libyoutubesource_la_LIBADD	 = $(DEPS_LIBS) @VFS_LIBS@

libyoutubesource_la_SOURCES = \
	youtube-source.c youtube-source.h

MAINTAINERCLEANFILES = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
