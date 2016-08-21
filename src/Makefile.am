
# binary programs
bin_PROGRAMS =
if RADARCAPE_UART
bin_PROGRAMS += openskyd-rc-uart
endif
if RADARCAPE_NETWORK
bin_PROGRAMS += openskyd-rc-net
endif
if RADARCAPE_DUMMY
bin_PROGRAMS += openskyd-rc-dummy
endif
if WATCHDOG_DAEMON
bin_PROGRAMS += wd
endif

# common sources of opensky sensor
openskyd_sources = main.c \
	relay.h relay.c filter.h filter.c message.h message.c buffer.h buffer.c \
	network.h network.c \
	cfgfile.h cfgfile.c \
	net_common.h net_common.c util.h util.c \
	component.h component.c threads.h \
	log.h log_stdio.c \
	gps.h gps.c \
	statistics.h tb.h recv.h fpga.h

# common sources of radarcape input
radarcape_sources = $(openskyd_sources) \
	radarcape.h radarcape.c rc-input.h

# feature sources
feature_sources =
if TALKBACK_COMP
feature_sources += tb.c recv.c proc.h proc.c
endif
if FPGA_COMP
feature_sources += fpga.c gpio.h gpio.c
endif
if STATISTICS_COMP
feature_sources += statistics.c
endif

EXTRA_sources = tb.c recv.c proc.h proc.c \
	fpga.c gpio.h gpio.c \
	statistics.c

# sources for binaries
openskyd_rc_uart_SOURCES = $(radarcape_sources) rc-input-uart.c
openskyd_rc_net_SOURCES = $(radarcape_sources) rc-input-network.c
openskyd_rc_dummy_SOURCES = $(radarcape_sources) rc-input-dummy.c

wd_SOURCES = wd-main.c \
	gpio.h gpio.c watchdog.h watchdog.c fpga.h fpga.c \
	cfgfile.h cfgfile.c util.h util.c log.h log_stdio.c

# compiler libs
openskyd_rc_uart_LIBS = @PTHREAD_LIBS@
openskyd_rc_net_LIBS = @PTHREAD_LIBS@
openskyd_rc_dummy_LIBSS = @PTHREAD_LIBS@
openskyd_rc_uart_LDADD = -lm
openskyd_rc_net_LDADD = -lm
openskyd_rc_dummy_LDADD = -lm

# common compile flags
AM_CFLAGS = @PTHREAD_CFLAGS@

# compile flags for development
if !LOCAL_FILES
AM_CPPFLAGS = -DSYSCONFDIR='"$(sysconfdir)"' -DFWDIR='"$(libdir)"'
endif

# compile flags
openskyd_rc_uart_AM_CFLAGS = $(AM_CPPFLAGS) -DINPUT_RADARCAPE \
	-DINPUT_RADARCAPE_UART
openskyd_rc_net_AM_CFLAGS = $(AM_CPPFLAGS) -DINPUT_RADARCAPE \
	-DINPUT_RADARCAPE_NETWORK
openskyd_rc_dummy_AM_CFLAGS = $(AM_CPPFLAGS) -DINPUT_RADARCAPE \
	-DINPUT_RADARCAPE_DUMMY
wd_CPPFLAGS = -DWD_ONLY $(AM_CPPFLAGS)

# data files
openskyd_rc_uart_DATA = openskyd.conf
openskyd_rc_net_DATA = openskyd.conf
openskyd_rc_dummy_DATA = openskyd.conf
EXTRA_DIST = openskyd.conf

# configuration directory
openskyd_rc_uartdir = $(sysconfdir)
openskyd_rc_netdir = $(sysconfdir)
openskyd_rc_dummydir = $(sysconfdir)

clean-local:
	-rm -f *.gcno *.gcda

SUBDIRS = . lib test
