
# binary programs
if WANT_EXAMPLES
bin_PROGRAMS = opensky-lib-example
endif

lib_LTLIBRARIES=libopensky.la

libopensky_la_SOURCES=opensky.hh opensky.cc MessageTypes.h \
	../filter.h ../filter.c ../buffer.h ../buffer.c ../relay.h ../relay.c \
	../network.h ../network.c ../net_common.h ../net_common.c \
	../statistics.h ../statistics.c ../component.h ../component.c \
	../threads.h ../util.h ../util.c ../log.h ../log_stdio.c

# version info: CURRENT (=iface version) : REVISION (internal changes only) : AGE (delta to last iface supported)
libopensky_la_LDFLAGS=-version-info 0:0:0
libopensky_la_CFLAGS=-fvisibility=hidden -DPREFIX=\"OpenSky\" -I$(srcdir)/..
libopensky_la_CXXFLAGS=-fvisibility=hidden -DPREFIX=\"OpenSky\" -I$(srcdir)/..
include_HEADERS=opensky.hh

opensky_lib_example_SOURCES=opensky-example.cc
opensky_lib_example_AM_CXXFLAGS=@PTHREAD_CFLAGS@
opensky_lib_example_LDFLAGS=@PTHREAD_CFLAGS@ @PTHREAD_LIBS@
opensky_lib_example_LDADD=libopensky.la
