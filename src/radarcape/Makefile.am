
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

# common sources of radarcape input
radarcape_sources = main.c \
	watchdog.h fpga.h \
	rc-input.h radarcape.c \
	../gps/gps_input.h \
	../gps/gps_recv.h ../gps/gps_recv.c ../gps/gps_parser.h ../gps/gps_parser.c

# sources for binaries
openskyd_rc_SOURCES = $(radarcape_sources) uart.c \
	../gps/gps_input_uart.c fpga.c watchdog.c \
	gpio.h gpio.c tb_standalone.h tb_standalone.c
openskyd_rcd_net_SOURCES = $(radarcape_sources) network.c \
	../gps/gps_input_network.c
openskyd_rc_dummy_SOURCES = $(radarcape_sources) dummy.c \
	../gps/gps_input_dummy.c

rc_wd_SOURCES = wd-main.c \
	gpio.h gpio.c \
	watchdog.h watchdog.c

# compiler libs
openskyd_rc_LIBS = @PTHREAD_LIBS@
openskyd_rc_LDADD = ../core/libcore.a ../util/libutil.a
openskyd_rcd_net_LIBS = @PTHREAD_LIBS@
openskyd_rcd_net_LDADD = ../core/libcore.a ../util/libutil.a
openskyd_rc_dummy_LIBS = @PTHREAD_LIBS@
openskyd_rc_dummy_LDADD = -lm ../core/libcore.a ../util/libutil.a
rc_wd_LDADD = ../core/libcore.a ../util/libutil.a

# common compile flags
AM_CFLAGS = @PTHREAD_CFLAGS@ -I$(top_srcdir)/src

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
openskyd_rc_DATA = ../openskyd.conf
openskyd_rcd_net_DATA = ../openskyd.conf
openskyd_rc_dummy_DATA = ../openskyd.conf
EXTRA_DIST = ../openskyd.conf

# configuration directory
openskyd_rcdir = $(sysconfdir)
openskyd_rcd_netdir = $(sysconfdir)
openskyd_rc_dummydir = $(sysconfdir)

clean-local:
	-rm -f *.gcno *.gcda

SUBDIRS = . lib
