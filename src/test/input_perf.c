#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <input_perf.h>
#include <input.h>
#include <stdlib.h>
#include <stdio.h>
#include <endian.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static uint8_t * buffer;
static size_t bufLen;
static uint8_t * curPtr;

void INPUT_init() {}

void INPUT_destruct() {}

void INPUT_connect()
{
	curPtr = buffer;
}

void INPUT_setBuffer(uint8_t * buf, size_t len)
{
	buffer = buf;
	bufLen = len;
}

size_t INPUT_read(uint8_t * buf, size_t len)
{
	size_t lenIn = len;

	if (len < curPtr - buffer + bufLen) {
		memcpy(buf, curPtr, len);
		if (curPtr + len == buffer + bufLen)
			curPtr = buffer;
		else
			curPtr += len;
	} else {
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

size_t INPUT_write(uint8_t * buf, size_t bufLen)
{
	return bufLen;
}

static inline void append(uint8_t ** buf, uint8_t c)
{
	if ((*(*buf)++ = c) == 0x1a)
		*(*buf)++ = 0x1a;
}

static inline void encode(uint8_t ** buf, const uint8_t * src, size_t len)
{
	while (len--)
		append(buf, *src++);
}

size_t INPUT_buildFrame(uint8_t * buf, enum ADSB_FRAME_TYPE type, uint64_t mlat,
	int8_t siglevel, const uint8_t * payload, size_t payloadLen)
{
	buf[0] = '\x1a';
	buf[1] = type + '1';

	uint8_t * ptr = buf + 2;
	mlat = htobe64(mlat);
	encode(&ptr, ((uint8_t*)&mlat) + 2, 6);

	append(&ptr, siglevel);
	encode(&ptr, payload, payloadLen);

	return ptr - buf;
}
