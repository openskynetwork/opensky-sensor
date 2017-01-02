
noinst_LIBRARIES = libutil.a

# utility sources of opensky sensor
libutil_a_SOURCES = list.h cfgfile.h cfgfile.c component.h component.c \
	endec.h log.h log_stdio.c net_common.h proc.h proc.c \
	statistics.h statistics.c threads.h util.h util.c \
	serial_eth.h \
	port/endian.h port/misc.h port/socket.h port/mmap.h
	
if SOCKET_POSIX
libutil_a_SOURCES += net_common-posix.c serial_eth.c
else
libutil_a_SOURCES += net_common-winsock.c
endif

if MMAP_POSIX
libutil_a_SOURCES += port/mmap-posix.c
else
libutil_a_SOURCES += port/mmap-windows.c
endif

AM_CFLAGS = -I$(top_srcdir)/src
