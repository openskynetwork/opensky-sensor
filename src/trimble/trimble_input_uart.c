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
#include "util/threads.h"

/** Component: Prefix */
static const char PFX[] = "TRIMBLE";

#define BOM 0x10
#define EOM 0x03

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
		sleepCancelable(RECONNECT_INTERVAL);
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
	t.c_cflag = CS8 | CREAD | HUPCL | CLOCAL | PARENB | PARODD;
	t.c_lflag &= ~(ISIG | ICANON | IEXTEN | ECHO);
	cfsetispeed(&t, B9600);
	cfsetospeed(&t, B9600);
	if (tcsetattr(fd, TCSANOW, &t)) {
		LOG_errno(LOG_LEVEL_WARN, PFX, "Could not set UART settings");
		closeUart();
		return false;
	}
	tcflush(fd, TCIOFLUSH);

	const uint8_t setSpeed[] = { BOM, 0xbc, 0x00, 0x0b, 0x0b, 0x03, 0x01, 0x00,
		0x00, 0x02, 0x02, 0x00, BOM, EOM };
	if (TRIMBLE_INPUT_write(setSpeed, sizeof setSpeed) != sizeof setSpeed)
		return false;
	t.c_iflag = IGNPAR;
	cfsetispeed(&t, B115200);
	cfsetospeed(&t, B115200);
	if (tcsetattr(fd, TCSADRAIN, &t)) {
		LOG_errno(LOG_LEVEL_WARN, PFX, "Could not set UART settings");
		closeUart();
		return false;
	}
	if (TRIMBLE_INPUT_write(setSpeed, sizeof setSpeed) != sizeof setSpeed)
		return false;
	tcdrain(fd);
	tcflush(fd, TCIFLUSH);

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
