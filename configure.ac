dnl initialize autoconf
AC_INIT(openskyd, 0.5.2)

dnl put autotools files into config subdirectory
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_SRCDIR(src/main.c)

dnl initialize automake
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS(config.h)
AM_MAINTAINER_MODE

dnl set some global variables here
VERSION=0.5.2
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
  [AS_HELP_STRING([--enable-network], [Use network input layer (default is no)])],
  [case "${enableval}" in
    yes) networking=true ;;
    no)  networking=false ;;
     *)  AC_MSG_ERROR([bad value ${enableval} for --enable-development]) ;;
   esac], [development=false])
AM_CONDITIONAL([NETWORKING], [test x$networking = xtrue])

dnl output directive
AC_OUTPUT(Makefile src/Makefile)
