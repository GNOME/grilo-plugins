#
# Makefile.am
#
# Author: Victor Toso <me@victortoso.com>
#
# Copyright (C) 2014 Victor Toso. All rights reserved.

include $(top_srcdir)/gtester.mk

SUBDIRS = sources

TEST_PROGS +=	\
	test_lua_factory_grl_media	\
	test_lua_factory_xml_parser

test_lua_factory_defines =	\
	-DLUA_FACTORY_PLUGIN_PATH=\""$(abs_top_builddir)/src/lua-factory/.libs/"\"	\
	-DLUA_FACTORY_SOURCES_PATH=\""$(abs_top_srcdir)/src/lua-factory/sources/"\"	\
	-DLUA_FACTORY_DATA_PATH=\""$(abs_top_srcdir)/tests/lua-factory/data/"\"		\
	-DLUA_FACTORY_PLUGIN_TEST_DATA_PATH=\""$(abs_top_builddir)/tests/lua-factory/sources/"\"

test_lua_factory_grl_media_SOURCES =	\
	test_lua_factory_grl_media.c	\
	luafactorytestsresources.h luafactorytestsresources.c

test_lua_factory_grl_media_LDADD =	\
	@DEPS_LIBS@			\
	@JSON_LIBS@

test_lua_factory_grl_media_CFLAGS =	\
	@DEPS_CFLAGS@			\
	@JSON_CFLAGS@			\
	$(test_lua_factory_defines)

test_lua_factory_xml_parser_SOURCES =	\
	test_lua_factory_xml_parser.c	\
	luafactorytestsresources.h luafactorytestsresources.c

test_lua_factory_xml_parser_LDADD =	\
	@DEPS_LIBS@

test_lua_factory_xml_parser_CFLAGS =	\
	@DEPS_CFLAGS@			\
	$(test_lua_factory_defines)

lua_factory_tests_resources_files =	\
  data/grl-media-test-all-metadata.json

luafactorytestsresources.h: test_lua_factory_tests.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/test_lua_factory_tests.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_lua_factory_tests --generate-header

luafactorytestsresources.c:	\
	test_lua_factory_tests.gresource.xml	\
	luafactorytestsresources.h		\
	$(lua_factory_tests_resources_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/test_lua_factory_tests.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _grl_lua_factory_tests --generate-source

CLEANFILES = \
	luafactorytestsresources.h	\
	luafactorytestsresources.c

dist_noinst_DATA =				\
	$(lua_factory_tests_resources_files)	\
	data/config.ini				\
	data/test-source-grl-media.lua		\
	data/test-source-xml-parser.lua		\
	data/xml-parser-test-simple-table.lua   \
	data/xml-parser-test-simple.xml

noinst_PROGRAMS = $(TEST_PROGS)

EXTRA_DIST += \
	$(lua_factory_tests_resources_files)	\
	test_lua_factory_tests.gresource.xml

-include $(top_srcdir)/git.mk
