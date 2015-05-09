
# binary programs
bin_PROGRAMS =
if WANT_NETWORK
bin_PROGRAMS += openskyd-net
endif
if WANT_UART
bin_PROGRAMS += openskyd-uart
endif
if WANT_WD
bin_PROGRAMS += wd
endif

# common sources of openskyd
SOURCES = main.c gpio.h gpio.c watchdog.h watchdog.c fpga.h fpga.c \
	adsb.c adsb.h buffer.h buffer.c network.h network.c cfgfile.h cfgfile.c \
	statistics.h statistics.c tb.h tb.c \
	input.h net_common.h net_common.c util.h util.c

wd_SOURCES = gpio.h gpio.c watchdog.h watchdog.c wd-main.c

# sources for talkback
if TALKBACK
SOURCES += proc.h proc.c
endif

# extra sources
openskyd_net_SOURCES = $(SOURCES) input_network.c
openskyd_uart_SOURCES = $(SOURCES) input_uart.c

# libs and cflags
openskyd_net_LIBS = @PTHREAD_LIBS@
openskyd_uart_LIBS = @PTHREAD_LIBS@
AM_CFLAGS = @PTHREAD_CFLAGS@

# flags for development
if DEVELOPMENT
AM_CPPFLAGS = -DDEVELOPMENT
else
AM_CPPFLAGS = -DSYSCONFDIR='"$(sysconfdir)"' -DFWDIR='"$(libdir)"'
endif

# flags for talkback
if TALKBACK
AM_CPPFLAGS += -DTALKBACK
endif

# Network needs another flag. Do this also with openskyd-uart, so object files
#  will be named properly
openskyd_net_CPPFLAGS = $(AM_CPPFLAGS) -DNETWORK
openskyd_uart_CPPFLAGS = $(AM_CPPFLAGS)
wd_CPPFLAGS = -DWD_ONLY

# data files
openskyd_net_DATA = openskyd.conf
openskyd_uart_DATA = openskyd.conf
EXTRA_DIST = openskyd.conf

# configuration directory
openskyd_netdir = $(sysconfdir)
openskyd_uartdir = $(sysconfdir)
