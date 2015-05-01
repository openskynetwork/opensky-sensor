
# binary programs
bin_PROGRAMS=openskyd

if WANT_NETWORK
bin_PROGRAMS+=openskyd-net
main=openskyd-net$(EXEEXT)
endif

if WANT_UART
bin_PROGRAMS+=openskyd-uart
main=openskyd-uart$(EXEEXT)
endif

openskyd$(EXEEXT): $(main)
	if test -e "$@"; then rm -f "$@"; fi
	ln -s "$<" "$@"

# sources of openskyd
SOURCES=main.c gpio.h gpio.c watchdog.h watchdog.c fpga.h fpga.c \
	adsb.c adsb.h buffer.h buffer.c network.h network.c cfgfile.h cfgfile.c \
	statistics.h statistics.c tb.h tb.c proc.h proc.c \
	input.h net_common.h net_common.c


openskyd_net_SOURCES = $(SOURCES) input_network.c
openskyd_uart_SOURCES = $(SOURCES) input_uart.c
openskyd_SOURCES=

LIBS+=@PTHREAD_LIBS@
AM_CFLAGS=@PTHREAD_CFLAGS@

if DEVELOPMENT
AM_CPPFLAGS=-DDEVELOPMENT
else
AM_CPPFLAGS=-DSYSCONFDIR='"$(sysconfdir)"' -DFWDIR='"$(libdir)"'
endif

openskyd_net_CPPFLAGS = $(AM_CPPFLAGS) -DNETWORK
openskyd_uart_CPPFLAGS = $(AM_CPPFLAGS)

openskyd_net_DATA=openskyd.conf
openskyd_uart_DATA=openskyd.conf

openskyd_netdir=$(sysconfdir)
openskyd_uartdir=$(sysconfdir)

EXTRA_DIST=openskyd.conf
