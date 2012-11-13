#
# Makefile.am
#
# Author: Iago Toral <itoral@igalia.com>
#
# Copyright (C) 2010, 2011 Igalia S.L.

INCLUDES = @DEPS_CFLAGS@

noinst_PROGRAMS = test_local_metadata test_tmdb
#noinst_PROGRAMS += test

#test_SOURCES =	\
#	main.c
#
#test_CFLAGS =	\
#	-DPREFIX=$(prefix)
#
#test_LDADD =		\
#	@DEPS_LIBS@
#
test_local_metadata_SOURCES =  \
	test_local_metadata.c

test_local_metadata_LDADD = \
	@DEPS_LIBS@ \
	@GTHREAD_LIBS@ \
	@GIO_LIBS@

test_local_metadata_CFLAGS = \
	-DPREFIX=$(prefix) \
	@GTHREAD_CFLAGS@ \
	@GIO_CFLAGS@

test_tmdb_SOURCES = \
	test_tmdb.c

test_tmdb_LDADD = \
	@DEPS_LIBS@

test_tmdb_CFLAGS = \
	-DPREFIX=$(prefix)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
