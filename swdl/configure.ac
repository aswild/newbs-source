AC_INIT([newbs-swdl], [1.0])
AM_INIT_AUTOMAKE([foreign no-dist subdir-objects -Wall -Werror])
AM_SILENT_RULES([yes])

AC_PROG_CC
AC_PROG_CXX

AC_ARG_ENABLE([lto], AS_HELP_STRING([--disable-lto], [Disable Link Time Optimization]))
AM_CONDITIONAL([ENABLE_LTO], [test "$enable_lto" != "no"])

AC_ARG_ENABLE([swdl], AS_HELP_STRING([--disable-swdl], [Build mknImage only]))
AM_CONDITIONAL([ENABLE_SWDL], [test "$enable_swdl" != "no"])

AC_OUTPUT([Makefile])
