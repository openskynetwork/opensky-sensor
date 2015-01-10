
# binary programs
bin_PROGRAMS=cape
# sources of that program
cape_SOURCES=main.c gpio.h gpio.c watchdog.h watchdog.c fpga.h fpga.c \
	adsb.c adsb.h buffer.h buffer.c network.h network.c cfgfile.h cfgfile.c
cape_LIBS="$(PTHREAD_LIBS)"
cape_CFLAGS="$(PTHREAD_CFLAGS)"
EXTRA_DIST=cape.cfg

