
# binary programs
bin_PROGRAMS =
if DUMP1090
bin_PROGRAMS += openskyd-dump1090
endif
if DUMP1090_RSHELL
bin_PROGRAMS += openskyd-dump1090-rshell
endif

openskyd_dump1090_SOURCES = dump1090.c req-serial.h req-serial.c \
	position.h position.c \
	../radarcape/rc-parser.c ../radarcape/network.c
	
openskyd_dump1090_rshell_SOURCES = $(openskyd_dump1090_SOURCES) \
	../radarcape/tb-rshell.c

# compiler libs
openskyd_dump1090_LIBS = @PTHREAD_LIBS@
openskyd_dump1090_LDADD = ../core/libcore.a ../util/libutil.a

openskyd_dump1090_rshell_LIBS = @PTHREAD_LIBS@
openskyd_dump1090_rshell_LDADD = ../core/libcore.a ../util/libutil.a

if SOCKET_WINSOCK
openskyd_dump1090_LDADD += -lws2_32
openskyd_dump1090_rshell_LDADD += -lws2_32
endif

# common compile flags
AM_CFLAGS = @PTHREAD_CFLAGS@ -I$(top_srcdir)/src

# compile flags for development
if !LOCAL_FILES
AM_CFLAGS += -DSYSCONFDIR='"$(sysconfdir)"' -DLOCALSTATEDIR='"$(localstatedir)"'
endif

# compile flags
openskyd_dump1090_CFLAGS = $(AM_CFLAGS)
openskyd_dump1090_rshell_CFLAGS = $(AM_CFLAGS) -DDUMP1090_RSHELL

clean-local:
	-rm -f *.gcno *.gcda
