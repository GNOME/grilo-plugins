include $(top_srcdir)/gtester.mk

ext_LTLIBRARIES		 = libgrlopensubtitles.la

libgrlopensubtitles_la_CFLAGS =	\
	$(DEPS_OPENSUBTITLES_CFLAGS)	\
	-DG_LOG_DOMAIN=\"GrlOpenSubtitles\" \
	-DLOCALEDIR=\"$(localedir)\"


libgrlopensubtitles_la_LIBADD =	\
	$(DEPS_OPENSUBTITLES_LIBS)

libgrlopensubtitles_la_LDFLAGS = \
	-no-undefined		 \
	-module			 \
	-avoid-version

libgrlopensubtitles_la_SOURCES = grl-opensubtitles.c grl-opensubtitles.h

extdir			= $(GRL_PLUGINS_DIR)
opensubtitlesxmldir	= $(GRL_PLUGINS_DIR)
opensubtitlesxml_DATA	= $(OPENSUBTITLES_PLUGIN_ID).xml

# This lets us test the plugin without installing it,
# because grilo expects the .so and .xml files to be in
# the same directory:
copy-xml-to-libs-dir: libgrlopensubtitles.la
	cp -f $(srcdir)/$(opensubtitlesxml_DATA) $(builddir)/.libs/

all-local: copy-xml-to-libs-dir

EXTRA_DIST += $(opensubtitlesxml_DATA)

MAINTAINERCLEANFILES	 = \
	*.in \
	*~

DISTCLEANFILES = $(MAINTAINERCLEANFILES)

-include $(top_srcdir)/git.mk
