/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <error.h>
#include <termios.h>
#include <unistd.h>
#include <stdbool.h>
#include "trimble_input.h"
#include "util/cfgfile.h"
#include "util/util.h"
#include "util/log.h"

/** Component: Prefix */
static const char PFX[] = "TRIMBLE";

/** Interval in seconds between two reconnection attempts */
#define RECONNECT_INTERVAL 10
/** UART Port */
#define UART "/dev/ttyS2"

/** file descriptor for UART */
static int fd;
/** poll set when waiting for data */
static struct pollfd fds;

static bool doConnect();
static void closeUart();

/** Register Trimble input */
void TRIMBLE_INPUT_register()
{
}

/** Initialize Trimble input */
void TRIMBLE_INPUT_init()
{
	fd = -1;
}

/** Disconnect Trimble input */
void TRIMBLE_INPUT_disconnect()
{
	closeUart();
}

/** Close connection */
static void closeUart()
{
	if (fd != -1) {
		close(fd);
		fd = -1;
	}
}

/** Reconnect Trimble input until success */
void TRIMBLE_INPUT_connect()
{
	while (!doConnect())
		sleep(RECONNECT_INTERVAL);
}

/** Connect to UART.
 * @return true upon success
 */
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
	t.c_iflag = IGNPAR | PARMRK;
	t.c_oflag = ONLCR;
	t.c_cflag = CS8 | CREAD | HUPCL | CLOCAL | B9600 | PARENB | PARODD;
	t.c_lflag &= ~(ISIG | ICANON | IEXTEN | ECHO);
	t.c_ispeed = B9600;
	t.c_ospeed = B9600;
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

/** Read from Trimble receiver.
 * @param buf buffer to read into
 * @param bufLen buffer size
 * @return used buffer length or 0 on failure
 */
size_t TRIMBLE_INPUT_read(uint8_t * buf, size_t bufLen)
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

/** Write to Trimble receiver.
 * @param buf buffer to be written
 * @param bufLen length of buffer
 * @return number of bytes written or 0 on failure
 */
size_t TRIMBLE_INPUT_write(const uint8_t * buf, size_t bufLen)
{
	ssize_t rc = write(fd, buf, bufLen);
	if (unlikely(rc <= 0))
		return 0;
	return rc;
}
