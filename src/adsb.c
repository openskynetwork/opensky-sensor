
#define _DEFAULT_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <error.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <endian.h>
#include <inttypes.h>

/** file descriptor for UART */
static int fd;
/** poll set when waiting for data */
static struct pollfd fds;
/** receive buffer */
static char buf[128];
/** current buffer length (max sizeof buf) */
static size_t len;
/** current pointer into buffer */
static char * cur;

static inline void discardAndFill();
static inline char peek();
static inline char next();
static inline void synchronize();
static inline size_t getFrame(char * frame);

/** Initialize ADSB UART.
 * \param uart path to uart to be used
 */
void ADSB_init(const char * uart)
{
	/* open uart */
	fd = open(uart, O_RDWR, O_NONBLOCK);
	if (fd < 0)
		error(-1, errno, "ADSB: Could not open UART at '%s'", uart);

	/* set uart options */
	struct termios t;
	if (tcgetattr(fd, &t) == -1)
		error(-1, errno, "ADSB: tcgetattr failed");

	/* TOOD: really modify options to needs */

	if (tcsetattr(fd, TCSANOW, &t))
		error(-1, errno, "ADSB: tcsetattr failed");

	/* setup polling */
	fds.events = POLLIN;
	fds.fd = fd;

	/* setup buffer */
	cur = buf;
	len = 0;

	/* setup ADSB */
	write(fd, "\x1a""1C", 3);
	write(fd, "\x1a""1d", 3);
	write(fd, "\x1a""1E", 3);
	write(fd, "\x1a""1f", 3);
	write(fd, "\x1a""1G", 3);
	write(fd, "\x1a""1H", 3);
	write(fd, "\x1a""1i", 3);
	write(fd, "\x1a""1J", 3);
}

/** ADSB main loop: receive, decode, print. */
void ADSB_main()
{
	char frame[23];
	while (1) {
		getFrame(frame);
		long type = frame[1];
		if (type != '3')
			continue;
		uint64_t mlat = 0;
		memcpy(((char*)&mlat) + 2, frame + 2, 6);
		mlat = be64toh(mlat);
		int8_t siglevel = frame[8];
		printf("ADSB: Got Mode-S long frame with mlat %" PRIu64
			" and level %+4" PRIi8 ": ", mlat, siglevel);
		int i;
		for (i = 0; i < 14; ++i)
			printf("%02x", frame[i + 9]);
		printf("\n");
	}
}

/** Get an ADSB frame.
 * \param frame frame buffer, must be at least 23 bytes
 * \return length of frame in bytes
 */
static inline size_t getFrame(char * frame)
{
	while (1) {
		char * curframe = frame;

		/* synchronize */
		while (1) {
			char sync = next();
			if (sync == 0x1a) {
				*curframe++ = sync;
				break;
			}
			printf("ADSB: Out of Sync: got 0x%2d instead of 0x1a\n", sync);
			synchronize();
		}

		/* decode frame type */
		char type;
decode_frame:
		type = next();
		size_t frame_len;
		switch (type) {
		case '1': /* mode-ac */
			frame_len = 9;
			break;
		case '2': /* mode-s short */
			frame_len = 14;
			break;
		case '3': /* mode-s long */
		case '4': /* ?? */
			frame_len = 21;
			break;
		default:
			printf("ADSB: Unknown frame type %c, resynchronizing\n", type);
			continue;
		}
		*curframe++ = type;

		/* decode payload */
		size_t i;
		for (i = 0; i < frame_len; ++i) {
			char c = next();
			if (c == 0x1a) { /* escaped character */
				c = peek();
				if (c == 0x1a) {
					next();
					*curframe++ = 0x1a;
				} else {
					printf("ADSB: Out of Sync: got unescaped 0x1a in frame, "
						"treating as resynchronization\n");
					goto decode_frame;
				}
			} else {
				*curframe++ = c;
			}
		}
		return frame_len;
	}
}

/** Discard buffer content and fill it again. */
static inline void discardAndFill()
{
	while (1) {
		int rc = read(fd, buf, sizeof buf);
		if (rc == -1) {
			if (errno == EAGAIN)
				poll(&fds, 1, -1);
			else
				error(-1, errno, "ADSB: read failed");
		} else if (rc == 0) {
			poll(&fds, 1, -1);
		} else {
			len = rc;
			break;
		}
	}

	cur = buf;
}

/** Get next character from buffer but keep it there.
 * \return next character in buffer
 */
static inline char peek()
{
	while (1) {
		if (cur < buf + len)
			return *cur;

		discardAndFill();
	}
}

/** Consume next character from buffer.
 * \return next character in buffer
 */
static inline char next()
{
	char ret = peek();
	++cur;
	return ret;
}

/** Synchronize buffer.
 * \note After calling that, the next call to next() or peek() will return 0x1a.
 */
static inline void synchronize()
{
	while (1) {
		char * esc = memchr(cur, 0x1a, len - (cur - buf));
		if (esc) {
			cur = esc;
			return;
		}
		discardAndFill();
	}
}
