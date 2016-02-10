
# binary programs
bin_PROGRAMS =
if WANT_NETWORK
bin_PROGRAMS += openskyd-net
endif
if WANT_UART
bin_PROGRAMS += openskyd-uart
endif
if WANT_DUMMY
bin_PROGRAMS += openskyd-dummy
endif
if WANT_WD
bin_PROGRAMS += wd
endif
if WANT_EXAMPLES
bin_PROGRAMS += opensky-lib-example
endif

lib_LTLIBRARIES=libopensky.la

libopensky_la_SOURCES=buffer.c buffer.h \
	relay.h relay.c network.h network.c net_common.h net_common.c \
	statistics.h statistics.c threads.h component.h component.c util.h util.c \
	opensky.h opensky.c filter.h filter.c
# version info: CURRENT (=iface version) : REVISION (internal changes only) : AGE (delta to last iface supported)
libopensky_la_LDFLAGS=-version-info 0:0:0
libopensky_la_CFLAGS=-fvisibility=hidden -DPREFIX=\"OpenSky\"
include_HEADERS=opensky.h

opensky_lib_example_SOURCES=opensky-example.c
opensky_lib_example_AM_CFLAGS=@PTHREAD_CFLAGS@
opensky_lib_example_LDFLAGS=@PTHREAD_CFLAGS@ @PTHREAD_LIBS@
opensky_lib_example_LDADD=libopensky.la

# common sources of openskyd
SOURCES = main.c gpio.h gpio.c watchdog.h watchdog.c fpga.h fpga.c \
	adsb.c adsb.h buffer.h buffer.c network.h network.c cfgfile.h cfgfile.c \
	statistics.h statistics.c tb.h tb.c recv.h recv.c \
	input.h net_common.h net_common.c util.h util.c \
	component.h component.c threads.h relay.h relay.c message.h message.c \
	gps.h gps.c gps_input.h gps_input_uart.c gps_parser.h gps_parser.c \
	filter.h filter.c
	
wd_SOURCES = gpio.h gpio.c watchdog.h watchdog.c fpga.h fpga.c \
	cfgfile.h cfgfile.c util.h util.c wd-main.c

# sources for talkback
if TALKBACK
SOURCES += proc.h proc.c
endif

# extra sources
openskyd_net_SOURCES = $(SOURCES) input_network.c
openskyd_uart_SOURCES = $(SOURCES) input_uart.c
openskyd_dummy_SOURCES = $(SOURCES) input_dummy.c

# libs and cflags
openskyd_net_LIBS = @PTHREAD_LIBS@
openskyd_uart_LIBS = @PTHREAD_LIBS@
openskyd_dummy_LIBS = @PTHREAD_LIBS@
openskyd_dummy_LDADD = -lm
AM_CFLAGS = @PTHREAD_CFLAGS@

# flags for development
if !DEVELOPMENT
AM_CPPFLAGS = -DSYSCONFDIR='"$(sysconfdir)"' -DFWDIR='"$(libdir)"'
endif

# Network needs another flag. Do this also with openskyd-uart, so object files
#  will be named properly
openskyd_net_CPPFLAGS = $(AM_CPPFLAGS) -DNETWORK
openskyd_uart_CPPFLAGS = $(AM_CPPFLAGS)
openskyd_dummy_CPPFLAGS = $(AM_CPPFLAGS)
wd_CPPFLAGS = -DWD_ONLY

# data files
openskyd_net_DATA = openskyd.conf
openskyd_uart_DATA = openskyd.conf
openskyd_dummy_DATA = openskyd.conf
EXTRA_DIST = openskyd.conf

# configuration directory
openskyd_netdir = $(sysconfdir)
openskyd_uartdir = $(sysconfdir)
openskyd_dummydir = $(sysconfdir)

clean-local:
	-rm -f *.gcno *.gcda

SUBDIRS = . test
