
LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
  $(top_srcdir)/build-aux/tap-driver.sh

if WITH_CHECK
TESTS = test_buffer test_radarcape test_recv test_gps test_lib
check_PROGRAMS = $(TESTS) perf_radarcape perf_buffer
check_LIBRARIES = libtest.a
endif

libtest_a_SOURCES = ../buffer.h ../buffer.c ../cfgfile.h \
	../statistics.h ../statistics.c ../component.h ../component.c \
	../util.h ../message.h ../message.c ../input.h ../radarcape/radarcape.c \
	../recv.h ../recv.c ../gps.h ../gps.c \
	../filter.h ../filter.c ../log.h ../log_stdio.c glue.c
libtest_a_CFLAGS = $(AM_CFLAGS)

AM_CFLAGS = -I$(top_srcdir)/src @PTHREAD_CFLAGS@ -DCHECK
AM_CXXFLAGS = -I$(top_srcdir)/src @PTHREAD_CFLAGS@ -DCHECK
LDADD = @CHECK_LIBS@ @PTHREAD_LIBS@ @PTHREAD_CFLAGS@ libtest.a

test_buffer_SOURCES = test_buffer.c
test_buffer_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)

test_radarcape_SOURCES = test_radarcape.c input_test.h input_test.c
test_radarcape_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)

test_recv_SOURCES = test_recv.c input_test.h input_test.c
test_recv_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)

test_gps_SOURCES = test_gps.c
test_gps_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)

test_lib_SOURCES = test_lib.cc ../lib/opensky.hh ../lib/opensky.cc
test_lib_CXXFLAGS = @CHECK_CFLAGS@ $(AM_CXXFLAGS) -I$(srcdir)/../lib

perf_radarcape_SOURCES = perf_radarcape.c input_perf.h input_perf.c \
	../statistics.h ../statistics.c ../input.h ../radarcape/radarcape.c \
	../buffer.h ../buffer.c ../component.h ../component.c \
	../log.h ../log_stdio.c
perf_radarcape_CFLAGS = $(AM_CFLAGS)
perf_radarcape_LDADD = @PTHREAD_LIBS@ @PTHREAD_CFLAGS@
perf_radarcape_LDFLAGS =

perf_buffer_SOURCES = perf_buffer.c input_perf.h input_perf.c \
	../statistics.h ../statistics.c ../input.h ../radarcape/radarcape.c \
	../buffer.h ../buffer.c ../component.h ../component.c \
	../recv.h ../recv.c ../filter.h ../filter.c \
	../log.h ../log_stdio.c
perf_buffer_CFLAGS = $(AM_CFLAGS)
perf_buffer_LDADD = @PTHREAD_LIBS@ @PTHREAD_CFLAGS@
perf_buffer_LDFLAGS = 

if TEST_COVERAGE
libtest_a_CFLAGS += -fprofile-arcs -ftest-coverage
test_buffer_LDFLAGS = -fprofile-arcs -ftest-coverage
test_radarcape_LDFLAGS = -fprofile-arcs -ftest-coverage
test_recv_LDFLAGS = -fprofile-arcs -ftest-coverage
test_gps_LDFLAGS = -fprofile-arcs -ftest-coverage
test_lib_CXXFLAGS +=  -fprofile-arcs -ftest-coverage
test_lib_LDFLAGS =  -fprofile-arcs -ftest-coverage

lcov: check
	lcov --capture --directory .. --base-directory $(PWD) \
		--output-file coverage.info
	genhtml coverage.info --output-directory out

clean-local:
	-rm -rf *.gcno *.gcda coverage.info out
endif

