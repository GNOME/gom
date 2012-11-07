dnl **************************************************************************
dnl Versioning Macros
dnl **************************************************************************
m4_define([gom_major_version], [0])
m4_define([gom_minor_version], [1])
m4_define([gom_micro_version], [10])
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

AM_INIT_AUTOMAKE([-Wno-portability])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
AS_AM_REALLY_SILENT

AC_PROG_CC
AM_PATH_GLIB_2_0

LT_PREREQ([2.2])
LT_INIT


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
PKG_CHECK_MODULES(GIO,     [gio-2.0 >= 2.26])
PKG_CHECK_MODULES(GOBJECT, [gobject-2.0 >= 2.26])
PKG_CHECK_MODULES(SQLITE3, [sqlite3 >= 3.6])


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
		CFLAGS="$CFLAGS -g"
		CFLAGS="$CFLAGS -Wall"
		CFLAGS="$CFLAGS -Werror"
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


dnl **************************************************************************
dnl Unit Tests
dnl **************************************************************************
AC_PATH_PROG([GTESTER], [gtester])
AC_PATH_PROG([GTESTER_REPORT], [gtester-report])
AM_CONDITIONAL(ENABLE_GLIB_TEST, test "x$enable_glibtest" = "xyes")


dnl **************************************************************************
dnl API Documentation
dnl **************************************************************************
GTK_DOC_CHECK([1.14],[--flavour no-tmpl])


dnl **************************************************************************
dnl Miscellaneous
dnl **************************************************************************
GOBJECT_INTROSPECTION_CHECK([0.6.7])


dnl **************************************************************************
dnl Output
dnl **************************************************************************
AC_CONFIG_FILES([
	Makefile
	data/gom-1.0.pc
	doc/version.xml
	doc/Makefile
])
AC_OUTPUT

echo ""
echo "Gom $VERSION"
echo ""
echo "  Prefix.....................: ${prefix}"
echo "  Debug Level................: ${enable_debug}"
echo "  Compiler Flags.............: ${CFLAGS}"
echo "  Enable API Reference.......: ${enable_gtk_doc}"
echo "  Enable Test Suite..........: ${enable_glibtest}"
echo ""
