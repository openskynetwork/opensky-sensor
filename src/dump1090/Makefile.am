
# binary programs
bin_PROGRAMS =
if DUMP1090
bin_PROGRAMS += openskyd-dump1090
endif

openskyd_dump1090_SOURCES = dump1090.c req-serial.h req-serial.c \
	../radarcape/radarcape.c ../radarcape/network.c

# compiler libs
openskyd_dump1090_LIBS = @PTHREAD_LIBS@
openskyd_dump1090_LDADD = ../core/libcore.a ../util/libutil.a

# common compile flags
AM_CFLAGS = @PTHREAD_CFLAGS@ -I$(top_srcdir)/src

# compiler flags
openskyd_rcd_net_CFLAGS = $(AM_CFLAGS) -DINPUT_RADARCAPE \
	-DINPUT_RADARCAPE_NETWORK

# compile flags for development
if !LOCAL_FILES
AM_CFLAGS += -DSYSCONFDIR='"$(sysconfdir)"'
endif

clean-local:
	-rm -f *.gcno *.gcda
