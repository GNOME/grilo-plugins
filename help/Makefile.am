SUBDIRS = examples

@YELP_HELP_RULES@
include $(top_srcdir)/gtester.mk

HELP_ID = grilo-plugins
HELP_FILES = grilo-plugins.xml
HELP_EXTRA = legal.xml
#DOC_INCLUDES = ../examples/example-tmdb.c
HELP_MEDIA =
HELP_LINGUAS =

# For manual checking:
XMLLINT = xmllint
validate: $(srcdir)/C/grilo-plugins.xml
	$(XMLLINT) --xinclude --postvalid --noout $<
