
# binary programs
bin_PROGRAMS =
if WINFEEDER
bin_PROGRAMS += openskyd-win32
endif

openskyd_win32_SOURCES = winmain.c resource.h opensky.rc
dist_data_DATA = opensky.ico

# compiler libs
openskyd_win32_LIBS = @PTHREAD_LIBS@
openskyd_win32_LDADD = ../core/libcore.a ../util/libutil.a -lws2_32 -lcomctl32
openskyd_win32_LDFLAGS = -mwindows -mconsole

# common compile flags
AM_CFLAGS = @PTHREAD_CFLAGS@ -I$(top_srcdir)/src \
	-DUNICODE -D_UNICODE -D_WIN32_IE=0x0500 -DWINVER=0x500 

# compile flags for development
if !LOCAL_FILES
AM_CFLAGS += -DSYSCONFDIR='"$(sysconfdir)"' -DLOCALSTATEDIR='"$(localstatedir)"'
endif

.rc.o:
	$(WINDRES) $^ -o $@

clean-local:
	-rm -f *.gcno *.gcda
