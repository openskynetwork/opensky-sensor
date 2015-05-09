dnl initialize autoconf
AC_INIT(openskyd, 0.7.1)

dnl put autotools files into config subdirectory
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_SRCDIR(src/main.c)

dnl initialize automake
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS(config.h)
AM_MAINTAINER_MODE

dnl set some global variables here
VERSION=0.7.1
AC_SUBST(VERSION) dnl this exports version as a AC macro

dnl required programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

dnl test for std headers
AC_HEADER_STDC

AC_SEARCH_LIBS([clock_gettime],[rt posix4])

AX_PTHREAD

AC_ARG_ENABLE([development],
  [AS_HELP_STRING([--enable-development], [Use local files (default is no)])],
  [case "${enableval}" in
    yes) development=true ;;
    no)  development=false ;;
     *)  AC_MSG_ERROR([bad value ${enableval} for --enable-development]) ;;
   esac], [development=false])
AM_CONDITIONAL([DEVELOPMENT], [test x$development = xtrue])

AC_ARG_ENABLE([network],
  [AS_HELP_STRING([--enable-network], [Build network input layer (default is no)])],
  [case "${enableval}" in
    yes) network=true ;;
    no)  network=false ;;
     *)  AC_MSG_ERROR([bad value ${enableval} for --enable-network]) ;;
   esac], [network=false])
AM_CONDITIONAL([WANT_NETWORK], [test x$network = xtrue])

AC_ARG_ENABLE([uart],
  [AS_HELP_STRING([--enable-uart], [Build uart input layer (default is yes)])],
  [case "${enableval}" in
    yes) uart=true ;;
    no)  uart=false ;;
     *)  AC_MSG_ERROR([bad value ${enableval} for --enable-uart]) ;;
   esac], [uart=true])
AM_CONDITIONAL([WANT_UART], [test x$uart = xtrue])

AC_ARG_ENABLE([wd],
  [AS_HELP_STRING([--enable-wd], [Build Watchdog Daemon (default is no)])],
  [case "${enableval}" in
    yes) wd=true ;;
    no)  wd=false ;;
     *)  AC_MSG_ERROR([bad value ${enableval} for --enable-wd]) ;;
   esac], [wd=false])
AM_CONDITIONAL([WANT_WD], [test x$wd = xtrue])

AC_ARG_ENABLE([invasive-talkback],
  [AS_HELP_STRING([--enable-invasive-talkback], [Enable invasive talkback (default is yes)])],
  [case "${enableval}" in
    yes) talkback=true ;;
    no)  talkback=false ;;
     *)  AC_MSG_ERROR([bad value ${enableval} for --enable-talkback]) ;;
   esac], [talkback=true])
AM_CONDITIONAL([TALKBACK], [test x$talkback = xtrue])

if test x$network = xfalse -a x$uart = xfalse
then
	AC_MSG_ERROR([No input layer chosen])
fi


dnl output directive
AC_OUTPUT(Makefile src/Makefile)
