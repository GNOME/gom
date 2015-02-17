dnl **************************************************************************
dnl Versioning Macros
dnl **************************************************************************
m4_define([gom_major_version], [0])
m4_define([gom_minor_version], [2])
m4_define([gom_micro_version], [2])
m4_define([gom_version],
          [gom_major_version.gom_minor_version.gom_micro_version])
m4_define([gom_binary_age], [m4_eval(100 * gom_minor_version +
           gom_micro_version)])
m4_define([lt_current], [m4_eval(100 * gom_minor_version +
           gom_micro_version - gom_interface_age)])
m4_define([lt_revision], [gom_interface_age])
m4_define([lt_age], [m4_eval(gom_binary_age - gom_interface_age)])


dnl **************************************************************************
dnl Initialize Automake
dnl **************************************************************************
AC_PREREQ([2.64])
AC_INIT([gom],[gom_version])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([subdir-objects -Wno-portability no-dist-gzip dist-xz])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AS_AM_REALLY_SILENT

AC_PROG_CC
AM_PATH_GLIB_2_0

LT_PREREQ([2.2])
LT_INIT

GNOME_COMPILE_WARNINGS([maximum])

dnl **************************************************************************
dnl Versioning Definitions
dnl **************************************************************************
GOM_MAJOR_VERSION=gom_major_version
GOM_MINOR_VERSION=gom_minor_version
GOM_MICRO_VERSION=gom_micro_version
GOM_API_VERSION=1.0
GOM_VERSION=gom_version
AC_SUBST(GOM_MAJOR_VERSION)
AC_SUBST(GOM_MINOR_VERSION)
AC_SUBST(GOM_MICRO_VERSION)
AC_SUBST(GOM_API_VERSION)
AC_SUBST(GOM_VERSION)


dnl **************************************************************************
dnl Check for Required Modules
dnl **************************************************************************
PKG_CHECK_MODULES(GIO,     [gio-2.0 >= 2.36])
PKG_CHECK_MODULES(GOBJECT, [gobject-2.0 >= 2.36])
PKG_CHECK_MODULES(SQLITE3, [sqlite3 >= 3.7])


dnl **************************************************************************
dnl Enable extra debugging options
dnl **************************************************************************
m4_define([debug_default],
	  [m4_if(m4_eval(gom_minor_version % 2), [1],
	  [yes], [minimum])])
AC_ARG_ENABLE([debug],
	      [AS_HELP_STRING([--enable-debug=@<:@no/minimum/yes@:>@],
	      		      [turn on debugging @<:@default=debug_default@:>@])],
	      		      [],
	      		      [enable_debug=debug_default])
AS_CASE([$enable_debug],
	[yes], [
		CFLAGS="$CFLAGS -Wall"
		CFLAGS="$CFLAGS -Wold-style-definition"
		CFLAGS="$CFLAGS -Wdeclaration-after-statement"
		CFLAGS="$CFLAGS -Wredundant-decls"
		CFLAGS="$CFLAGS -Wmissing-noreturn"
		CFLAGS="$CFLAGS -Wcast-align"
		CFLAGS="$CFLAGS -Wwrite-strings"
		CFLAGS="$CFLAGS -Winline"
		CFLAGS="$CFLAGS -Wformat-nonliteral"
		CFLAGS="$CFLAGS -Wformat-security"
		CFLAGS="$CFLAGS -Wswitch-enum"
		CFLAGS="$CFLAGS -Wswitch-default"
		CFLAGS="$CFLAGS -Winit-self"
		CFLAGS="$CFLAGS -Wmissing-include-dirs"
		CFLAGS="$CFLAGS -Wundef"
		CFLAGS="$CFLAGS -Waggregate-return"
		CFLAGS="$CFLAGS -Wmissing-format-attribute"
		CFLAGS="$CFLAGS -Wnested-externs"
		CFLAGS="$CFLAGS -Wshadow"
	],
	[minimum], [
		CFLAGS="$CFLAGS -DG_DISABLE_CAST_CHECKS"
	],
	[no], [
		CFLAGS="$CFLAGS -DG_DISABLE_ASSERT"
		CFLAGS="$CFLAGS -DG_DISABLE_CHECKS"
		CFLAGS="$CFLAGS -DG_DISABLE_CAST_CHECKS"
	],
	[AC_MSG_ERROR([Unknown argument to --enable-debug])]
)

dnl -------------------------------------------------------------------
dnl Use intltools.
dnl -------------------------------------------------------------------
IT_PROG_INTLTOOL([0.40.0])

AC_SUBST([GETTEXT_PACKAGE], [gom])
AC_DEFINE([GETTEXT_PACKAGE], [PACKAGE_TARNAME], [Define to the gettext package name.])


dnl gettext library
AC_CHECK_LIB([gettextpo], [po_message_is_format],
             [GETTEXT_LIBS=-lgettextpo], [
              AC_MSG_ERROR(Missing gettext library)])

AC_SUBST([GETTEXT_LIBS])


dnl **************************************************************************
dnl Unit Tests
dnl **************************************************************************
AC_PATH_PROG([GTESTER], [gtester])
AC_PATH_PROG([GTESTER_REPORT], [gtester-report])
PKG_CHECK_MODULES(PIXBUF, [gdk-pixbuf-2.0], [], [enable_glibtest=no])
AM_CONDITIONAL(ENABLE_GLIB_TEST, test "x$enable_glibtest" = "xyes")


dnl **************************************************************************
dnl API Documentation
dnl **************************************************************************
GTK_DOC_CHECK([1.14])


dnl **************************************************************************
dnl Miscellaneous
dnl **************************************************************************
GOBJECT_INTROSPECTION_CHECK([1.30.0])


dnl **************************************************************************
dnl Output
dnl **************************************************************************
AC_CONFIG_FILES([
	Makefile
	data/gom-1.0.pc
	doc/version.xml
	doc/Makefile
	po/Makefile.in
])
AC_OUTPUT

echo ""
echo "Gom $VERSION"
echo ""
echo "  Prefix.....................: ${prefix}"
echo "  Debug Level................: ${enable_debug}"
echo "  Compiler Flags.............: ${CFLAGS}"
echo "  Enable API Reference.......: ${enable_gtk_doc}"
echo "  Enable Introspection.......: ${found_introspection}"
echo "  Enable Test Suite..........: ${enable_glibtest}"
echo ""
