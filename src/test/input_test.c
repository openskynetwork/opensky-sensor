#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <input_test.h>
#include <check.h>
#include <input.h>
#include <stdlib.h>
#include <stdio.h>
#include <endian.h>

struct TEST test;

void INPUT_init()
{
	test.init = true;
}

void INPUT_destruct()
{
	test.destruct = true;
}

void INPUT_connect()
{
	++test.connect;
}

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

size_t INPUT_read(uint8_t * buf, size_t bufLen)
{
	if (test.hasRead)
		return 0;
	test.hasRead = true;

	if (test.frmRaw) {
		size_t l = bufLen < test.frmMsgLen ? bufLen : test.frmMsgLen;
		memcpy(buf, test.frmMsg, l);
		memcpy(test.raw, test.frmMsg, test.rawLen = l);
		return l;
	} else {
		uint8_t mbuf[46] = { 0x1a };

		uint8_t * ptr = mbuf + 2;
		uint64_t mlat = htobe64(test.frmMlat);
		encode(&ptr, ((uint8_t*)&mlat) + 2, 6);

		mbuf[1] = test.frmType + '1';
		append(&ptr, test.frmSigLevel);
		if (test.frmRaw) {
			memcpy(ptr, test.frmMsg, test.frmMsgLen);
			ptr += test.frmMsgLen;
		} else {
			encode(&ptr, test.frmMsg, test.frmMsgLen);
		}
		size_t len = ptr - mbuf;
		size_t l = bufLen < len ? bufLen : len;
		memcpy(buf, mbuf, l);

		memcpy(test.raw, mbuf, test.rawLen = len);

		return l;
	}
}

size_t INPUT_write(uint8_t * buf, size_t bufLen)
{
	if (test.testAck != -1) {
		if (test.testAck--)
			return 0;
	}

	ck_assert(!(bufLen % 3));
	uint32_t off;
	for (off = 0; off < bufLen; off += 3) {
		ck_assert_int_eq(buf[off + 0], '\x1a');
		ck_assert_int_eq(buf[off + 1], '1');
		uint8_t type = buf[off + 2];
		ck_assert(('c' <= type && type <= 'j') || ('C' <= type && type <= 'J'));
		uint8_t lower = tolower(type);
		test.params[lower - 'c'] = lower == type ? false : true;
	}
	++test.write;
	test.write_bytes += bufLen;
	return bufLen;
}
