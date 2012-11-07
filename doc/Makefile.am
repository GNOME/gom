## Process this file with automake to produce Makefile.in

test:

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=gom

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gom-docs.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=

# Extra options to pass to gtkdoc-scangobj
SCANGOBJ_OPTIONS=--type-init-func="g_type_init()" 

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=$(top_srcdir)/gom

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/gom/*.h
CFILE_GLOB=$(top_srcdir)/gom/*.c

# Header files to ignore when scanning
IGNORE_HFILES=gom.h

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
INCLUDES = \
	-I$(top_srcdir) 	\
	$(GOBJECT_CFLAGS)

GTKDOC_LIBS = \
	$(GOBJECT_LIBS) \
	$(top_builddir)/libgom-1.0.la

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files = version.xml

# Images to copy into HTML directory
HTML_IMAGES =

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS = \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gio \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/glib \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gobject

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += version.xml.in

