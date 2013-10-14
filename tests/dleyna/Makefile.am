#
# Makefile.am
#
# Authors: Iago Toral <itoral@igalia.com>
#          Emanuele Aina <emanuele.aina@collabora.com>
#
# Copyright (C) 2010-2013 Igalia S.L.
# Copyright (C) 2013 Intel Corp.

include $(top_srcdir)/gtester.mk

AM_CPPFLAGS = \
	@DEPS_CFLAGS@ \
	@GIO_CFLAGS@

noinst_PROGRAMS = \
	test_dleyna

if DLEYNA_TESTS_RUN_DBUSMOCK
TEST_PROGS += \
	test_dleyna
endif

# Let the .c source code know about these paths, even when sourcedir!=builddir,
# such as during make distcheck.
#
# We could set GRL_PLUGIN_PATH here in TESTS_ENVIRONMENT, specifying all the
# paths (separated by ':') to all the plugins, but it is cleaner to restrict
# the test to just one plugin by doing it in the code.
# Note that the dleyna plugin Makefile.am copies the .xml file into .libs so we
# can use the plugin.

test_dleyna_defines = \
	-DGRILO_PLUGINS_TESTS_DLEYNA_SERVICES_PATH=\""$(abs_top_builddir)/tests/dleyna/dbusmock"\" \
	-DGRILO_PLUGINS_TESTS_DLEYNA_PLUGIN_PATH=\""$(abs_top_builddir)/src/dleyna/.libs/"\" \
	-DGRILO_PLUGINS_TESTS_DLEYNA_DATA_PATH=\""$(abs_top_srcdir)/tests/dleyna/data/"\"

test_dleyna_SOURCES = \
	test_dleyna.c \
	test_dleyna_utils.c \
	test_dleyna_utils.h

test_dleyna_LDADD = \
	@DEPS_LIBS@

test_dleyna_CFLAGS = \
	-DPREFIX=$(prefix) \
	$(test_dleyna_defines)

EXTRA_DIST += \
	data/helloworld.txt \
	dbusmock/dleyna-server-mock \
	dbusmock/dleyna-server-mock.service.in \
	dbusmock/dleynamanager.py \
	dbusmock/dleynamediacontainer.py \
	dbusmock/dleynamediadevice.py \
	dbusmock/dleynamediaobject.py \
	dbusmock/items.py

clean-local:
	-rm -rf dbusmock/__pycache__

CLEANFILES = \
	dbusmock/*.pyc
