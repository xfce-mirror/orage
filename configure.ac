dnl configure.ac
dnl
dnl xfcalendar - Calendar application for XFce
dnl
dnl 2003 Mickael Graf <korbinus@linux.se>
dnl

AC_INIT([configure])

AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([configure.ac], [0.1.1])

AM_MAINTAINER_MODE

dnl Check for UNIX variants
AC_AIX
AC_ISC_POSIX
AC_MINIX

dnl Check for basic programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

dnl Check for standard header files
AC_HEADER_STDC
AC_CHECK_HEADERS([errno.h])

dnl Check for i18n support
BM_I18N([xfcalendar], [de])

dnl Check for required packages
BM_DEPEND([LIBXFCEGUI4], [libxfcegui4-1.0], [3.91.0])

dnl Check for debugging support
BM_DEBUG_SUPPORT

AC_OUTPUT([
Makefile
po/Makefile.in
src/Makefile
])
