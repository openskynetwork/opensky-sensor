
# binary programs
bin_PROGRAMS =
if WINFEEDER
bin_PROGRAMS += openskyd-win32
endif

openskyd_win32_SOURCES = win32.c opensky.rc

# compiler libs
openskyd_win32_LIBS = @PTHREAD_LIBS@
openskyd_win32_LDADD = ../core/libcore.a ../util/libutil.a -lws2_32
openskyd_win32_LDFLAGS = -mwindows -mconsole

# common compile flags
AM_CFLAGS = @PTHREAD_CFLAGS@ -I$(top_srcdir)/src

if HOST_IS_MINGW
# add information and icon to binary
openskyd_dump1090_SOURCES += opensky.rc
endif
dist_data_DATA = opensky.ico

# compile flags for development
if !LOCAL_FILES
AM_CFLAGS += -DSYSCONFDIR='"$(sysconfdir)"' -DLOCALSTATEDIR='"$(localstatedir)"'
endif

.rc.o:
	$(WINDRES) $^ -o $@

clean-local:
	-rm -f *.gcno *.gcda
