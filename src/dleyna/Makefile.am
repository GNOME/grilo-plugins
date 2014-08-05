#
# Makefile.am
#
# Copyright (C) 2010, 2011 Igalia S.L. All rights reserved.
# Copyright (C) 2013 Intel Corp. All rights reserved.

include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES	= libgrldleyna.la

libgrldleyna_la_CFLAGS =	     \
	$(DEPS_DLEYNA_CFLAGS)	     \
	-DG_LOG_DOMAIN=\"GrlDleyna\" \
	-DLOCALEDIR=\"$(localedir)\"

libgrldleyna_la_LIBADD =    \
	$(DEPS_DLEYNA_LIBS)

libgrldleyna_la_LDFLAGS = \
	-no-undefined     \
	-module		  \
	-avoid-version

dbus_built_sources = \
	grl-dleyna-proxy-mediaserver2.h \
	grl-dleyna-proxy-mediaserver2.c \
	grl-dleyna-proxy-mediadevice.h \
	grl-dleyna-proxy-mediadevice.c \
	grl-dleyna-proxy-manager.h \
	grl-dleyna-proxy-manager.c \
	$(NULL)

BUILT_SOURCES = $(dbus_built_sources)

libgrldleyna_la_SOURCES =       \
	$(dbus_built_sources)        \
	grl-dleyna.c                 \
	grl-dleyna-source.c          \
	grl-dleyna-source.h          \
	grl-dleyna-server.c          \
	grl-dleyna-server.h          \
	grl-dleyna-servers-manager.c \
	grl-dleyna-servers-manager.h \
	grl-dleyna-utils.h           \
	grl-dleyna-utils.c           \
	$(NULL)

extdir		= $(GRL_PLUGINS_DIR)
dleynaxmldir	= $(GRL_PLUGINS_DIR)
dleynaxml_DATA	= $(DLEYNA_PLUGIN_ID).xml

# This lets us test the plugin without installing it, because grilo expects the
# .so and .xml files to be in the same directory.
copy-xml-to-libs-dir: libgrldleyna.la
	$(AM_V_at)cp -f $(srcdir)/$(dleynaxml_DATA) $(builddir)/.libs/

all-local: copy-xml-to-libs-dir

grl-dleyna-proxy-mediaserver2.h grl-dleyna-proxy-mediaserver2.c: org.gnome.UPnP.MediaServer2.xml
	$(AM_V_GEN)${GDBUS_CODEGEN} \
		--c-namespace GrlDleyna \
		--generate-c-code grl-dleyna-proxy-mediaserver2 \
		--interface-prefix org.gnome.UPnP. \
		$^

grl-dleyna-proxy-manager.h grl-dleyna-proxy-manager.c: com.intel.dLeynaServer.Manager.xml
	$(AM_V_GEN)${GDBUS_CODEGEN} \
		--c-namespace GrlDleyna \
		--generate-c-code grl-dleyna-proxy-manager \
		--interface-prefix com.intel.dLeynaServer. \
		$^

grl-dleyna-proxy-mediadevice.h grl-dleyna-proxy-mediadevice.c: com.intel.dLeynaServer.MediaDevice.xml
	$(AM_V_GEN)${GDBUS_CODEGEN} \
		--c-namespace GrlDleyna \
		--generate-c-code grl-dleyna-proxy-mediadevice \
		--interface-prefix com.intel.dLeynaServer. \
		$^

EXTRA_DIST +=										\
	com.intel.dLeynaServer.Manager.xml     \
	com.intel.dLeynaServer.MediaDevice.xml \
	org.gnome.UPnP.MediaServer2.xml        \
	$(dleynaxml_DATA)

MAINTAINERCLEANFILES =	\
	*.in		\
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)
