/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <check.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>
#include "input_test.h"
#include "core/openskytypes.h"
#include "core/input.h"
#include "radarcape/rc-input.h"
#include "util/port/endian.h"
#include "util/threads.h"

struct TEST test;

void RC_INPUT_register() {}

void RC_INPUT_init()
{
	test.init = true;
}

void RC_INPUT_disconnect()
{
	test.destruct = true;
}

void RC_INPUT_connect()
{
	++test.connect;
}

size_t RC_INPUT_read(uint8_t * buf, size_t bufLen)
{
	if (!test.buffers)
		return 0;
	if (test.curBuffer == test.nBuffers) {
		if (test.noRet)
			while (true)
				sleepCancelable(10);
		else
			return 0;
	}

	struct TEST_Buffer * tbuf = &test.buffers[test.curBuffer];

	size_t l = bufLen < tbuf->length ? bufLen : tbuf->length;
	memcpy(buf, tbuf->payload, l);
	tbuf->payload += l;
	if ((tbuf->length -= l) == 0)
		test.curBuffer++;
	return l;
}

size_t RC_INPUT_write(uint8_t * buf, size_t bufLen)
{
	if (test.testAck != -1) {
		if (test.testAck--)
			return 0;
	}

	ck_assert(!(bufLen % 3));
	uint32_t off;
	for (off = 0; off < bufLen; off += 3) {
		ck_assert_int_eq(buf[off + 0], BEAST_SYNC);
		ck_assert_int_eq(buf[off + 1], '1');
		uint8_t type = buf[off + 2];
		ck_assert(('c' <= type && type <= 'j') ||
			('C' <= type && type <= 'J') ||
			(type == 'Y' || type == 'R'));
		uint8_t lower = tolower(type);
		test.params[lower - 'c'] = lower == type ? false : true;
	}
	++test.write;
	test.write_bytes += bufLen;
	return bufLen;
}

static inline void append(uint8_t ** buf, uint8_t c)
{
	if ((*(*buf)++ = c) == BEAST_SYNC)
		*(*buf)++ = BEAST_SYNC;
}

static inline void encode(uint8_t ** buf, const uint8_t * src, size_t len)
{
	while (len--)
		append(buf, *src++);
}

size_t RC_INPUT_buildFrame(uint8_t * buf, enum OPENSKY_FRAME_TYPE type,
	uint64_t mlat, int8_t siglevel, const char * payload, size_t payloadLen)
{
	buf[0] = BEAST_SYNC;
	buf[1] = type;

	uint8_t * ptr = buf + 2;
	mlat = htobe64(mlat);
	encode(&ptr, ((uint8_t*)&mlat) + 2, 6);

	append(&ptr, siglevel);
	encode(&ptr, (const uint8_t*)payload, payloadLen);

	return ptr - buf;
}
