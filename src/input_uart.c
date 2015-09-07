#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <error.h>
#include <termios.h>
#include <unistd.h>
#include <cfgfile.h>
#include <util.h>

/** file descriptor for UART */
static int fd;
/** poll set when waiting for data */
static struct pollfd fds;

static bool doConnect();
static void closeUart();

void INPUT_init()
{
	fd = -1;
}

void INPUT_destruct()
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

void INPUT_connect()
{
	while(!doConnect())
		sleep(CFG_config.input.reconnectInterval);
}

static bool doConnect()
{
	closeUart();

	/* open uart */
	fd = open(CFG_config.input.uart, O_RDWR, O_NONBLOCK | O_NOCTTY | O_CLOEXEC);
	if (fd < 0) {
		error(0, errno, "INPUT: Could not open UART at '%s'",
			CFG_config.input.uart);
		fd = -1;
		return false;
	}

	/* set uart options */
	struct termios t;
	if (tcgetattr(fd, &t) == -1) {
		error(0, errno, "INPUT: tcgetattr failed");
		closeUart();
		return false;
	}

	t.c_iflag = IGNPAR;
	t.c_oflag = ONLCR;
	t.c_cflag = CS8 | CREAD | HUPCL | CLOCAL | B3500000;
	if (CFG_config.input.rtscts)
		t.c_cflag |= CRTSCTS;
	t.c_lflag &= ~(ISIG | ICANON | IEXTEN | ECHO);
	t.c_ispeed = B3500000;
	t.c_ospeed = B3500000;

	if (tcsetattr(fd, TCSANOW, &t)) {
		error(0, errno, "INPUT: tcsetattr failed");
		closeUart();
		return false;
	}

	tcflush(fd, TCIOFLUSH);

	/* setup polling */
	fds.events = POLLIN;
	fds.fd = fd;

	return true;
}

size_t INPUT_read(uint8_t * buf, size_t bufLen)
{
	while (true) {
		ssize_t rc = read(fd, buf, bufLen);
		if (unlikely(rc == -1)) {
			if (errno == EAGAIN) {
				poll(&fds, 1, -1);
			} else {
				error(0, errno, "INPUT: read failed");
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

size_t INPUT_write(uint8_t * buf, size_t bufLen)
{
	ssize_t rc = write(fd, buf, bufLen);
	if (unlikely(rc <= 0))
		return 0;
	return rc;
}
