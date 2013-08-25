#
# Makefile.am
#
# Author: Marco Piazza <mpiazza@gmail.com>
#

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES		 = libgrlraitv.la

libgrlraitv_la_CFLAGS =	\
	$(DEPS_RAITV_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlRaitv\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrlraitv_la_LIBADD =	\
	$(DEPS_RAITV_LIBS)

libgrlraitv_la_LDFLAGS = \
	-no-undefined	  \
	-module		  \
	-avoid-version

libgrlraitv_la_SOURCES = 		\
	grl-raitv.c			\
	grl-raitv.h

extdir 			= $(GRL_PLUGINS_DIR)
raitvxmldir 	= $(GRL_PLUGINS_DIR)
raitvxml_DATA	= $(RAITV_PLUGIN_ID).xml

EXTRA_DIST = $(raitvxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
