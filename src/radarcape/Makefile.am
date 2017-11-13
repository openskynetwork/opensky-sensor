
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
	rc-input.h rc-parser.h rc-parser.c

# sources for binaries
openskyd_rc_SOURCES = $(radarcape_sources) uart.c \
	fpga.c watchdog.c gpio.h gpio.c tb-rshell.h tb-rshell.c tb-misc.h tb-misc.c
openskyd_rcd_net_SOURCES = $(radarcape_sources) network.c
openskyd_rc_dummy_SOURCES = $(radarcape_sources) dummy.c

rc_wd_SOURCES = wd-main.c \
	gpio.h gpio.c \
	watchdog.h watchdog.c

# compiler libs
openskyd_rc_LIBS = @PTHREAD_LIBS@
openskyd_rc_LDADD = ../core/libcore.a ../trimble/libtrimble-uart.a \
	../util/libutil.a
openskyd_rcd_net_LIBS = @PTHREAD_LIBS@
openskyd_rcd_net_LDADD = ../core/libcore.a ../trimble/libtrimble-net.a \
	../util/libutil.a
openskyd_rc_dummy_LIBS = @PTHREAD_LIBS@
openskyd_rc_dummy_LDADD = -lm ../core/libcore.a ../trimble/libtrimble-dummy.a \
	../util/libutil.a
rc_wd_LDADD = ../core/libcore.a ../util/libutil.a

# common compile flags
AM_CFLAGS = @PTHREAD_CFLAGS@ -I$(top_srcdir)/src

# compile flags for development
if !LOCAL_FILES
AM_CFLAGS += -DSYSCONFDIR='"$(sysconfdir)"' -DFWDIR='"$(libdir)"'
endif

# compile flags
openskyd_rc_CFLAGS = $(AM_CFLAGS) -DINPUT_RADARCAPE_UART \
	-DSTANDALONE
openskyd_rcd_net_CFLAGS = $(AM_CFLAGS) -DINPUT_RADARCAPE_NETWORK
openskyd_rc_dummy_CFLAGS = $(AM_CFLAGS) -DINPUT_RADARCAPE_DUMMY
rc_wd_CPPFLAGS = -DWD_ONLY $(AM_CPPFLAGS)

# data files
if RADARCAPE_STANDALONE
openskyd_rc_DATA = openskyd.conf
endif
if RADARCAPE_NETWORK
openskyd_rcd_net_DATA = openskyd.conf
endif
if RADARCAPE_WATCHDOG
openskyd_rc_dummy_DATA = openskyd.conf
endif
EXTRA_DIST = openskyd.conf

# configuration directory
openskyd_rcdir = $(sysconfdir)
openskyd_rcd_netdir = $(sysconfdir)
openskyd_rc_dummydir = $(sysconfdir)

clean-local:
	-rm -f *.gcno *.gcda

SUBDIRS = . lib
