
# binary programs
bin_PROGRAMS=openskyd
# sources of openskyd
openskyd_SOURCES=main.c gpio.h gpio.c watchdog.h watchdog.c fpga.h fpga.c \
	adsb.c adsb.h buffer.h buffer.c network.h network.c cfgfile.h cfgfile.c \
	statistics.h statistics.c tb.h tb.c proc.h proc.c \
	input.h net_common.h net_common.c

openskyd_LIBS="$(PTHREAD_LIBS)"
openskyd_CFLAGS="$(PTHREAD_CFLAGS)"

if NETWORKING
openskyd_SOURCES += input_network.c
openskyd_CFLAGS += -DNETWORKING
else
openskyd_SOURCES += input_uart.c
endif

if DEVELOPMENT
openskyd_CPPFLAGS=-DDEVELOPMENT
else
openskyd_CPPFLAGS=-DSYSCONFDIR='"$(sysconfdir)"'
openskyd_CPPFLAGS+=-DFWDIR='"$(libdir)"'
endif

openskyd_DATA=openskyd.conf
openskyddir=$(sysconfdir)

EXTRA_DIST=openskyd.conf
