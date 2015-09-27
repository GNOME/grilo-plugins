#
# Makefile.am
#
# Author: Victor Toso <me@victortoso.com>
#
# Copyright (C) 2013 Victor Toso. All rights reserved.

include $(top_srcdir)/gtester.mk

SUBDIRS = sources

ext_LTLIBRARIES = libgrlluafactory.la

lua-library/htmlentity.c: lua-library/htmlentity.gperf
	$(AM_V_GEN) gperf --struct-type -l -N html_entity_hash -L ANSI-C $< > $@

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
	luafactoryresources.c					\
	luafactoryresources.h					\
	lua-library/htmlentity.c				\
	lua-library/htmlentity.h				\
	lua-library/lua-json.c					\
	lua-library/lua-xml.c					\
	lua-library/lua-libraries.h

extdir			= $(GRL_PLUGINS_DIR)
luafactoryxmldir	= $(GRL_PLUGINS_DIR)
luafactoryxml_DATA	= $(LUA_FACTORY_PLUGIN_ID).xml

lua_factory_resources_files =	\
  lua-library/inspect.lua

luafactoryresources.c: grl-lua-factory.gresource.xml luafactoryresources.h $(lua_factory_resources_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/grl-lua-factory.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_lua_factory --generate-source

luafactoryresources.h: grl-lua-factory.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/grl-lua-factory.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_lua_factory --generate-header

# Rules to enable tests
copy-xml-to-libs-dir: libgrlluafactory.la
	cp -f $(srcdir)/$(luafactoryxml_DATA) $(builddir)/.libs/

all-local: copy-xml-to-libs-dir

CLEANFILES =				\
	luafactoryresources.h		\
	luafactoryresources.c

EXTRA_DIST +=					\
	$(luafactoryxml_DATA)			\
	$(lua_factory_resources_files)		\
	grl-lua-factory.gresource.xml 		\
	lua-library/htmlentity.gperf

DIST_SUBDIRS = sources

-include $(top_srcdir)/git.mk
