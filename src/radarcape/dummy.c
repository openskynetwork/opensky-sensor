/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <inttypes.h>
#include <stdbool.h>
#include "core/beast.h"
#include "rc-input.h"

/** First time: send a status packet once */
static bool first;
/** Timing state */
static uint_fast32_t x, x2;

/** Register Radarcape input */
void RC_INPUT_register()
{
}

/** Initialize Radarcape input */
void RC_INPUT_init()
{
	srandom(time(NULL));
	first = true;
	x = x2 = 0;
}

/** Disconnect Radarcape input */
void RC_INPUT_disconnect() {}

/** Reconnect Radarcape receiver until success */
void RC_INPUT_connect() {}

static void inline append(uint8_t ** buf, uint8_t c)
{
	if ((*(*buf)++ = c) == 0x1a)
		*(*buf)++ = 0x1a;
}

static void inline encode(uint8_t ** buf, const uint8_t * src, size_t len)
{
	while (len--)
		append(buf, *src++);
}

static double s(double x)
{
	return (1 + sin(x * M_PI + 3. / 2. * M_PI)) / 2.;
}

static double s2(double x)
{
	return s(s(s(s(x))) * 2.);
}

/** Read from Radarcape receiver.
 * @param buf buffer to read into
 * @param bufLen buffer size
 * @return used buffer length or 0 on failure
 */
size_t RC_INPUT_read(uint8_t * buf, size_t bufLen)
{
	uint8_t mbuf[15];
	uint8_t ebuf[43] = { BEAST_SYNC };

	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	uint64_t sod = tp.tv_sec % (60 * 60 * 24);
	uint64_t mlat = be64toh((sod << 30) | tp.tv_nsec);

	uint8_t * ptr = ebuf + 2;
	ptr +=  BEAST_encode(ptr, ((uint8_t*)&mlat) + 2, 6);

	if (first) {
		first = false;
		ebuf[1] = '4';
		memset(mbuf, 0, sizeof mbuf);
	} else {
		ebuf[1] = '3';
		mbuf[0] = (random() & 0xff) - 0x7f;
		mbuf[1] = 0x8a;
		uint_fast32_t i;
		for (i = 2; i < 14; ++i)
			mbuf[i] = random() & 0xff;
	}
	ptr += BEAST_encode(ptr, mbuf, 15);

	size_t len = ptr - ebuf;
	size_t l = bufLen < len ? bufLen : len;
	memcpy(buf, ebuf, l);
	uint_fast32_t k = s2(pow(x / 52., 2.)) * 200000;
	x = (x + 1) % 53;
	uint_fast32_t k2 = s2(pow(x2 / 400., 2.)) * 20000;
	x2 = (x2 + 1) % 401;
	uint_fast32_t s = 5000ul + random() % 100000ul + k + k2;
	usleep(s);
	return l;
}

/** Write to Radarcape receiver.
 * @param buf buffer to be written
 * @param bufLen length of buffer
 * @return number of bytes written or 0 on failure
 */
size_t RC_INPUT_write(uint8_t * buf, size_t bufLen)
{
	return bufLen;
}
