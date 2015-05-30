#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <input.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <inttypes.h>

static bool first;
static uint32_t x, x2;

void INPUT_init()
{
	srandom(time(NULL));
	first = true;
	x = x2 = 0;
}

void INPUT_destruct() {}

void INPUT_connect() {}

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

size_t INPUT_read(uint8_t * buf, size_t bufLen)
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
		uint32_t i;
		for (i = 10; i < 23; ++i)
			append(&ptr, random() & 0xff);
	}
	size_t len = ptr - mbuf;
	size_t l = bufLen < len ? bufLen : len;
	memcpy(buf, mbuf, l);
	uint32_t k = s2(pow(x / 52., 2.)) * 200000;
	x = (x + 1) % 53;
	uint32_t k2 = s2(pow(x2 / 400., 2.)) * 20000;
	x2 = (x2 + 1) % 401;
	uint32_t s = 5000ul + random() % 100000ul + k + k2;
	//printf("k: %6" PRIu32 ", s: %7" PRIu32 "\n", k, s);
	usleep(s);
	return l;
}

size_t INPUT_write(uint8_t * buf, size_t bufLen)
{
	return bufLen;
}
