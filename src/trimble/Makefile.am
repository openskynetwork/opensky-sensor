
noinst_LIBRARIES =

if RADARCAPE_STANDALONE
noinst_LIBRARIES += libtrimble-uart.a
endif
if RADARCAPE_NETWORK
noinst_LIBRARIES += libtrimble-net.a
endif
if RADARCAPE_DUMMY
noinst_LIBRARIES += libtrimble-dummy.a
endif

# sources for trimble GPS modul
common_sources = trimble_recv.h trimble_recv.c trimble_parser.h \
	trimble_parser.c trimble_input.h

libtrimble_uart_a_SOURCES = $(common_sources) trimble_input_uart.c

libtrimble_net_a_SOURCES = $(common_sources) trimble_input_network.c

libtrimble_dummy_a_SOURCES = $(common_sources) trimble_input_dummy.c

AM_CFLAGS = -I$(top_srcdir)/src
