dnl ************************************************************************
dnl Versioning Macros
dnl ************************************************************************
m4_define([gom_major_version], [0])
m4_define([gom_minor_version], [1])
m4_define([gom_micro_version], [1])
m4_define([gom_version],
          [gom_major_version.gom_minor_version.gom_micro_version])
m4_define([gom_binary_age], [m4_eval(100 * gom_minor_version +
           gom_micro_version)])
m4_define([lt_current], [m4_eval(100 * gom_minor_version +
           gom_micro_version - gom_interface_age)])
m4_define([lt_revision], [gom_interface_age])
m4_define([lt_age], [m4_eval(gom_binary_age - gom_interface_age)])


dnl ************************************************************************
dnl Initialize Automake
dnl ************************************************************************
AC_PREREQ([2.64])
AC_INIT([gom],[gom_version])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([build/m4])

AM_INIT_AUTOMAKE([1.10])
AM_MAINTAINER_MODE([enable])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
AS_AM_REALLY_SILENT

# Check for programs
AC_PROG_CC
AM_PROG_CC_C_O
AM_PATH_GLIB_2_0
GLIB_GSETTINGS

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT


dnl ************************************************************************
dnl Versioning Definitions
dnl ************************************************************************
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


dnl ************************************************************************
dnl Check for Required Modules
dnl ************************************************************************
PKG_CHECK_MODULES(GOBJECT, [gobject-2.0 >= 2.26])


dnl ************************************************************************
dnl Enable extra debugging options
dnl ************************************************************************
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
		test "$cflags_set" = set || CFLAGS="$CFLAGS -g"
		GOM_DEBUG_CFLAGS="-DGOM_TRACE"
	],
	[minimum], [
		GOM_DEBUG_CFLAGS="-DG_DISABLE_CAST_CHECKS"
	],
	[no], [
		GOM_DEBUG_CFLAGS="-DG_DISABLE_ASSERT -DG_DISABLE_CHECKS -DG_DISABLE_CAST_CHECKS"
	],
	[AC_MSG_ERROR([Unknown argument to --enable-debug])]
)
AC_SUBST(GOM_DEBUG_CFLAGS)


dnl ************************************************************************
dnl Unit Tests
dnl ************************************************************************
AC_PATH_PROG([GTESTER], [gtester])
AC_PATH_PROG([GTESTER_REPORT], [gtester-report])
AM_CONDITIONAL(ENABLE_GLIB_TEST, test "x$enable_glibtest" = "xyes")


dnl ************************************************************************
dnl Miscellaneous
dnl ************************************************************************
GTK_DOC_CHECK([1.11])
GOBJECT_INTROSPECTION_CHECK([0.6.7])
SHAMROCK_EXPAND_LIBDIR
SHAMROCK_EXPAND_BINDIR
SHAMROCK_EXPAND_DATADIR


dnl ************************************************************************
dnl Output
dnl ************************************************************************
AC_CONFIG_FILES([
	Makefile
	data/gom-1.0.pc
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
