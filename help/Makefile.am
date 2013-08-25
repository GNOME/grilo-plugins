SUBDIRS = examples

include $(top_srcdir)/gnome-doc-utils.make
include $(top_srcdir)/gtester.mk

dist-hook: doc-dist-hook

DOC_MODULE = grilo-plugins
DOC_ENTITIES = legal.xml
DOC_INCLUDES = ../examples/example-tmdb.c
DOC_FIGURES =

DOC_LINGUAS =



# For manual checking:
XMLLINT = xmllint
validate: $(srcdir)/C/grilo-plugins.xml
	$(XMLLINT) --xinclude --postvalid --noout $<
