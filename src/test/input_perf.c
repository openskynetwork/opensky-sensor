/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <pthread.h>
#include "input_perf.h"
#include "core/input.h"
#include "core/beast.h"
#include "util/port/endian.h"

static uint8_t * buffer;
static size_t bufLen;
static uint8_t * curPtr;

void RC_INPUT_register() {}

void RC_INPUT_init() {}

void RC_INPUT_disconnect() {}

void RC_INPUT_connect()
{
	curPtr = buffer;
}

void RC_INPUT_setBuffer(uint8_t * buf, size_t len)
{
	buffer = buf;
	bufLen = len;
}

size_t RC_INPUT_read(uint8_t * buf, size_t len)
{
	size_t lenIn = len;

	if (len < curPtr - buffer + bufLen) {
		memcpy(buf, curPtr, len);
		if (curPtr + len == buffer + bufLen)
			curPtr = buffer;
		else
			curPtr += len;
	} else {
		pthread_testcancel();
		size_t endlen = buffer + bufLen - curPtr;
		memcpy(buf, curPtr, endlen);
		len -= endlen;
		buf += endlen;
		while (len > bufLen) {
			memcpy(buf, buffer, bufLen);
			buf += bufLen;
			len -= bufLen;
		}
		if (len)
			memcpy(buf, buffer, len);
		curPtr = buffer + len;
	}

	return lenIn;
}

size_t RC_INPUT_write(uint8_t * buf, size_t bufLen)
{
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
