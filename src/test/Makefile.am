
LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
  $(top_srcdir)/build-aux/tap-driver.sh

check_LIBRARIES = libtest.a
libtest_a_SOURCES = ../buffer.h ../buffer.c ../cfgfile.h \
	../statistics.h ../statistics.c ../component.h ../component.c \
	../util.h ../util.c ../message.h ../message.c
libtest_a_CFLAGS = $(AM_CFLAGS)

AM_CFLAGS = -I$(top_srcdir)/src @PTHREAD_CFLAGS@

check_PROGRAMS = test_buffer
test_buffer_SOURCES = test_buffer.c

test_buffer_CFLAGS = @CHECK_CFLAGS@ $(AM_CFLAGS)
LDADD = @CHECK_LIBS@ @PTHREAD_LIBS@ @PTHREAD_CFLAGS@ libtest.a

if TEST_COVERAGE
libtest_a_CFLAGS += -fprofile-arcs -ftest-coverage
test_buffer_LDFLAGS = -fprofile-arcs -ftest-coverage

lcov: check
	lcov --capture --directory .. --base-directory $(PWD) \
		--output-file coverage.info
	genhtml coverage.info --output-directory out

clean-local:
	-rm -rf *.gcno *.gcda coverage.info out
endif

TESTS = $(check_PROGRAMS)

