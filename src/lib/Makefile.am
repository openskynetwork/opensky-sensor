
# libraries and binary programs
if RADARCAPE_LIBRARY
lib_LTLIBRARIES=libopensky.la
endif
if RADARCAPE_LIBRARY_EXAMPLES
bin_PROGRAMS = opensky-lib-example opensky-lib-test
endif

libopensky_la_SOURCES=opensky.hh opensky.cc MessageTypes.h GpsStatus.h \
	../filter.h ../filter.c ../buffer.h ../buffer.c ../relay.h ../relay.c \
	../network.h ../network.c ../net_common.h ../net_common.c \
	../statistics.h ../statistics.c ../component.h ../component.c \
	../threads.h ../util.h ../util.c ../log.h log_lib.cc ../gps.h \
	../tb.h ../tb.c ../gps.h ../gps.c ../cfgfile.h ../cfgfile.c

# version info: CURRENT (=iface version) : REVISION (internal changes only) : AGE (delta to last iface supported)
libopensky_la_LDFLAGS=-version-info 0:1:0
libopensky_la_CFLAGS=-fvisibility=hidden -DPREFIX=\"OpenSky\" -I$(srcdir)/.. -DLIB
libopensky_la_CXXFLAGS=-fvisibility=hidden -DPREFIX=\"OpenSky\" -I$(srcdir)/.. -DLIB
include_HEADERS=opensky.hh

opensky_lib_example_SOURCES=opensky-example.cc
opensky_lib_example_AM_CXXFLAGS=@PTHREAD_CFLAGS@
opensky_lib_example_LDFLAGS=@PTHREAD_CFLAGS@ @PTHREAD_LIBS@
opensky_lib_example_LDADD=libopensky.la

opensky_lib_test_SOURCES=opensky-test.cc \
	../input.h ../radarcape/radarcape.c ../radarcape/network.c \
	../statistics.h ../statistics.c ../cfgfile.h ../cfgfile.c \
	../log.h ../log_stdio.c ../util.h ../util.c ../net_common.c \
	../filter.h ../filter.c ../component.h ../component.c
opensky_lib_test_CXXFLAGS=@PTHREAD_CFLAGS@ -I$(srcdir)/..
opensky_lib_test_CFLAGS=@PTHREAD_CFLAGS@ -I$(srcdir)/..
opensky_lib_test_LDFLAGS=@PTHREAD_CFLAGS@ @PTHREAD_LIBS@
opensky_lib_test_LDADD=libopensky.la
