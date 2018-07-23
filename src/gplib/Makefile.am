
# libraries and binary programs
if GP_LIBRARY
lib_LTLIBRARIES=libopensky-c++.la
myincludedir=$(includedir)/opensky
myinclude_HEADERS=opensky.hh devicetypes.h
endif
if GP_LIBRARY_EXAMPLES
bin_PROGRAMS = opensky-lib-example
endif

libopensky_c___la_SOURCES=opensky.hh opensky.cc devicetypes.h \
	../core/filter.h ../core/filter.c \
	../core/buffer.h ../core/buffer.c \
	../core/relay.h ../core/relay.c \
	../core/network.h ../core/network.c \
	../core/gps.h ../core/gps.c \
	../core/tb.h ../core/tb.c \
	../core/login.h ../core/login.c \
	../core/beast.h ../core/beast.c \
	../util/net_common.h ../util/net_common-posix.c \
	../util/statistics.h ../util/statistics.c \
	../util/component.h ../util/component.c \
	../util/threads.h \
	../util/util.h ../util/util.c \
	../util/log.h log_lib.cc \
	../util/cfgfile.h ../util/cfgfile.c \
	../util/port/mmap.h ../util/port/mmap-posix.c \
	../util/serial_eth.h ../util/serial_eth.c

# version info: CURRENT (=iface version) : REVISION (internal changes only) : AGE (delta to last iface supported)
libopensky_c___la_LDFLAGS=-version-info 1:0:0
libopensky_c___la_CFLAGS=-fvisibility=hidden -DPREFIX=\"OpenSky\" -I$(top_srcdir)/src -DLIB
libopensky_c___la_CXXFLAGS=-fvisibility=hidden -DPREFIX=\"OpenSky\" -I$(top_srcdir)/src -DLIB

opensky_lib_example_SOURCES=opensky-example.cc
opensky_lib_example_AM_CXXFLAGS=@PTHREAD_CFLAGS@
opensky_lib_example_LDFLAGS=@PTHREAD_CFLAGS@ @PTHREAD_LIBS@
opensky_lib_example_LDADD=libopensky-c++.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libopensky-c++.pc
EXTRA_DIST = libopensky-c++.pc.in
