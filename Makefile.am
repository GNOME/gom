include Makefile.include
include Makefile.tests
include data/Makefile.include
include gom/Makefile.include
include tests/Makefile.include

SUBDIRS = . doc

ACLOCAL_AMFLAGS = -I build/m4 ${ACLOCAL_FLAGS}
DISTCHECK_CONFIGURE_FLAGS = --enable-maintainer-flags --enable-gtk-doc --enable-introspection

gomdocdir = ${prefix}/doc/gom-1.0
gomdoc_DATA =
gomdoc_DATA += README
gomdoc_DATA += COPYING
gomdoc_DATA += AUTHORS
gomdoc_DATA += ChangeLog
gomdoc_DATA += INSTALL
gomdoc_DATA += NEWS

EXTRA_DIST += $(gomdoc_DATA)

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
