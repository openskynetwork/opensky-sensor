
# binary programs
bin_PROGRAMS =
if WANT_DAEMON
bin_PROGRAMS += openskyd
endif
if WANT_WD
bin_PROGRAMS += wd
endif

# common sources of openskyd
openskyd_SOURCES = main.c \
	adsb.c adsb.h buffer.h buffer.c network.h network.c cfgfile.h cfgfile.c \
	statistics.h statistics.c tb.h tb.c recv.h recv.c \
	input.h net_common.h net_common.c util.h util.c \
	component.h component.c threads.h relay.h relay.c message.h message.c \
	filter.h filter.c log.h log_stdio.c

# for now, disable gps
#	gps.h gps.c gps_input.h gps_input_uart.c gps_parser.h gps_parser.c

# input layer
openskyd_SOURCES += input_@INPUT_LAYER@.c

EXTRA_DIST = input_uart.c input_network.c input_dummy.c

# watchdog sources
wd_SOURCES = gpio.h gpio.c watchdog.h watchdog.c fpga.h fpga.c \
	cfgfile.h cfgfile.c util.h util.c wd-main.c log.h log_stdio.c

# sources for standalone
if STANDALONE
openskyd_SOURCES += gpio.h gpio.c watchdog.h watchdog.c fpga.h fpga.c
endif

# sources for talkback
if TALKBACK
openskyd_SOURCES += proc.h proc.c
endif

# libs and cflags
openskyd_LIBS = @PTHREAD_LIBS@
openskyd_LDADD = -lm
AM_CFLAGS = @PTHREAD_CFLAGS@

# flags for development
if !DEVELOPMENT
AM_CPPFLAGS = -DSYSCONFDIR='"$(sysconfdir)"' -DFWDIR='"$(libdir)"'
endif

# WD needs other flags
wd_CPPFLAGS = -DWD_ONLY $(AM_CPPFLAGS)

# data files
openskyd_DATA = openskyd.conf
EXTRA_DIST += openskyd.conf

# configuration directory
openskyddir = $(sysconfdir)

clean-local:
	-rm -f *.gcno *.gcda

SUBDIRS = . lib test
