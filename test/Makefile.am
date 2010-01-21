#
# Makefile.am
#
# Author: Iago Toral <itoral@igalia.com>
#
# Copyright (C) 2010 Igalia S.L.

INCLUDES = @DEPS_CFLAGS@ @VFS_CFLAGS@

bin_PROGRAMS = test

test_SOURCES =	\
	main.c

test_CFLAGS =	\
	-DPREFIX=$(prefix)

test_LDADD =		\
	-ldl -lpthread	\
	@DEPS_LIBS@

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
