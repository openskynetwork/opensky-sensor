
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
#include <stdint.h>
#include <endian.h>
#include <stdio.h>
#include <inttypes.h>
#include <pthread.h>
#include <adsb.h>

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

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static struct ADSB_Frame * volatile subscribers = NULL;

static inline void discardAndFill();
static inline char peek();
static inline char next();
static inline void synchronize();

static inline void publish(struct ADSB_Frame * frame);

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

	tcflush(fd, TCIOFLUSH);

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

/** ADSB main loop: receive, decode, distribute */
void ADSB_main()
{
	while (1) {
		struct ADSB_Frame frame;

		char * rawPtr = frame.raw;

		/* synchronize */
		while (1) {
			char sync = next();
			if (sync == 0x1a) {
				*rawPtr++ = sync;
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
			synchronize();
			continue;
		}
		*rawPtr++ = type;
		frame.type = type - '0';
		frame.len = frame_len;

		frame.lenRaw = 2;

		/* decode payload */
		char decoded[21];
		char * decPtr = decoded;
		size_t i;
		for (i = 0; i < frame_len; ++i) {
			char c = next();
			*rawPtr++ = c;
			++frame.lenRaw;
			if (c == 0x1a) { /* escaped character */
				c = peek();
				if (c == 0x1a) {
					next();
					*rawPtr++ = c;
					++frame.lenRaw;
					*decPtr++ = 0x1a;
				} else {
					printf("ADSB: Out of Sync: got unescaped 0x1a in frame, "
						"treating as resynchronization\n");
					goto decode_frame;
				}
			} else {
				*decPtr++ = c;
			}
		}

		uint64_t mlat = 0;
		memcpy(((char*)&mlat) + 2, decoded, 6);
		frame.mlat = be64toh(mlat);
		frame.siglevel = decoded[6];
		memcpy(frame.frame, decoded + 7, frame_len);

		publish(&frame);
	}
}

static inline void publish(struct ADSB_Frame * frame)
{
	frame->valid = 1;
	frame->next = frame->prev = NULL;

	pthread_mutex_lock(&mutex);

	struct ADSB_Frame * list, * next;
	for (list = subscribers; list; list = next) {
		next = list->next;
		memcpy(list, frame, sizeof(struct ADSB_Frame));
	}

	subscribers = NULL;

	pthread_cond_broadcast(&cond);

	pthread_mutex_unlock(&mutex);
}

int ADSB_getFrame(struct ADSB_Frame * frame, int32_t timeout_ms)
{
	struct timespec ts;

	if (timeout_ms >= 0) {
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += timeout_ms / 1000;
		ts.tv_nsec += (timeout_ms % 1000) * 10000000;
		if (ts.tv_nsec >= 1000000000) {
			++ts.tv_sec;
			ts.tv_nsec -= 1000000000;
		}
	}

	frame->prev = NULL;
	pthread_mutex_lock(&mutex);
	frame->next = subscribers;
	if (subscribers)
		subscribers->prev = frame;
	subscribers = frame;

	frame->valid = 0;

	int r;
	do {
		if (timeout_ms < 0) {
			r = pthread_cond_wait(&cond, &mutex);
		} else {
			r = pthread_cond_timedwait(&cond, &mutex, &ts);
			if (r == ETIMEDOUT) {
				if (frame->prev)
					frame->prev->next = frame->next;
				if (frame->next)
					frame->next->prev = frame->prev;
				if (frame == subscribers)
					subscribers = frame->next;
				pthread_mutex_unlock(&mutex);
				return 0;
			}
		}
		if (r)
			error(-1, r, "pthread_cond_wait failed");
	} while (!frame->valid);

	pthread_mutex_unlock(&mutex);

	return 1;
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
