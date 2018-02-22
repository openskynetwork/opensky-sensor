
# binary programs
bin_PROGRAMS =
if DUMP1090
bin_PROGRAMS += openskyd-dump1090
endif
if DUMP1090_DONATED
bin_PROGRAMS += openskyd-dump1090-donated
endif

if DUMP1090_KIT
bin_PROGRAMS += openskyd-dump1090-kit
endif

openskyd_dump1090_SOURCES = dump1090.c req-serial.h req-serial.c \
	position.h position.c \
	../radarcape/rc-parser.c ../radarcape/network.c
	
openskyd_dump1090_donated_SOURCES = $(openskyd_dump1090_SOURCES) \
	../radarcape/tb-rshell.c
	
openskyd_dump1090_kit_SOURCES = $(openskyd_dump1090_SOURCES)

# compiler libs
openskyd_dump1090_LIBS = @PTHREAD_LIBS@
openskyd_dump1090_LDADD = ../core/libcore.a ../util/libutil.a

openskyd_dump1090_donated_LIBS = @PTHREAD_LIBS@
openskyd_dump1090_donated_LDADD = ../core/libcore.a ../util/libutil.a

openskyd_dump1090_kit_LIBS = @PTHREAD_LIBS@
openskyd_dump1090_kit_LDADD = ../core/libcore.a ../util/libutil.a

if SOCKET_WINSOCK
openskyd_dump1090_LDADD += -lws2_32
openskyd_dump1090_donated_LDADD += -lws2_32
openskyd_dump1090_kit_LDADD += -lws2_32
endif

# common compile flags
AM_CFLAGS = @PTHREAD_CFLAGS@ -I$(top_srcdir)/src

# compile flags for development
if !LOCAL_FILES
AM_CFLAGS += -DSYSCONFDIR='"$(sysconfdir)"' -DLOCALSTATEDIR='"$(localstatedir)"'
endif

# compile flags
openskyd_dump1090_CFLAGS = $(AM_CFLAGS)
openskyd_dump1090_donated_CFLAGS = $(AM_CFLAGS) -DDUMP1090_DONATED
openskyd_dump1090_kit_CFLAGS = $(AM_CFLAGS) -DDUMP1090_KIT

clean-local:
	-rm -f *.gcno *.gcda
