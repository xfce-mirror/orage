dnl Orage - Calendar application for Xfce
dnl
dnl Copyright (c) 2003-2025
dnl         The Xfce development team. All rights reserved.
dnl
dnl Written for Xfce by Juha Kautto <juha@xfce.org>
dnl

dnl Version information
XDT_VERSION_INIT([4.20.2])

m4_define([glib_minimum_version], [2.58.0])
m4_define([gtk_minimum_version], [3.24.0])
m4_define([xfce_minimum_version], [4.20.0])
m4_define([notify_minimum_version], [0.7.0])
m4_define([libical_minimum_version], [3.0])


dnl # DO NOT MODIFY ANYTHING BELOW THIS LINE, UNLESS YOU KNOW WHAT
dnl # YOU ARE DOING.


dnl Initialize autoconf
AC_COPYRIGHT([Copyright (c) 2003-2025
        The Xfce development team. All rights reserved.

Written for Xfce by Juha Kautto <juha@xfce.org>.])
AC_INIT([orage], [xdt_version], [https://gitlab.xfce.org/apps/orage])
AC_CONFIG_MACRO_DIRS([m4])
AC_PREREQ([2.69])
AC_REVISION([xdt_version_build])

dnl Initialize automake
AM_INIT_AUTOMAKE(1.8 no-dist-gzip dist-bzip2 tar-ustar foreign)
AM_SILENT_RULES([yes])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE()

AC_PROG_CC()

dnl Check for UNIX variants
AC_USE_SYSTEM_EXTENSIONS()
AC_SEARCH_LIBS([strerror], [cposix])
AM_CONDITIONAL([HAVE_CYGWIN], [test "`uname | grep \"CYGWIN\"`" != ""])

dnl Check for basic programs
AC_PROG_INSTALL()
AC_PROG_SED()
AC_PROG_YACC()
AC_PROG_LN_S()
m4_version_prereq([2.70], [AC_PROG_LEX(noyywrap)], [AC_PROG_LEX])
AM_PROG_CC_C_O()

dnl Initialize libtool
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

dnl Check for standard header files
AC_CHECK_HEADERS([errno.h time.h sys/types.h unistd.h wctype.h])

dnl Checks for typedefs, structures, and compiler characteristics (libical)
AC_C_CONST()
AC_TYPE_SIZE_T()
AC_STRUCT_TM()

dnl Checks for library functions (libical)
AC_CHECK_FUNCS([gmtime_r])

dnl Check for i18n support
GETTEXT_PACKAGE="$PACKAGE"
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Name of default gettext domain])
AC_SUBST([GETTEXT_PACKAGE])

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])

dnl Check for required packages

dnl *******************************
dnl *** Check for X11 installed ***
dnl *******************************
XDT_CHECK_LIBX11_REQUIRE()

XDT_CHECK_PACKAGE([GLIB], [glib-2.0], [glib_minimum_version])
XDT_CHECK_PACKAGE([GIO], [gio-2.0], [glib_minimum_version])
XDT_CHECK_PACKAGE([LIBGTK], [gtk+-3.0], [gtk_minimum_version])
XDT_CHECK_PACKAGE([LIBXFCE4UTIL], [libxfce4util-1.0], [xfce_minimum_version])
XDT_CHECK_PACKAGE([LIBICAL], [libical], [libical_minimum_version])
XDT_CHECK_PACKAGE([LIBXFCE4UI], [libxfce4ui-2], [xfce_minimum_version])

if test "$LIBICAL_VERSION" = "3.0.15"; then
    AC_MSG_ERROR(["unsupported libical version $LIBICAL_VERSION, see https://gitlab.xfce.org/apps/orage/-/issues/20"])
fi

dnl **********************************
dnl *** check if we have _NL_TIME_FIRST_WEEKDAY 
dnl *** note that it is an enum and not a define
dnl **********************************
AC_MSG_CHECKING([for _NL_TIME_FIRST_WEEKDAY])
AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <langinfo.h>], [
char c;
c = *((unsigned char *)  nl_langinfo(_NL_TIME_FIRST_WEEKDAY));
])], nl_ok=yes, nl_ok=no)
AC_MSG_RESULT($nl_ok)
if test "$nl_ok" = "yes"; then
  AC_DEFINE([HAVE__NL_TIME_FIRST_WEEKDAY], [1],
      [Define if _NL_TIME_FIRST_WEEKDAY is available])
fi

dnl **************************************
dnl *** Optional support for libnotify ***
dnl **************************************
XDT_CHECK_OPTIONAL_PACKAGE([NOTIFY], [libnotify],
                           [notify_minimum_version], [libnotify],
                           [LIBNOTIFY support])

dnl Check for debugging support
XDT_FEATURE_DEBUG([xdt_debug_default])

dnl ************************************************
dnl *** Optional support for automatic archiving ***
dnl ************************************************
have_archive="no"
AC_ARG_ENABLE([archive],
    AS_HELP_STRING([--enable-archive], [use automatic archiving (defaut=yes)])
    AS_HELP_STRING([--disable-archive], [do not use automatic archiving]),
    [], [enable_archive=yes])
if test x"$enable_archive" = x"yes"; then
    AC_DEFINE([HAVE_ARCHIVE], [1], [Define to enable archiving])
    have_archive="yes"
fi

dnl ************************************************
dnl *** Support for Orage tray icon              ***
dnl ************************************************
have_x11_tray_icon="no"
AC_ARG_ENABLE([x11_tray_icon],
    AS_HELP_STRING([--enable-x11-tray-icon], [add X11 tray icon, not comptible with Wayland (defaut=no)])
    AS_HELP_STRING([--disable-x11-tray-icon], [do not add tray icon, Wayland compatible]),
    [], [enable_x11_tray_icon=no])
if test x"$enable_x11_tray_icon" = x"yes"; then
    AC_DEFINE([HAVE_X11_TRAY_ICON], [1], [Define to enable X11 tray icon])
    have_x11_tray_icon="yes"
fi
AM_CONDITIONAL([HAVE_X11_TRAY_ICON], [test x"$enable_x11_tray_icon" = x"yes"])

dnl TODO: Add switch or condition to enable sync code
AC_DEFINE([ENABLE_SYNC], [1], [Enable experimental sync code])

AC_CONFIG_FILES([
Makefile
icons/Makefile
icons/16x16/Makefile
icons/24x24/Makefile
icons/32x32/Makefile
icons/48x48/Makefile
icons/64x64/Makefile
icons/128x128/Makefile
icons/scalable/Makefile
po/Makefile.in
sounds/Makefile
src/Makefile
plugin/Makefile
themes/Makefile
])
AC_OUTPUT

dnl ***************************
dnl *** Print configuration ***
dnl ***************************
echo
echo "Build Configuration:"
if test x"$NOTIFY_FOUND" = x"yes"; then
echo "* LIBNOTIFY support:         yes"
else
echo "* LIBNOTIFY support:         no"
fi
echo "* Automatic archiving:       $have_archive"
echo "* X11 tray icon:             $have_x11_tray_icon"
echo
