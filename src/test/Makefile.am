
LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
  $(top_srcdir)/build-aux/tap-driver.sh

check_LIBRARIES = libtest.a
libtest_a_SOURCES = ../buffer.h ../buffer.c ../cfgfile.h \
	../statistics.h ../statistics.c ../component.h ../component.c \
	../util.h ../util.c ../message.h ../message.c ../adsb.h ../adsb.c \
	../recv.h ../recv.c
libtest_a_CFLAGS = $(AM_CFLAGS)

AM_CFLAGS = -I$(top_srcdir)/src @PTHREAD_CFLAGS@
LDADD = @CHECK_LIBS@ @PTHREAD_LIBS@ @PTHREAD_CFLAGS@ libtest.a

TESTS = test_buffer test_adsb test_recv
check_PROGRAMS = $(TESTS) perf_adsb perf_buffer

test_buffer_SOURCES = test_buffer.c
test_buffer_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)

test_adsb_SOURCES = test_adsb.c input_test.h input_test.c
test_adsb_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)

test_recv_SOURCES = test_recv.c input_test.h input_test.c
test_recv_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)

perf_adsb_SOURCES = perf_adsb.c input_perf.h input_perf.c \
	../statistics.h ../statistics.c ../adsb.h ../adsb.c \
	../buffer.h ../buffer.c ../component.h ../component.c
perf_adsb_CFLAGS = $(AM_CFLAGS)
perf_adsb_LDADD = @PTHREAD_LIBS@ @PTHREAD_CFLAGS@

perf_buffer_SOURCES = perf_buffer.c input_perf.h input_perf.c \
	../statistics.h ../statistics.c ../adsb.h ../adsb.c \
	../buffer.h ../buffer.c ../component.h ../component.c \
	../recv.h ../recv.c
perf_buffer_CFLAGS = $(AM_CFLAGS)
perf_buffer_LDADD = @PTHREAD_LIBS@ @PTHREAD_CFLAGS@
perf_buffer_LDFLAGS = 

if TEST_COVERAGE
libtest_a_CFLAGS += -fprofile-arcs -ftest-coverage
test_buffer_LDFLAGS = -fprofile-arcs -ftest-coverage
test_adsb_LDFLAGS = -fprofile-arcs -ftest-coverage
test_recv_LDFLAGS = -fprofile-arcs -ftest-coverage

lcov: check
	lcov --capture --directory .. --base-directory $(PWD) \
		--output-file coverage.info
	genhtml coverage.info --output-directory out

clean-local:
	-rm -rf *.gcno *.gcda coverage.info out
endif

