
# binary programs
bin_PROGRAMS=openskyd
# sources of openskyd
openskyd_SOURCES=main.c gpio.h gpio.c watchdog.h watchdog.c fpga.h fpga.c \
	adsb.c adsb.h buffer.h buffer.c network.h network.c cfgfile.h cfgfile.c \
	statistics.h statistics.c
openskyd_LIBS="$(PTHREAD_LIBS)"
openskyd_CFLAGS="$(PTHREAD_CFLAGS)"
openskyd_CPPFLAGS=-DSYSCONFDIR='"$(sysconfdir)"'
openskyd_CPPFLAGS+=-DFWDIR='"$(libdir)"'
openskyd_DATA=openskyd.conf
openskyddir=$(sysconfdir)

EXTRA_DIST=openskyd.conf
