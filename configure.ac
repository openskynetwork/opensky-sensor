dnl initialize autoconf
AC_INIT(openskyd, 1.0.2)

dnl put autotools files into config subdirectory
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_SRCDIR(src/main.c)
AC_REQUIRE_AUX_FILE([tap-driver.sh])

dnl initialize automake
AM_INIT_AUTOMAKE([gnu subdir-objects -Wall -Werror])
AC_CONFIG_HEADERS(config.h)
AM_MAINTAINER_MODE

dnl required programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AM_PROG_AR

dnl test for std headers
AC_HEADER_STDC

AC_SEARCH_LIBS([clock_gettime],[rt posix4])
AC_CHECK_FUNCS([clock_gettime])

AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h limits.h netdb.h netinet/in.h])
AC_CHECK_HEADERS([stddef.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h])
AC_CHECK_HEADERS([termios.h unistd.h])

AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([ptrdiff_t])

AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_MMAP
AC_CHECK_FUNCS([clock_gettime memchr memmove memset munmap pow setenv socket])
AC_CHECK_FUNCS([strerror strncasecmp strtoul])

AX_PTHREAD

AC_ARG_WITH([check],
  [AS_HELP_STRING([--with-check], [Add test cases using libcheck @<:@check@:>@])],
  [:],[with_check=check])
AS_IF([test x"$with_check" != x"no"],
  [PKG_CHECK_MODULES([CHECK], [check >= 0.9.6],
    [with_check=yes],
    [AS_IF([test x"$with_check" = x"yes"],
      [AC_MSG_ERROR([check required, but not found.])],
      [with_check=no])])])
AM_CONDITIONAL([WITH_CHECK], [test x$with_check = xyes])

AC_ARG_ENABLE([development],
  [AS_HELP_STRING([--enable-development], [Use local files @<:@no@:>@])],
  [case "${enableval}" in
    yes) development=true ;;
    no)  development=false ;;
     *)  AC_MSG_ERROR([bad value ${enableval} for --enable-development]) ;;
   esac], [development=false])
AM_CONDITIONAL([DEVELOPMENT], [test x$development = xtrue])

AC_ARG_ENABLE([test-coverage],
  [AS_HELP_STRING([--enable-test-coverage], [Enable Test Coverage @<:@no@:>@])],
  [case "${enableval}" in
    yes) test_coverage=true ;;
    no)  test_coverage=false ;;
     *)  AC_MSG_ERROR([bad value ${enableval} for --enable-test-coverage]) ;;
   esac], [test_coveraget=false])
AM_CONDITIONAL([TEST_COVERAGE], [test x$test_coverage = xtrue])

AC_ARG_ENABLE([network],
  [AS_HELP_STRING([--enable-network], [Build network input layer @<:@no@:>@])],
  [case "${enableval}" in
    yes) network=true ;;
    no)  network=false ;;
     *)  AC_MSG_ERROR([bad value ${enableval} for --enable-network]) ;;
   esac], [network=false])
AM_CONDITIONAL([WANT_NETWORK], [test x$network = xtrue])

AC_ARG_ENABLE([uart],
  [AS_HELP_STRING([--enable-uart], [Build uart input layer @<:@yes@:>@])],
  [case "${enableval}" in
    yes) uart=true ;;
    no)  uart=false ;;
     *)  AC_MSG_ERROR([bad value ${enableval} for --enable-uart]) ;;
   esac], [uart=true])
AM_CONDITIONAL([WANT_UART], [test x$uart = xtrue])

AC_ARG_ENABLE([dummy],
  [AS_HELP_STRING([--enable-dummy], [Build dummy input layer @<:@no@:>@])],
  [case "${enableval}" in
    yes) dummy=true ;;
    no)  dummy=false ;;
     *)  AC_MSG_ERROR([bad value ${enableval} for --enable-dummy]) ;;
   esac], [dummy=false])
AM_CONDITIONAL([WANT_DUMMY], [test x$dummy = xtrue])

AC_ARG_ENABLE([wd],
  [AS_HELP_STRING([--enable-wd], [Build Watchdog Daemon @<:@no@:>@])],
  [case "${enableval}" in
    yes) wd=true ;;
    no)  wd=false ;;
     *)  AC_MSG_ERROR([bad value ${enableval} for --enable-wd]) ;;
   esac], [wd=false])
AM_CONDITIONAL([WANT_WD], [test x$wd = xtrue])

AC_ARG_ENABLE([talkback],
  [AS_HELP_STRING([--enable-talkback], [Enable talkback @<:@yes@:>@])],
  [case "${enableval}" in
    yes) talkback=true ;;
    no)  talkback=false ;;
     *)  AC_MSG_ERROR([bad value ${enableval} for --enable-talkback]) ;;
   esac], [talkback=true])
AM_CONDITIONAL([TALKBACK], [test x$talkback = xtrue])

AC_ARG_ENABLE([cleanup],
  [AS_HELP_STRING([--enable-cleanup], [Add cleanup routines @<:@yes@:>@])],
  [case "${enableval}" in
    yes) cleanup=yes ;;
    no)  cleanup=no ;;
     *)  AC_MSG_ERROR([bad value ${enableval} for --enable-cleanup]) ;;
   esac], [cleanup=yes])
AS_IF([test x"$cleanup" = x"yes"],
  [AC_DEFINE([CLEANUP], [1], [Add cleanup routines])])

AC_ARG_WITH([systemd],
  [AS_HELP_STRING([--with-systemd], [Compile with systemd integration @<:@check@:>@])],
  [:],[with_systemd=check])
AS_IF([test x"$with_systemd" != x"no"],
  [PKG_CHECK_MODULES([systemd],
    [systemd],
    [:],
    [AS_IF([test x"$with_systemd" = x"yes"],
      [AC_MSG_ERROR([systemd required, but not found.])],
      [with_systemd=no])])])
AS_IF([test x"$with_systemd" != x"no"],
  [AC_MSG_CHECKING(location of systemd)])
AS_IF([test x"$with_systemd" = x"yes" -o x"$with_systemd" = x"check"],
  [systemd_PREFIX=$(pkg-config --variable=prefix systemd)],
  [systemd_PREFIX=$with_systemd])
AS_IF([test x"$with_systemd" != x"no"],
  [AC_MSG_RESULT($systemd_PREFIX);
   AC_DEFINE_UNQUOTED([WITH_SYSTEMD], ["$systemd_PREFIX"], [Enable systemd integration])])

AC_ARG_WITH([pacman],
  [AS_HELP_STRING([--with-pacman], [Compile with pacman integration @<:@check@:>@])],
  [:],[with_pacman=check])
AS_IF([test x"$with_pacman" != x"no"],
  [AC_MSG_CHECKING(for pacman);
    AS_IF([test -e "/usr/bin/pacman"],
    [AC_MSG_RESULT(found); with_pacman=yes],
    [AC_MSG_RESULT(not found);
     AS_IF([test x"$with_pacman" = x"check"],[with_pacman=no],
     [AC_MSG_ERROR([pacman required, but not found.])])])])
AS_IF([test x"$with_pacman" != x"no"],
  [AC_DEFINE([WITH_PACMAN], [1], [Enable pacman integration])])

AS_IF([test x$network = xfalse -a x$uart = xfalse -a x$wd = xfalse -a x$dummy = xfalse],
  [AC_MSG_ERROR([No input layer chosen])])

AS_IF([test x$development = xtrue],
  [AC_DEFINE([DEVELOPMENT], [], [Enable development features])])

AS_IF([test x$talkback = xtrue],
  [AC_DEFINE([TALKBACK], [], [Enable talkback component])])

dnl output directive
AC_OUTPUT([Makefile src/Makefile src/test/Makefile])
