#
# Makefile.am
#
# Author: Victor Toso <me@victortoso.com>
#
# Copyright (C) 2013 Victor Toso. All rights reserved.

include $(top_srcdir)/gtester.mk

SUBDIRS = sources

ext_LTLIBRARIES = libgrlluafactory.la

libgrlluafactory_la_CFLAGS =						\
	$(DEPS_LUA_FACTORY_CFLAGS)					\
	-DLUA_FACTORY_SOURCE_LOCATION=\"@LUA_FACTORY_SOURCE_LOCATION@\"

libgrlluafactory_la_LIBADD = $(DEPS_LUA_FACTORY_LIBS)

libgrlluafactory_la_LDFLAGS =	\
	-no-undefined		\
	-module			\
	-avoid-version

libgrlluafactory_la_SOURCES =					\
	grl-lua-factory.c					\
	grl-lua-factory.h					\
	grl-lua-library.c					\
	grl-lua-library.h					\
	grl-lua-common.h					\
	lua-library/lua-json.c					\
	lua-library/lua-libraries.h

extdir			= $(GRL_PLUGINS_DIR)
luafactoryxmldir	= $(GRL_PLUGINS_DIR)
luafactoryxml_DATA	= $(LUA_FACTORY_PLUGIN_ID).xml

EXTRA_DIST += $(luafactoryxml_DATA)

DIST_SUBDIRS = sources

MAINTAINERCLEANFILES =	*.in *~
DISTCLEANFILES = $(MAINTAINERCLEANFILES)
