AC_INIT([newbs-swdl], [1.0])
AM_INIT_AUTOMAKE([foreign no-dist subdir-objects -Wall -Werror])
AM_SILENT_RULES([yes])

AC_CANONICAL_HOST
AC_PROG_CC
AC_PROG_CXX

AC_ARG_ENABLE([lto], AS_HELP_STRING([--disable-lto], [Disable Link Time Optimization]))
AM_CONDITIONAL([ENABLE_LTO], [test "$enable_lto" != "no"])

AC_ARG_ENABLE([swdl], AS_HELP_STRING([--disable-swdl], [Build mknImage only]))
AM_CONDITIONAL([ENABLE_SWDL], [test "$enable_swdl" != "no"])

AC_ARG_ENABLE([swdl-test], AS_HELP_STRING([--enable-swdl-test],
                               [Test swdl code, do not write files or reboot (default for x86)]))
AS_CASE([$host_cpu], [x86*],
        [AC_MSG_NOTICE([Compiling for x86 host, assume --enable-swdl-test])
         enable_swdl_test="yes"])
AM_CONDITIONAL([SWDL_TEST], [test "$enable_swdl_test" = "yes"])

AC_OUTPUT([Makefile])
