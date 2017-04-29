include Makefile.tests
include bindings/python/Makefile.include
include data/Makefile.include
include examples/Makefile.include
include gom/Makefile.include
include tests/Makefile.include

SUBDIRS = . doc

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
DISTCHECK_CONFIGURE_FLAGS =			\
	--enable-gtk-doc			\
	--enable-introspection			\
	--enable-silent-rules			\
	--with-pythondir='$${libdir}/python-gom-distcheck/site-packages/gi/overrides'

EXTRA_DIST += README COPYING NEWS

dist-hook:
	@if test -d "$(srcdir)/.git"; then                              \
          (cd "$(srcdir)" &&                                            \
           $(top_srcdir)/missing --run git log --stat ) > ChangeLog.tmp \
           && mv -f ChangeLog.tmp $(top_distdir)/ChangeLog              \
           || (rm -f ChangeLog.tmp;                                     \
               echo Failed to generate ChangeLog >&2);                  \
	else                                                            \
	  echo A git checkout is required to generate a ChangeLog >&2;  \
	fi

uninstall-local:
	-rm -r $(gomdocdir)

EXTRA_DIST += bindings/meson.build bindings/python/meson.build gom/meson.build meson.build meson_options.txt
