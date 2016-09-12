/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include "rc-input.h"
#include "../cfgfile.h"
#include "../util.h"
#include "../log.h"

static const char PFX[] = "INPUT";

#define RECONNECT_INTERVAL 10
#define UART "/dev/ttyS5"

/** file descriptor for UART */
static int fd;
/** poll set when waiting for data */
static struct pollfd fds;

static bool doConnect();
static void closeUart();

void RC_INPUT_register()
{
}

void RC_INPUT_init()
{
	fd = -1;
}

void RC_INPUT_destruct()
{
	closeUart();
}

static void closeUart()
{
	if (fd != -1) {
		close(fd);
		fd = -1;
	}
}

void RC_INPUT_connect()
{
	while (!doConnect())
		sleep(RECONNECT_INTERVAL);
}

static bool doConnect()
{
	closeUart();

	/* open uart */
	fd = open(UART, O_RDWR, O_NONBLOCK | O_NOCTTY | O_CLOEXEC);
	if (fd < 0) {
		LOG_errno(LOG_LEVEL_WARN, PFX, "Could not open UART at '%s'", UART);
		fd = -1;
		return false;
	}

	/* set uart options */
	struct termios t;
	if (tcgetattr(fd, &t) == -1) {
		LOG_errno(LOG_LEVEL_WARN, PFX, "Could not get UART settings");
		closeUart();
		return false;
	}

	t.c_iflag = IGNPAR;
	t.c_oflag = ONLCR;
	t.c_cflag = CS8 | CREAD | HUPCL | CLOCAL | B3500000;
	t.c_cflag |= CRTSCTS;
	t.c_lflag &= ~(ISIG | ICANON | IEXTEN | ECHO);
	t.c_ispeed = B3500000;
	t.c_ospeed = B3500000;

	if (tcsetattr(fd, TCSANOW, &t)) {
		LOG_errno(LOG_LEVEL_WARN, PFX, "Could not set UART settings");
		closeUart();
		return false;
	}

	tcflush(fd, TCIOFLUSH);

	/* setup polling */
	fds.events = POLLIN;
	fds.fd = fd;

	return true;
}

size_t RC_INPUT_read(uint8_t * buf, size_t bufLen)
{
	while (true) {
		ssize_t rc = read(fd, buf, bufLen);
		if (unlikely(rc == -1)) {
			if (errno == EAGAIN) {
				poll(&fds, 1, -1);
			} else {
				LOG_errno(LOG_LEVEL_WARN, PFX, "Could not read from UART");
				closeUart();
				return 0;
			}
		} else if (unlikely(rc == 0)) {
			poll(&fds, 1, -1);
		} else {
			return rc;
		}
	}
}

size_t RC_INPUT_write(uint8_t * buf, size_t bufLen)
{
	ssize_t rc = write(fd, buf, bufLen);
	if (unlikely(rc <= 0))
		return 0;
	return rc;
}
