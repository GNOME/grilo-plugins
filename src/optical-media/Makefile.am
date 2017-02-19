#
# Makefile.am
#
# Author: Bastien Nocera <hadess@hadess.net>
#
# Copyright (C) 2012 Bastien Nocera

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES		 = libgrlopticalmedia.la

libgrlopticalmedia_la_CFLAGS =			\
	$(DEPS_OPTICAL_MEDIA_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlOpticalMedia\"	\
	-DLOCALEDIR=\"$(localedir)\"

libgrlopticalmedia_la_LIBADD =	\
	$(DEPS_OPTICAL_MEDIA_LIBS)

libgrlopticalmedia_la_LDFLAGS = \
	-no-undefined	      \
	-module		      \
	-avoid-version

libgrlopticalmedia_la_SOURCES =	\
	grl-optical-media.c	\
	grl-optical-media.h

extdir						= $(GRL_PLUGINS_DIR)

-include $(top_srcdir)/git.mk
