
# libraries and binary programs
if RADARCAPE_LIBRARY
lib_LTLIBRARIES=libopensky.la
include_HEADERS=opensky.hh
endif
if RADARCAPE_LIBRARY_EXAMPLES
bin_PROGRAMS = opensky-lib-example opensky-lib-test
endif

libopensky_la_SOURCES=opensky.hh opensky.cc MessageTypes.h GpsStatus.h \
	../../core/filter.h ../../core/filter.c \
	../../core/buffer.h ../../core/buffer.c \
	../../core/relay.h ../../core/relay.c \
	../../core/network.h ../../core/network.c \
	../../core/gps.h ../../core/gps.c \
	../../core/tb.h ../../core/tb.c \
	../../core/login.h ../../core/login.c \
	../../core/beast.h ../../core/beast.c \
	../../util/net_common.h ../../util/net_common.c \
	../../util/statistics.h ../../util/statistics.c \
	../../util/component.h ../../util/component.c \
	../../util/threads.h \
	../../util/util.h ../../util/util.c \
	../../util/log.h log_lib.cc \
	../../util/cfgfile.h ../../util/cfgfile.c \
	../../util/serial_eth.h ../../util/serial_eth.c

# version info: CURRENT (=iface version) : REVISION (internal changes only) : AGE (delta to last iface supported)
libopensky_la_LDFLAGS=-version-info 0:2:0
libopensky_la_CFLAGS=-fvisibility=hidden -DPREFIX=\"OpenSky\" -I$(top_srcdir)/src -DLIB
libopensky_la_CXXFLAGS=-fvisibility=hidden -DPREFIX=\"OpenSky\" -I$(top_srcdir)/src -DLIB

opensky_lib_example_SOURCES=opensky-example.cc
opensky_lib_example_AM_CXXFLAGS=@PTHREAD_CFLAGS@
opensky_lib_example_LDFLAGS=@PTHREAD_CFLAGS@ @PTHREAD_LIBS@
opensky_lib_example_LDADD=libopensky.la

opensky_lib_test_SOURCES=opensky-test.cc ../rc-parser.c ../network.c
opensky_lib_test_CXXFLAGS=@PTHREAD_CFLAGS@ -I$(top_srcdir)/src
opensky_lib_test_CFLAGS=@PTHREAD_CFLAGS@ -I$(top_srcdir)/src
opensky_lib_test_LDFLAGS=@PTHREAD_CFLAGS@ @PTHREAD_LIBS@
opensky_lib_test_LDADD=libopensky.la ../../core/libcore.a ../../util/libutil.a
