## Process this file with automake to produce Makefile.in

# We require automake 1.6 at least.
AUTOMAKE_OPTIONS = 1.6

# This is a blank Makefile.am for using gtk-doc.
# Copy this to your project's API docs directory and modify the variables to
# suit your project. See the GTK+ Makefiles in gtk+/docs/reference for examples
# of using the various options.

# The name of the module, e.g. 'glib'.
DOC_MODULE=grilo-plugins

# Uncomment for versioned docs and specify the version of the module, e.g. '2'.
#DOC_MODULE_VERSION=2


# The top-level SGML file. You can change this if you want to.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# The directory containing the source code. Relative to $(srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting the functions and macros.
# e.g. DOC_SOURCE_DIR=../../../gtk
DOC_SOURCE_DIR=$(top_srcdir)/src

# Extra options to pass to gtkdoc-scangobj. Not normally needed.
SCANGOBJ_OPTIONS=

# Extra options to supply to gtkdoc-scan.
# e.g. SCAN_OPTIONS=--deprecated-guards="GTK_DISABLE_DEPRECATED"
SCAN_OPTIONS=

# Extra options to supply to gtkdoc-mkdb.
# e.g. MKDB_OPTIONS=--sgml-mode --output-format=xml
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-mktmpl
# e.g. MKTMPL_OPTIONS=--only-section-tmpl
MKTMPL_OPTIONS=

# Extra options to supply to gtkdoc-mkhtml
MKHTML_OPTIONS=

# Extra options to supply to gtkdoc-fixref. Not normally needed.
# e.g. FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html
FIXXREF_OPTIONS=--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html

# Used for dependencies. The docs will be rebuilt if any of these change.
# e.g. HFILE_GLOB=$(top_srcdir)/gtk/*.h
# e.g. CFILE_GLOB=$(top_srcdir)/gtk/*.c
HFILE_GLOB=
CFILE_GLOB=

# Extra header to include when scanning, which are not under DOC_SOURCE_DIR
# e.g. EXTRA_HFILES=$(top_srcdir}/contrib/extra.h
EXTRA_HFILES=

# Header files to ignore when scanning. Use base file name, no paths
# e.g. IGNORE_HFILES=gtkdebug.h gtkintl.h
IGNORE_HFILES=config.h

# Images to copy into HTML directory.
# e.g. HTML_IMAGES=$(top_srcdir)/gtk/stock-icons/stock_about_24.png
HTML_IMAGES=

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
# e.g. content_files=running.sgml building.sgml changes-2.0.sgml
content_files=overview.xml

# SGML files where gtk-doc abbrevations (#GtkWidget) are expanded
# These files must be listed here *and* in content_files
# e.g. expand_content_files=running.sgml
expand_content_files=

# CFLAGS and LDFLAGS for compiling gtkdoc-scangobj with your library.
# Only needed if you are using gtkdoc-scangobj to dynamically query widget
# signals and properties.
# e.g. GTKDOC_CFLAGS=-I$(top_srcdir) -I$(top_builddir) $(GTK_DEBUG_FLAGS)
# e.g. GTKDOC_LIBS=$(top_builddir)/gtk/$(gtktargetlib)
GTKDOC_CFLAGS=-I$(top_srcdir)/src \
	$(DEPS_APPLE_TRAILERS_CFLAGS) \
	$(DEPS_BLIPTV_CFLAGS) \
	$(DEPS_BOOKMARKS_CFLAGS) \
	$(DEPS_DMAP_CFLAGS) \
	$(DEPS_FAKE_METADATA_CFLAGS) \
	$(DEPS_FILESYSTEM_CFLAGS) \
	$(DEPS_FLICKR_CFLAGS) \
	$(DEPS_GRAVATAR_CFLAGS) \
	$(DEPS_JAMENDO_CFLAGS) \
	$(DEPS_LASTFM_ALBUMART_CFLAGS) \
	$(DEPS_LOCAL_METADATA_CFLAGS) \
	$(DEPS_OPTICAL_MEDIA_CFLAGS) \
	$(DEPS_PODCASTS_CFLAGS) \
	$(DEPS_SHOUTCAST_CFLAGS) \
	$(DEPS_TMDB_CFLAGS) \
	$(DEPS_TRACKER_CFLAGS) \
	$(DEPS_UPNP_CFLAGS) \
	$(DEPS_VIMEO_CFLAGS) \
	$(DEPS_YOUTUBE_CFLAGS)

GTKDOC_LIBS=\
	$(top_builddir)/src/apple-trailers/libgrlappletrailers.la \
	$(DEPS_APPLE_TRAILERS_LIBS) \
	$(top_builddir)/src/bliptv/libgrlbliptv.la \
	$(DEPS_BLIPTV_LIBS) \
	$(top_builddir)/src/bookmarks/libgrlbookmarks.la \
	$(DEPS_BOOKMARKS_LIBS) \
	$(top_builddir)/src/dmap/libgrldmap.la \
	$(DEPS_DMAP_LIBS) \
	$(top_builddir)/src/fake-metadata/libgrlfakemetadata.la \
	$(DEPS_FAKE_METADATA_LIBS) \
	$(top_builddir)/src/filesystem/libgrlfilesystem.la \
	$(DEPS_FILESYSTEM_LIBS) \
	$(top_builddir)/src/flickr/libgrlflickr.la \
	$(DEPS_FLICKR_LIBS) \
	$(top_builddir)/src/gravatar/libgrlgravatar.la \
	$(DEPS_GRAVATAR_LIBS) \
	$(top_builddir)/src/jamendo/libgrljamendo.la \
	$(DEPS_JAMENDO_LIBS) \
	$(top_builddir)/src/lastfm-albumart/libgrllastfm-albumart.la \
	$(DEPS_LASTFM_ALBUMART_LIBS) \
	$(top_builddir)/src/metadata-store/libgrlmetadatastore.la \
	$(DEPS_LOCAL_METADATA_LIBS) \
	$(top_builddir)/src/local-metadata/libgrllocalmetadata.la \
	$(DEPS_METADATA_STORE_LIBS) \
	$(top_builddir)/src/optical-media/libgrloptical-media.la \
	$(DEPS_OPTICAL_MEDIA_LIBS) \
	$(top_builddir)/src/podcasts/libgrlpodcasts.la \
	$(DEPS_PODCASTS_LIBS) \
	$(top_builddir)/src/shoutcast/libgrlshoutcast.la \
	$(DEPS_SHOUTCAST_LIBS) \
	$(top_builddir)/src/tmdb/libgrltmdb.la \
	$(DEPS_TMDB_LIBS) \
	$(top_builddir)/src/tracker/libgrltracker.la \
	$(DEPS_TRACKER_LIBS) \
	$(top_builddir)/src/upnp/libgrlupnp.la \
	$(DEPS_UPNP_LIBS) \
	$(top_builddir)/src/vimeo/libgrlvimeo.la \
	$(DEPS_VIMEO_LIBS) \
	$(top_builddir)/src/youtube/libgrlyoutube.la \
	$(DEPS_YOUTUBE_LIBS)

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

# Other files to distribute
# e.g. EXTRA_DIST += version.xml.in
EXTRA_DIST +=

# Files not to distribute
# for --rebuild-types in $(SCAN_OPTIONS), e.g. $(DOC_MODULE).types
# for --rebuild-sections in $(SCAN_OPTIONS) e.g. $(DOC_MODULE)-sections.txt
#DISTCLEANFILES +=

# Comment this out if you want your docs-status tested during 'make check'
if ENABLE_GTK_DOC
#TESTS_ENVIRONMENT = cd $(srcsrc) &&
#TESTS = $(GTKDOC_CHECK)
endif

-include $(top_srcdir)/git.mk
