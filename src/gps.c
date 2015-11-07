/* Copyright (c) 2015 Sero Systems <contact at sero-systems dot de> */

#include <gps.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <termios.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <inttypes.h>

static int fd;
static struct pollfd fds;

static void UART_open(const char * uart);
static size_t readFrame(uint8_t * buf, size_t len);
static uint16_t tou16(const uint8_t * buf);
static int16_t toi16(const uint8_t * buf);
static uint32_t tou32(const uint8_t * buf);
//static int32_t toi32(const uint8_t * buf);
static uint64_t tou64(const uint8_t * buf);
//static float tofloat(const uint8_t * buf);
static double todouble(const uint8_t * buf);

static void timeFrame(const uint8_t * buf);
static void posFrame(const uint8_t * buf);

static void construct();
static void destruct();
static void mainloop();

struct Component GPS_comp = {
	.description = "GPS",
	.construct = &construct,
	.destruct = &destruct,
	.main = &mainloop
};

static const time_t gpsEpoch = 315964800;

_Atomic enum GPS_TIME_FLAGS GPS_timeFlags = ATOMIC_VAR_INIT(GPS_TIME_FLAG_NONE);

static void construct()
{
	UART_open("/dev/ttyO2");

	const uint8_t setutc[] = { 0x10, 0x35, 0x04, 0x00, 0x01, 0x08, 0x10, 0x03 };
	(void)write(fd, setutc, sizeof setutc);
}

static void destruct()
{
	close(fd);
}

static void mainloop()
{
	while (true) {
		uint8_t buf[1024];
		size_t len = readFrame(buf, sizeof buf);
		if (!len)
			continue;
		switch (buf[0]) {
		case 0x8f:
			if (len < 2)
				continue;
			switch (buf[1]) {
			case 0xab:
				if (len == 18)
					timeFrame(buf + 1);
				break;
			case 0xac:
				if (len == 69)
					posFrame(buf + 1);
				break;
			/*case 0xa2:
				if (len == 3)
					ioFrame2(buf + 1);*/
			}
			break;
		case 0x46:
			//statFrame(buf);
			break;
		case 0x55:
			//ioFrame(buf);
			break;
		}
	}
}

static void timeFrame(const uint8_t * buf)
{
	uint_fast32_t secondsOfWeek = tou32(buf + 1);
	uint_fast16_t week = tou16(buf + 5);
	int_fast16_t offset = toi16(buf + 7);
	bool testMode = !!(buf[9] & 0x10);
	bool hasOffset = !(buf[9] & 0x08);
	bool hasGpsTime = !(buf[9] & 0x04);
	bool isUTCTime = !!(buf[9] & 0x01);
	bool hasWeek = week != 1024;

	enum GPS_TIME_FLAGS flags;
	flags = (testMode ? 0 : GPS_TIME_FLAG_NON_TEST_MODE) |
		(hasGpsTime ? GPS_TIME_FLAG_HAS_GPS_TIME : 0) |
		(hasWeek ? GPS_TIME_FLAG_HAS_WEEK : 0) |
		(hasOffset ? GPS_TIME_FLAG_HAS_OFFSET : 0) |
		(isUTCTime ? GPS_TIME_FLAG_IS_UTC : 0);
	atomic_store_explicit(&GPS_timeFlags, flags, memory_order_relaxed);
#if 0
	enum GPS_TIME_STATUS stat = preGpsTime ? GPS_TIME_PRE : hasGpsTime ? GPS_TIME_FULL : GPS_TIME_NONE;

	if (!testMode) {
		//printf("isUTC: %d 0x%02x\n", isUTCTime, buf[9]);
		time_t unixtime = secondsOfWeek + week * 7 * 24 * 60 * 60 - offset +
			gps0;
		pthread_mutex_lock(&GPS_gps.mutex);
		GPS_gps.unixtime = unixtime;
		if (stat != GPS_gps.timeStatus)
			printf("GPS: timeStatus changed from %d to %d -> %s", GPS_gps.timeStatus, stat,
				ctime(&unixtime));
		GPS_gps.timeStatus = stat;
		if (hasOffset != GPS_gps.hasOffset)
			printf("GPS: hasOffset changed from %d to %d\n", GPS_gps.hasOffset, hasOffset);
		GPS_gps.hasOffset = hasOffset;
		pthread_mutex_unlock(&GPS_gps.mutex);
	}

	/*if (++GPS_gps.ctr == 10) {
		GPS_gps.ctr = 0;
		const char buf[] = { 0x10, 0x35, 0x10, 0x03 };
		(void)write(fd, buf, sizeof buf);

		const uint8_t setio3[] = { 0x10, 0x8e, 0xa2, 0x10, 0x03 };
		(void)write(fd, setio3, sizeof setio3);
	}*/
#endif
}

#define R2D 57.2957795130823208767981548141051703

static void posFrame(const uint8_t * buf)
{
	double latitude = todouble(buf + 36) * R2D;
	double longitude = todouble(buf + 44) * R2D;
	//double alt = todouble(buf + 52);
	uint16_t alarms = tou16(buf + 10);
	bool antError = !!(alarms & 3);
	bool fixes = buf[12] == 0;

#if 0
	pthread_mutex_lock(&GPS_gps.mutex);
	if (antError != GPS_gps.antennaError)
		printf("GPS: antennaError changed from %d to %d\n", GPS_gps.antennaError, antError);
	GPS_gps.antennaError = antError;
	if (fixes != GPS_gps.hasPosition)
		printf("GPS: hasPosition changed from %d to %d -> %.2f %.2f\n",
			GPS_gps.hasPosition, fixes, latitude, longitude);
	GPS_gps.hasPosition = fixes;
	GPS_gps.latitude = latitude;
	GPS_gps.longitude = longitude;
	pthread_mutex_unlock(&GPS_gps.mutex);
#endif
}

static void UART_open(const char * uart)
{
	fd = open(uart, O_RDWR, O_NONBLOCK | O_NOCTTY | O_CLOEXEC);
	if (fd < 0)
		error(EXIT_FAILURE, errno, "GPS: Could not open UART at '%s'", uart);

	/* set uart options */
	struct termios t;
	if (tcgetattr(fd, &t) == -1)
		error(EXIT_FAILURE, errno, "GPS: tcgetattr failed");

	t.c_iflag = IGNPAR | PARMRK;
	t.c_oflag = ONLCR;
	t.c_cflag = CS8 | CREAD | HUPCL | CLOCAL | B9600 | PARENB | PARODD;
	t.c_lflag &= ~(ISIG | ICANON | IEXTEN | ECHO);
	t.c_ispeed = B9600;
	t.c_ospeed = B9600;

	if (tcsetattr(fd, TCSANOW, &t))
		error(EXIT_FAILURE, errno, "GPS: tcsetattr failed");

	tcflush(fd, TCIOFLUSH);

	/* setup polling */
	fds.events = POLLIN;
	fds.fd = fd;
}

static size_t UART_read(uint8_t * buf, size_t bufLen)
{
	while (true) {
		ssize_t rc = read(fd, buf, bufLen);
		if (rc == -1) {
			if (errno == EAGAIN) {
				poll(&fds, 1, -1);
			} else {
				error(EXIT_FAILURE, errno, "INPUT: read failed");
			}
		} else if (rc == 0) {
			poll(&fds, 1, -1);
		} else {
			return rc;
		}
	}
}

static uint8_t buf[128];
static size_t len = 0;
static uint8_t * cur = buf;

/** Discard buffer content and fill it again. */
static inline void discardAndFill()
{
	size_t rc = UART_read(buf, sizeof buf);
	if (rc == 0)
		error(EXIT_FAILURE, errno, "read failed");
	len = rc;
	cur = buf;
}

static inline void consume()
{
	if (cur >= buf + len)
		error(EXIT_FAILURE, 0, "assertion cur >= buf + len violated");
	++cur;
}

static inline uint8_t peek()
{
	do {
		if (cur < buf + len)
			return *cur;
		discardAndFill();
	} while (true);
}

static inline uint8_t next()
{
	uint8_t ret = peek();
	consume();
	return ret;
}

static inline void synchronize()
{
	do {
		uint8_t * esc = memchr(cur, 0x10, len - (cur - buf));
		if (esc) {
			cur = esc;
			return;
		}
		discardAndFill();
	} while (true);
}

static size_t readFrame(uint8_t * buf, size_t len)
{
	uint8_t * bufEnd = buf + len;

	while (true) {
		uint8_t ch = next();
		if (ch == 0x10)
			break;
		printf("Start: Out of Sync, received 0x%02" PRIx8 "instead of 0x10\n",
			ch);
		synchronize();
	}
	while (buf < bufEnd) {
		uint8_t ch = next();
		if (ch == 0x10) {
			ch = peek();
			if (ch == 0x03) {
				consume();
				return len - (bufEnd - buf);
			}
			if (ch == 0x10) {
				consume();
				*buf++ = 0x10;
			} else {
				printf(
					"Decode: Out of Sync, received 0x%02" PRIx8 "instead of 0x10\n",
					ch);
			}
		} else
			*buf++ = ch;
	}
	return 0;
}

static uint16_t tou16(const uint8_t * buf)
{
	uint16_t u;
	memcpy(&u, buf, sizeof u);
	return be16toh(u);
}

static int16_t toi16(const uint8_t * buf)
{
	union {
		uint16_t u;
		int16_t i;
	} c;
	c.u = tou16(buf);
	return c.i;
}

static uint32_t tou32(const uint8_t * buf)
{
	uint32_t u;
	memcpy(&u, buf, sizeof u);
	return be32toh(u);
}

#if 0
static int32_t toi32(const uint8_t * buf)
{
	union {
		int32_t i;
		uint32_t u;
	} c;
	c.u = tou32(buf);
	return c.i;
}
#endif

static uint64_t tou64(const uint8_t * buf)
{
	uint64_t u;
	memcpy(&u, buf, sizeof u);
	return be64toh(u);
}

#if 0
static float tofloat(const uint8_t * buf)
{
	union
	{
		uint32_t u;
		float f;
	} c;
	c.u = tou32(buf);
	return c.f;
}
#endif

static double todouble(const uint8_t * buf)
{
	union
	{
		uint64_t u;
		double d;
	} c;
	c.u = tou64(buf);
	return c.d;
}
