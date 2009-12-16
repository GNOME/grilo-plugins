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
libyoutubesource_la_SOURCES      = youtube-source.c youtube-source.h

libfakemetadatasource_la_CFLAGS	 = $(DEPS_CFLAGS)
libfakemetadatasource_la_LIBADD	 = $(DEPS_LIBS) 
libfakemetadatasource_la_SOURCES = fake-metadata-source.c fake-metadata-source.h

libfakesource_la_CFLAGS	 = $(DEPS_CFLAGS)
libfakesource_la_LIBADD	 = $(DEPS_LIBS) 
libfakesource_la_SOURCES = fake-source.c fake-source.h



MAINTAINERCLEANFILES = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
