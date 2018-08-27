AC_INIT([newbs-swdl], [1.0])
AM_INIT_AUTOMAKE([foreign no-dist -Wall -Werror])
AM_SILENT_RULES([yes])

AC_PROG_CC

AC_ARG_ENABLE([lto], AS_HELP_STRING([--disable-lto], [Disable Link Time Optimization]))
AM_CONDITIONAL([ENABLE_LTO], [test "$enable_lto" != "no"])

AC_ARG_ENABLE([swdl], AS_HELP_STRING([--disable-swdl], [Build mknImage only, do not install SWDL programs]))
AM_CONDITIONAL([INSTALL_SWDL], [test "$enable_swdl" != "no"])

AC_CONFIG_FILES([
    Makefile
    newbs-swdl
    newbs-swdl-functions.sh
])
AC_OUTPUT
