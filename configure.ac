dnl initialize autoconf
AC_INIT(serocape, 0.0.9)

dnl put autotools files into config subdirectory
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_SRCDIR(src/cape.c)

dnl initialize automake
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS(config.h)

dnl set some global variables here
VERSION=0.0.9
AC_SUBST(VERSION) dnl this exports version as a AC macro

dnl required programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

dnl test for std headers
AC_HEADER_STDC

AC_SEARCH_LIBS([clock_gettime],[rt posix4])

AX_PTHREAD

dnl output directive
AC_OUTPUT(Makefile src/Makefile)
