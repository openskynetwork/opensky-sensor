dnl initialize autoconf
AC_INIT(openskyd, 2.1.3)

dnl put autotools files into config subdirectory
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_SRCDIR(src/core/buffer.c)
AC_REQUIRE_AUX_FILE([tap-driver.sh])

dnl initialize automake
AM_INIT_AUTOMAKE([gnu subdir-objects -Wall -Werror])
AC_CONFIG_HEADERS(config.h)
AM_MAINTAINER_MODE

CFLAGS="$CFLAGS -std=gnu11"
CXXFLAGS="$CXXFLAGS -std=c++0x"

dnl required programs
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AM_PROG_AR
AC_PROG_LIBTOOL

case "$host" in
  *cygwin* | *mingw* )
    AC_CHECK_TOOL([WINDRES], [windres], [:])
    HOST_IS_MINGW=1
    ac_cv_func_realloc_0_nonnull=yes
    ac_cv_func_malloc_0_nonnull=yes
  ;;
  *)
    WINDRES=""
    HOST_IS_MINGW=0
  ;;
esac

case "$host" in
  *i?86* )
    ac_cv_func_realloc_0_nonnull=yes
    ac_cv_func_malloc_0_nonnull=yes
  ;;
esac

dnl test for std headers
AC_HEADER_STDC

AC_SEARCH_LIBS([clock_gettime],[rt posix4])

AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h limits.h netdb.h netinet/in.h])
AC_CHECK_HEADERS([stddef.h stdint.h stdlib.h string.h sys/ioctl.h])
AC_CHECK_HEADERS([sys/socket.h], [SOCKET_POSIX=1], [SOCKET_POSIX=0])
AC_CHECK_HEADERS([winsock2.h], [SOCKET_WINSOCK=1], [SOCKET_WINSOCK=0])
AC_CHECK_HEADERS([sys/mman.h], [MMAP_POSIX=1], [MMAP_POSIX=0])
AC_CHECK_HEADERS([termios.h unistd.h sys/time.h endian.h features.h direct.h])

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
AC_TYPE_UID_T
AC_CHECK_TYPES([ptrdiff_t])

AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_STRERROR_R
AC_FUNC_STRTOD
AC_CHECK_FUNCS([clock_gettime gettimeofday mkdir strcasecmp])
AC_CHECK_FUNCS([memchr memmove memset munmap pow setenv socket])
AC_CHECK_FUNCS([strerror strncasecmp strtoul])
AC_CHECK_FUNCS([setlinebuf pause sigwait])
AC_CHECK_FUNCS([getaddrinfo setuid scandir])
AC_CHECK_FUNCS([pthread_timedjoin_np pthread_setname_np])
AC_CHECK_FUNCS([wait], [WAIT_POSIX=1], [WAIT_POSIX=0])

AX_PTHREAD

AC_ARG_WITH([check],
  [AS_HELP_STRING([--with-check],
    [Add test cases using libcheck @<:@check@:>@])],
  [:],[with_check=check])
AS_IF([test x"$with_check" != x"no"],
  [PKG_CHECK_MODULES([CHECK], [check >= 0.9.6],
    [with_check=yes],
    [AS_IF([test x"$with_check" = x"yes"],
      [AC_MSG_ERROR([check required, but not found.])],
      [with_check=no])])])
AM_CONDITIONAL([WITH_CHECK], [test x$with_check = xyes])

AC_ARG_ENABLE([test-coverage],
  [AS_HELP_STRING([--enable-test-coverage], [Enable Test Coverage @<:@no@:>@])],
  [case "${enableval}" in
    yes|no) ;;
    *)  AC_MSG_ERROR([bad value ${enableval} for --enable-test-coverage]) ;;
   esac], [enableval=no])
AM_CONDITIONAL([TEST_COVERAGE], [test x$enableval = xyes])

AC_ARG_ENABLE([cleanup-routines],
  [AS_HELP_STRING([--enable-cleanup-routines],
    [Add cleanup routines (disable for valgrind maybe) @<:@yes@:>@])],
  [case "${enableval}" in
    yes|no) ;;
    *)  AC_MSG_ERROR([bad value ${enableval} for --enable-cleanup]) ;;
   esac], [enableval=yes])
AS_IF([test x$enableval = xyes],
  [AC_DEFINE([CLEANUP_ROUTINES], [1], [Add cleanup routines])])

AC_ARG_ENABLE([local-files],
  [AS_HELP_STRING([--enable-local-files], [Use local files @<:@no@:>@])],
  [case "${enableval}" in
    yes|no) ;;
    *)  AC_MSG_ERROR([bad value ${enableval} for --enable-local-files]) ;;
   esac], [enableval=no])
AM_CONDITIONAL([LOCAL_FILES], [test x$enableval = xyes])
AS_IF([test x$enableval = xyes],
  [AC_DEFINE([LOCAL_FILES], [1], [Use local files for development])])

AC_ARG_ENABLE([radarcape-standalone],
  [AS_HELP_STRING([--enable-radarcape-standalone],
    [Build daemon for standalone radarcape @<:@no@:>@])],
  [case "${enableval}" in
    yes|no) ;;
    *)  AC_MSG_ERROR([bad value ${enableval} for --enable-radarcape-standalone]) ;;
   esac], [enableval=no])
AM_CONDITIONAL([RADARCAPE_STANDALONE], [test x$enableval = xyes])
AS_IF([test x$enableval = xyes -a $HOST_IS_MINGW = 1],
	[AC_MSG_ERROR([Cannot build radarcape binaries with mingw])])

AC_ARG_ENABLE([radarcape-network],
  [AS_HELP_STRING([--enable-radarcape-network],
    [Build daemon to connect to rcd @<:@no@:>@])],
  [case "${enableval}" in
    yes|no) ;;
    *)  AC_MSG_ERROR([bad value ${enableval} for --enable-radarcape-network]) ;;
   esac], [enableval=no])
AM_CONDITIONAL([RADARCAPE_NETWORK], [test x$enableval = xyes])
AS_IF([test x$enableval = xyes -a $HOST_IS_MINGW = 1],
	[AC_MSG_ERROR([Cannot build radarcape binaries with mingw])])

AC_ARG_ENABLE([radarcape-dummy],
  [AS_HELP_STRING([--enable-radarcape-dummy],
    [Build daemon with radarcape dummy input layer @<:@no@:>@])],
  [case "${enableval}" in
    yes|no) ;;
    *)  AC_MSG_ERROR([bad value ${enableval} for --enable-radarcape-dummy]) ;;
   esac], [enableval=no])
AM_CONDITIONAL([RADARCAPE_DUMMY], [test x$enableval = xyes])
AS_IF([test x$enableval = xyes -a $HOST_IS_MINGW = 1],
	[AC_MSG_ERROR([Cannot build radarcape binaries with mingw])])

AC_ARG_ENABLE([watchdog-daemon],
  [AS_HELP_STRING([--enable-watchdog-daemon],
    [Build Watchdog daemon for standalone radarcape @<:@no@:>@])],
  [case "${enableval}" in
    yes|no) ;;
    *)  AC_MSG_ERROR([bad value ${enableval} for --enable-watchdog-daemon]) ;;
   esac], [enableval=no])
AM_CONDITIONAL([RADARCAPE_WATCHDOG], [test x$enableval = xyes])
AS_IF([test x$enableval = xyes -a $HOST_IS_MINGW = 1],
	[AC_MSG_ERROR([Cannot build radarcape binaries with mingw])])

AC_ARG_ENABLE([radarcape-library],
  [AS_HELP_STRING([--enable-radarcape-library],
  [Build library for rcd integration @<:@no@:>@])],
  [case "${enableval}" in
    yes|no) ;;
    *)  AC_MSG_ERROR([bad value ${enableval} for --enable-library]) ;;
   esac], [enableval=no])
AM_CONDITIONAL([RADARCAPE_LIBRARY], [test x$enableval = xyes])
AS_IF([test x$enableval = xyes -a $HOST_IS_MINGW = 1],
	[AC_MSG_ERROR([Cannot build radarcape binaries with mingw])])

AC_ARG_ENABLE([radarcape-library-examples],
  [AS_HELP_STRING([--enable-radarcape-library-examples],
    [Build rcd integration library examples @<:@no@:>@])],
  [case "${enableval}" in
    yes|no) ;;
    *)  AC_MSG_ERROR([bad value ${enableval} for --enable-radarcape-library-examples]) ;;
   esac], [enableval=false])
AM_CONDITIONAL([RADARCAPE_LIBRARY_EXAMPLES], [test x$enableval = xyes])
AS_IF([test x$enableval = xyes -a $HOST_IS_MINGW = 1],
	[AC_MSG_ERROR([Cannot build radarcape binaries with mingw])])

AC_ARG_ENABLE([dump1090],
  [AS_HELP_STRING([--enable-dump1090],
    [Build daemon to connect to dump1090 feeder @<:@no@:>@])],
  [case "${enableval}" in
    yes|no) ;;
    *)  AC_MSG_ERROR([bad value ${enableval} for --enable-dump1090]) ;;
   esac], [enableval=no])
AM_CONDITIONAL([DUMP1090], [test x$enableval = xyes])

AC_ARG_ENABLE([dump1090-rshell],
  [AS_HELP_STRING([--enable-dump1090-rshell],
    [Build daemon to connect to dump1090 feeder with reverse shell support @<:@no@:>@])],
  [case "${enableval}" in
    yes|no) ;;
    *)  AC_MSG_ERROR([bad value ${enableval} for --enable-dump1090-rshell]) ;;
   esac], [enableval=no])
AM_CONDITIONAL([DUMP1090_RSHELL], [test x$enableval = xyes])

AM_CONDITIONAL([SOCKET_POSIX], [test $SOCKET_POSIX = 1])
AM_CONDITIONAL([SOCKET_WINSOCK], [test $SOCKET_WINSOCK = 1])
AM_CONDITIONAL([MMAP_POSIX], [test $MMAP_POSIX = 1])
AM_CONDITIONAL([WAIT_POSIX], [test $WAIT_POSIX = 1])

AC_ARG_VAR([ETHER_DEV], [Set the name of the ethernet device @<:@eth0@:>@])
AS_IF([test x${ETHER_DEV} != x],
  [AC_DEFINE_UNQUOTED([ETHER_DEV], ["${ETHER_DEV}"],
    [Override ethernet device])])
  
AC_ARG_WITH([systemd],
  [AS_HELP_STRING([--with-systemd],
    [Compile with systemd integration @<:@check@:>@])],
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
   AC_DEFINE_UNQUOTED([WITH_SYSTEMD], ["$systemd_PREFIX"],
     [Enable systemd integration])])

AC_ARG_WITH([pacman],
  [AS_HELP_STRING([--with-pacman],
    [Compile with pacman integration @<:@check@:>@])],
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

AC_ARG_WITH([pthreadCancellationPoints],
  [AS_HELP_STRING([--with-pthreadCancellationPoints],
    [Assume pthread has working cancellation points @<:@guess@:>@])],
  [:],[with_pthreadCancellationPoints=guess])
AS_IF([test x"$with_pthreadCancellationPoints" = x"guess"],
  [AS_IF([test $HOST_IS_MINGW = 0], [with_pthreadCancellationPoints=yes],
    [with_pthreadCancellationPoints=no])])
AS_IF([test x"$with_pthreadCancellationPoints" != x"no"],
  [AC_DEFINE([PTHREAD_CANCELLATION_POINTS], [1],
  [Assume pthread has working cancellation points])])

dnl output directive
AC_OUTPUT([Makefile src/Makefile src/core/Makefile src/util/Makefile
	src/trimble/Makefile src/radarcape/Makefile src/radarcape/lib/Makefile
	src/dump1090/Makefile src/test/Makefile])
