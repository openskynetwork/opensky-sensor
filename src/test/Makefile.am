
LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) \
  $(top_srcdir)/build-aux/tap-driver.sh

TESTS = test_buffer
check_PROGRAMS = test_buffer
test_buffer_SOURCES = test_buffer.c ../buffer.h ../buffer.c ../cfgfile.h \
	../statistics.h ../statistics.c ../component.h ../component.c \
	../util.h ../util.c ../message.h ../message.c
test_buffer_CFLAGS = @CHECK_CFLAGS@ -I..
# TODO: add -fprofile-arcs -ftest-coverage if needed
test_buffer_LDADD = @CHECK_LIBS@
