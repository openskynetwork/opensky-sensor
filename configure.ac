dnl initialize autoconf
AC_INIT(openskyd, 0.7.8)

dnl put autotools files into config subdirectory
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_SRCDIR(src/main.c)

dnl initialize automake
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS(config.h)
AM_MAINTAINER_MODE

dnl required programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

dnl test for std headers
AC_HEADER_STDC

AC_SEARCH_LIBS([clock_gettime],[rt posix4])

AX_PTHREAD

AC_ARG_ENABLE([development],
  [AS_HELP_STRING([--enable-development], [Use local files @<:@no@:>@])],
  [case "${enableval}" in
    yes) development=true ;;
    no)  development=false ;;
     *)  AC_MSG_ERROR([bad value ${enableval} for --enable-development]) ;;
   esac], [development=false])
AM_CONDITIONAL([DEVELOPMENT], [test x$development = xtrue])

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

AC_ARG_ENABLE([bb-black],
  [AS_HELP_STRING([--enable-bb-black], [Build openskyd for BeagleBone Black @<:@no@:>@])],
  [case "${enableval}" in
    yes) bbblack=yes ;;
    no)  bbblack=no ;;
     *)  AC_MSG_ERROR([bad value ${enableval} for --enable-bb-black]) ;;
   esac], [bbblack=no])
AS_IF([test x"$bbblack" = x"yes"],
  [AC_DEFINE([BEAGLEBONE_BLACK], [1], [Build for BeagleBone Black])])

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

AS_IF([test x$network = xfalse -a x$uart = xfalse -a x$wd = xfalse],
  [AC_MSG_ERROR([No input layer chosen])])

AS_IF([test x$development = xtrue],
  [AC_DEFINE([DEVELOPMENT], [], [Enable development features])])

AS_IF([test x$talkback = xtrue],
  [AC_DEFINE([TALKBACK], [], [Enable talkback component])])

dnl output directive
AC_OUTPUT(Makefile src/Makefile)
