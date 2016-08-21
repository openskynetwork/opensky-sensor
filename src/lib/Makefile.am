
# binary programs and libraries
if LIBRARY_EXAMPLES
bin_PROGRAMS = opensky-lib-example opensky-lib-test
endif
if LIBRARY
lib_LTLIBRARIES=libopensky.la
endif

libopensky_la_SOURCES=opensky.hh opensky.cc MessageTypes.h GpsStatus.h \
	../filter.h ../filter.c ../buffer.h ../buffer.c ../relay.h ../relay.c \
	../network.h ../network.c ../net_common.h ../net_common.c \
	../statistics.h ../statistics.c ../component.h ../component.c \
	../threads.h ../util.h ../util.c ../log.h log_lib.cc ../gps.h \
	../tb.h ../tb.c ../gps.h ../gps.c

# version info: CURRENT (=iface version) : REVISION (internal changes only) : AGE (delta to last iface supported)
libopensky_la_LDFLAGS=-version-info 0:0:0
libopensky_la_CFLAGS=-fvisibility=hidden -DPREFIX=\"OpenSky\" -I$(srcdir)/.. -DLIB
libopensky_la_CXXFLAGS=-fvisibility=hidden -DPREFIX=\"OpenSky\" -I$(srcdir)/.. -DLIB
include_HEADERS=opensky.hh

opensky_lib_example_SOURCES=opensky-example.cc
opensky_lib_example_AM_CXXFLAGS=@PTHREAD_CFLAGS@
opensky_lib_example_LDFLAGS=@PTHREAD_CFLAGS@ @PTHREAD_LIBS@
opensky_lib_example_LDADD=libopensky.la

opensky_lib_test_SOURCES=opensky-test.cc \
	../adsb.h ../adsb.c ../input.h ../input_network.c ../statistics.h \
	../statistics.c ../cfgfile.h ../cfgfile.c ../log.h ../log_stdio.c ../util.h \
	../util.c ../net_common.c
opensky_lib_test_CXXFLAGS=@PTHREAD_CFLAGS@ -I$(srcdir)/..
opensky_lib_test_CFLAGS=@PTHREAD_CFLAGS@ -I$(srcdir)/..
opensky_lib_test_LDFLAGS=@PTHREAD_CFLAGS@ @PTHREAD_LIBS@
opensky_lib_test_LDADD=libopensky.la
