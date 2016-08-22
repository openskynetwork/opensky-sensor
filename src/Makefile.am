
# binary programs
bin_PROGRAMS =
if RADARCAPE_STANDALONE
bin_PROGRAMS += openskyd-rc
endif
if RADARCAPE_NETWORK
bin_PROGRAMS += openskyd-rcd-net
endif
if RADARCAPE_DUMMY
bin_PROGRAMS += openskyd-rc-dummy
endif
if RADARCAPE_WATCHDOG
bin_PROGRAMS += rc-wd
endif

# common sources of opensky sensor
openskyd_sources = main.c \
	relay.h relay.c filter.h filter.c message.h message.c buffer.h buffer.c \
	network.h network.c endec.h \
	cfgfile.h cfgfile.c \
	net_common.h net_common.c util.h util.c \
	component.h component.c threads.h \
	log.h log_stdio.c \
	gps.h gps.c \
	watchdog.h statistics.h tb.h recv.h fpga.h

# common sources of radarcape input
radarcape_sources = $(openskyd_sources) \
	radarcape/rc-input.h radarcape/radarcape.c \
	gps/gps_recv.h gps/gps_recv.c gps/gps_parser.h gps/gps_parser.c \
	tb.c recv.c proc.h proc.c statistics.c

# sources for binaries
openskyd_rc_SOURCES = $(radarcape_sources) radarcape/uart.c \
	gps/gps_input_uart.c fpga.c watchdog.c gpio.h gpio.c
openskyd_rcd_net_SOURCES = $(radarcape_sources) radarcape/network.c \
	gps/gps_input_network.c
openskyd_rc_dummy_SOURCES = $(radarcape_sources) radarcape/dummy.c \
	gps/gps_input_dummy.c

rc_wd_SOURCES = wd-main.c \
	gpio.h gpio.c watchdog.h watchdog.c fpga.h fpga.c \
	cfgfile.h cfgfile.c util.h util.c log.h log_stdio.c

# compiler libs
openskyd_rc_LIBS = @PTHREAD_LIBS@
openskyd_rc_LDADD = -lm
openskyd_rcd_net_LIBS = @PTHREAD_LIBS@
openskyd_rcd_net_LDADD = -lm
openskyd_rc_dummy_LIBS = @PTHREAD_LIBS@
openskyd_rc_dummy_LDADD = -lm

# common compile flags
AM_CFLAGS = @PTHREAD_CFLAGS@

# compile flags for development
if !LOCAL_FILES
AM_CFLAGS += -DSYSCONFDIR='"$(sysconfdir)"' -DFWDIR='"$(libdir)"'
endif

# compile flags
openskyd_rc_CFLAGS = $(AM_CFLAGS) -DINPUT_RADARCAPE -DINPUT_RADARCAPE_UART \
	-DSTANDALONE
openskyd_rcd_net_CFLAGS = $(AM_CFLAGS) -DINPUT_RADARCAPE \
	-DINPUT_RADARCAPE_NETWORK
openskyd_rc_dummy_CFLAGS = $(AM_CFLAGS) -DINPUT_RADARCAPE \
	-DINPUT_RADARCAPE_DUMMY
rc_wd_CPPFLAGS = -DWD_ONLY $(AM_CPPFLAGS)

# data files
openskyd_rc_DATA = openskyd.conf
openskyd_rcd_net_DATA = openskyd.conf
openskyd_rc_dummy_DATA = openskyd.conf
EXTRA_DIST = openskyd.conf

# configuration directory
openskyd_rcdir = $(sysconfdir)
openskyd_rcd_netdir = $(sysconfdir)
openskyd_rc_dummydir = $(sysconfdir)

clean-local:
	-rm -f *.gcno *.gcda

SUBDIRS = . lib test
