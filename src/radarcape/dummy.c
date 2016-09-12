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
#include "rc-input.h"

static bool first;
static uint_fast32_t x, x2;

void RC_INPUT_register()
{
}

void RC_INPUT_init()
{
	srandom(time(NULL));
	first = true;
	x = x2 = 0;
}

void RC_INPUT_destruct() {}

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

size_t RC_INPUT_read(uint8_t * buf, size_t bufLen)
{
	uint8_t mbuf[46] = { 0x1a };

	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	uint64_t sod = tp.tv_sec % (60 * 60 * 24);
	uint64_t mlat = be64toh((sod << 30) | tp.tv_nsec);

	uint8_t * ptr = mbuf + 2;
	encode(&ptr, ((uint8_t*)&mlat) + 2, 6);

	if (first) {
		first = false;
		mbuf[1] = '4';
		memset(ptr, 0, 15);
		ptr += 15;
	} else {
		mbuf[1] = '3';
		append(&ptr, (random() & 0xff) - 0x7f);
		append(&ptr, 0x8a);
		uint_fast32_t i;
		for (i = 10; i < 23; ++i)
			append(&ptr, random() & 0xff);
	}
	size_t len = ptr - mbuf;
	size_t l = bufLen < len ? bufLen : len;
	memcpy(buf, mbuf, l);
	uint_fast32_t k = s2(pow(x / 52., 2.)) * 200000;
	x = (x + 1) % 53;
	uint_fast32_t k2 = s2(pow(x2 / 400., 2.)) * 20000;
	x2 = (x2 + 1) % 401;
	uint_fast32_t s = 5000ul + random() % 100000ul + k + k2;
	usleep(s);
	return l;
}

size_t RC_INPUT_write(uint8_t * buf, size_t bufLen)
{
	return bufLen;
}
