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

-include $(top_srcdir)/git.mk
