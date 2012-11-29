include $(top_srcdir)/gnome-doc-utils.make
dist-hook: doc-dist-hook

DOC_MODULE = grilo-plugins
DOC_ENTITIES = legal.xml
DOC_INCLUDES = ../../examples/example-tmdb.c
DOC_FIGURES =

DOC_LINGUAS =



# For manual checking:
XMLLINT = xmllint
validate: $(srcdir)/C/grilo-plugins.xml
	$(XMLLINT) --xinclude --postvalid --noout $<
