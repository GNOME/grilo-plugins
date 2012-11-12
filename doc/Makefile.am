DOC_MODULE = grilo-plugins
DOC_ENTITIES =
DOC_INCLUDES = ../../examples/example-tmdb.c
DOC_FIGURES =

DOC_LINGUAS =

include $(top_srcdir)/gnome-doc-utils.make


# For manual checking:
XMLLINT = xmllint
validate: $(srcdir)/C/grilo-plugins.xml
	$(XMLLINT) --xinclude --postvalid --noout $<

