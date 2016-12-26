/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <check.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include "input_test.h"
#include "core/openskytypes.h"
#include "core/input.h"
#include "core/filter.h"
#include "core/buffer.h"
#include "util/component.h"
#include "util/cfgfile.h"
#include "radarcape/radarcape.h"

static void setup()
{
	test.testAck = -1;
	COMP_register(&INPUT_comp);
	COMP_register(&FILTER_comp);
	COMP_fixup();
	CFG_loadDefaults();
}

START_TEST(test_init_destruct)
{
	COMP_initAll();
	ck_assert(test.init);
	INPUT_disconnect();
	ck_assert(test.destruct);
}
END_TEST

START_TEST(test_connect)
{
	COMP_initAll();
	COMP_startAll();
	ck_assert(test.init);
	INPUT_connect();
	ck_assert_uint_eq(test.write, 10);
}
END_TEST

static void configAssert(char testC)
{
	char lower = tolower(testC);
	bool expect = testC == lower ? false : true;

	test.params[lower - 'c'] = !expect;
	COMP_initAll();
	COMP_startAll();
	ck_assert(test.init);
	INPUT_connect();
	ck_assert_uint_eq(test.write, 10);
	ck_assert_int_eq(!!test.params[lower - 'c'], !!expect);
}

START_TEST(test_config_bin)
{
	configAssert('C');
}
END_TEST

START_TEST(test_config_frameFilter_0)
{
	CFG_setBoolean("FILTER", "ModeSExtSquitterOnly", true);
	configAssert('D');
}
END_TEST

START_TEST(test_config_frameFilter_1)
{
	CFG_setBoolean("FILTER", "ModeSExtSquitterOnly", false);
	configAssert('d');
}
END_TEST

START_TEST(test_config_crc_0)
{
	CFG_setBoolean("FILTER", "CRC", false);
	configAssert('F');
}
END_TEST

START_TEST(test_config_crc_1)
{
	CFG_setBoolean("FILTER", "CRC", true);
	configAssert('f');
}
END_TEST

START_TEST(test_config_gps)
{
	configAssert('G');
}
END_TEST

START_TEST(test_config_rtscts)
{
	configAssert('H');
}
END_TEST

START_TEST(test_config_fec_0)
{
	CFG_setBoolean("INPUT", "FEC", false);
	configAssert('I');
}
END_TEST

START_TEST(test_config_fec_1)
{
	CFG_setBoolean("INPUT", "FEC", true);
	configAssert('i');
}
END_TEST

START_TEST(test_config_modeac)
{
	configAssert('j');
}
END_TEST

START_TEST(test_config_R)
{
	configAssert('R');
}
END_TEST

START_TEST(test_config_Y)
{
	configAssert('Y');
}
END_TEST

START_TEST(test_config_fail)
{
	test.testAck = 3;
	COMP_initAll();
	COMP_startAll();
	ck_assert(test.init);
	INPUT_connect();
	ck_assert_uint_eq(test.write, 10);
	ck_assert_uint_eq(test.connect, 4);
}
END_TEST

START_TEST(test_decode_modeac)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC,
			UINT64_C(0x1234567890ab), -50, "ab", 2);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0x1234567890ab));
	ck_assert_uint_eq(decoded.payloadLen, 2);
	ck_assert_int_eq(decoded.siglevel, -50);
	ck_assert(!memcmp(decoded.payload, "ab", 2));
	ck_assert_uint_eq(frame.rawLen, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

START_TEST(test_decode_modesshort)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_S_SHORT,
			UINT64_C(0xcafebabedead), 127, "abcdefg", 7);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_SHORT);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xcafebabedead));
	ck_assert_uint_eq(decoded.payloadLen, 7);
	ck_assert_int_eq(decoded.siglevel, 127);
	ck_assert(!memcmp(decoded.payload, "abcdefg", 7));
	ck_assert_uint_eq(frame.rawLen, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

START_TEST(test_decode_modeslong)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_S_LONG,
			UINT64_C(0xdeadbeefbabe), 0, "abcdefghijklmn", 14);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xdeadbeefbabe));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, 0);
	ck_assert(!memcmp(decoded.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.rawLen, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

START_TEST(test_decode_status)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_STATUS,
			UINT64_C(0xdeadbeefbabe), 0, "abcdefghijklmn", 14);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_STATUS);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xdeadbeefbabe));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, 0);
	ck_assert(!memcmp(decoded.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.rawLen, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

START_TEST(test_decode_unknown)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = RC_INPUT_buildFrame(frm, '5', UINT64_C(0xcafebabedead), 0,
			"abcdefghijklmn", 14);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(!ret);
}
END_TEST

START_TEST(test_decode_unknown_next)
{
	uint8_t frm[46 * 2];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len1 = RC_INPUT_buildFrame(frm, '5', 0xcafeba, 0,
			"abcdefghijklmn", 14);
	size_t len2 = RC_INPUT_buildFrame(frm + len1,
			OPENSKY_FRAME_TYPE_MODE_AC, UINT64_C(0x1234567890ab), 0, "ab",
			2);
	buf.length = len1 + len2;
	test.buffers = &buf;
	test.nBuffers = 1;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0x1234567890ab));
	ck_assert_uint_eq(decoded.payloadLen, 2);
	ck_assert_int_eq(decoded.siglevel, 0);
	ck_assert(!memcmp(decoded.payload, "ab", 2));
	ck_assert_uint_eq(frame.rawLen, len2);
	ck_assert(!memcmp(frame.raw, frm + len1, len2));
}
END_TEST

START_TEST(test_decode_escape)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	const char msg[] =
			"\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a";
	size_t len = RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_S_LONG,
			UINT64_C(0x1a1a1a1a1a1a), 0x1a, msg, 14);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0x1a1a1a1a1a1a));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, 0x1a);
	ck_assert(!memcmp(decoded.payload, msg, 14));
	ck_assert_uint_eq(frame.rawLen, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

START_TEST(test_decode_unsynchronized_start)
{
	uint8_t frm[48] = { 'a', 'b' };
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = RC_INPUT_buildFrame(frm + 2, OPENSKY_FRAME_TYPE_MODE_AC,
			UINT64_C(0x1234567890ab), -50, "ab", 2);
	buf.length = len + 2;
	test.buffers = &buf;
	test.nBuffers = 1;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0x1234567890ab));
	ck_assert_uint_eq(decoded.payloadLen, 2);
	ck_assert_int_eq(decoded.siglevel, -50);
	ck_assert(!memcmp(decoded.payload, "ab", 2));
	ck_assert_uint_eq(frame.rawLen, len);
	ck_assert(!memcmp(frame.raw, frm + 2, len));

	struct RADARCAPE_Statistics stats;
	RADARCAPE_getStatistics(&stats);
	ck_assert_uint_eq(stats.outOfSync, 1);
}
END_TEST

START_TEST(test_decode_unsynchronized_type)
{
	uint8_t frm[46 + 1];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = 5;
	memcpy(frm, "\x1a\x1a" "ABC", 5);
	len = RC_INPUT_buildFrame(frm + len, OPENSKY_FRAME_TYPE_MODE_S_LONG,
			UINT64_C(0xcafebabedead), -128, "abcdefghijklmn", 14);
	buf.length = len + 5;
	test.buffers = &buf;
	test.nBuffers = 1;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xcafebabedead));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, -128);
	ck_assert(!memcmp(decoded.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.rawLen, len);
	ck_assert(!memcmp(frame.raw, frm + 5, len));
	struct RADARCAPE_Statistics stats;
	RADARCAPE_getStatistics(&stats);
	ck_assert_uint_eq(stats.outOfSync, 1);
}
END_TEST

START_TEST(test_decode_unsynchronized_header)
{
	uint8_t frm[46 + 4];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC,
			UINT64_C(0x1234567890ab), -50, "ab", 2);
	len = RC_INPUT_buildFrame(frm + 4, OPENSKY_FRAME_TYPE_MODE_S_LONG,
			UINT64_C(0xcafebabedead), -128, "abcdefghijklmn", 14);
	buf.length = len + 4;
	test.buffers = &buf;
	test.nBuffers = 1;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xcafebabedead));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, -128);
	ck_assert(!memcmp(decoded.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.rawLen, len);
	ck_assert(!memcmp(frame.raw, frm + 4, len));
	struct RADARCAPE_Statistics stats;
	RADARCAPE_getStatistics(&stats);
	ck_assert_uint_eq(stats.outOfSync, 1);
}
END_TEST

START_TEST(test_decode_unsynchronized_payload)
{
	uint8_t frm[46 + 9];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC,
			UINT64_C(0x1234567890ab), -50, "ab", 2);
	len = RC_INPUT_buildFrame(frm + 9, OPENSKY_FRAME_TYPE_MODE_S_LONG,
			UINT64_C(0xcafebabedead), -128, "abcdefghijklmn", 14);
	buf.length = len + 9;
	test.buffers = &buf;
	test.nBuffers = 1;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xcafebabedead));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, -128);
	ck_assert(!memcmp(decoded.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.rawLen, len);
	ck_assert(!memcmp(frame.raw, frm + 9, len));
	struct RADARCAPE_Statistics stats;
	RADARCAPE_getStatistics(&stats);
	ck_assert_uint_eq(stats.outOfSync, 1);
}
END_TEST

START_TEST(test_buffer_two_frames)
{
	uint8_t frm[46 * 2];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len1 = RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC,
			UINT64_C(0x1234567890ab), -50, "ab", 2);
	size_t len2 = RC_INPUT_buildFrame(frm + len1,
			OPENSKY_FRAME_TYPE_MODE_S_LONG, UINT64_C(0xcafebabedead), 127,
			"abcdefghijklmn", 14);
	buf.length = len1 + len2;
	test.buffers = &buf;
	test.nBuffers = 1;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0x1234567890ab));
	ck_assert_uint_eq(decoded.payloadLen, 2);
	ck_assert_int_eq(decoded.siglevel, -50);
	ck_assert(!memcmp(decoded.payload, "ab", 2));
	ck_assert_uint_eq(frame.rawLen, len1);
	ck_assert(!memcmp(frame.raw, frm, len1));

	ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xcafebabedead));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, 127);
	ck_assert(!memcmp(decoded.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.rawLen, len2);
	ck_assert(!memcmp(frame.raw, frm + len1, len2));
}
END_TEST

START_TEST(test_buffer_fail_start)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC,
			UINT64_C(0x1234567890ab), -50, "ab", 2);
	buf.length = 0;
	test.buffers = &buf;
	test.nBuffers = 1;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(!ret);
}
END_TEST

START_TEST(test_buffer_fail_type)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC,
			UINT64_C(0x1234567890ab), -50, "ab", 2);
	buf.length = 1;
	test.buffers = &buf;
	test.nBuffers = 1;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(!ret);
}
END_TEST

START_TEST(test_buffer_fail_header)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC,
			UINT64_C(0x1234567890ab), -50, "ab", 2);
	buf.length = 4;
	test.buffers = &buf;
	test.nBuffers = 1;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(!ret);
}
END_TEST

START_TEST(test_buffer_fail_payload)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC,
			UINT64_C(0x1234567890ab), -50, "ab", 2);
	buf.length = 10;
	test.buffers = &buf;
	test.nBuffers = 1;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(!ret);
}
END_TEST

START_TEST(test_buffer_fail_escape)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC,
			UINT64_C(0x1234567890ab), -50, "\x1a" "b", 2);
	buf.length = 9;
	test.buffers = &buf;
	test.nBuffers = 1;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(!ret);
}
END_TEST

START_TEST(test_buffer_end_start)
{
	uint8_t frm[46];
	uint8_t frm2[46];
	struct TEST_Buffer buf[2] = { { .payload = frm }, { .payload = frm2 } };
	size_t len1 = RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC,
			UINT64_C(0x1234567890ab), -50, "ab", 2);
	size_t len2 = RC_INPUT_buildFrame(frm2, OPENSKY_FRAME_TYPE_MODE_S_LONG,
			UINT64_C(0xcafebabedead), 26, "abcdefghijklmn", 14);
	buf[0].length = len1;
	buf[1].length = len2;
	test.buffers = buf;
	test.nBuffers = 2;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0x1234567890ab));
	ck_assert_uint_eq(decoded.payloadLen, 2);
	ck_assert_int_eq(decoded.siglevel, -50);
	ck_assert(!memcmp(decoded.payload, "ab", 2));
	ck_assert_uint_eq(frame.rawLen, len1);
	ck_assert(!memcmp(frame.raw, frm, len1));

	ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xcafebabedead));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, 26);
	ck_assert(!memcmp(decoded.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.rawLen, len2);
	ck_assert(!memcmp(frame.raw, frm2, len2));
}
END_TEST

START_TEST(test_buffer_end)
{
	uint8_t frm[46];
	struct TEST_Buffer buf[2];
	size_t len = RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC,
			UINT64_C(0x1234567890ab), -50, "ab", 2);
	buf[0].payload = frm;
	buf[0].length = _i;
	buf[1].payload = frm + _i;
	buf[1].length = len < _i ? 0 : len - _i;
	test.buffers = buf;
	test.nBuffers = len < _i ? 1 : 2;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0x1234567890ab));
	ck_assert_uint_eq(decoded.payloadLen, 2);
	ck_assert_int_eq(decoded.siglevel, -50);
	ck_assert(!memcmp(decoded.payload, "ab", 2));
	ck_assert_uint_eq(frame.rawLen, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

START_TEST(test_buffer_end_escape)
{
	uint8_t frm[46];
	struct TEST_Buffer buf[2];
	size_t len = RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC,
			UINT64_C(0x1234567890ab), -50, "\x1a" "b", 2);
	buf[0].payload = frm;
	buf[0].length = 10;
	buf[1].payload = frm + 10;
	buf[1].length = len - 10;
	test.buffers = buf;
	test.nBuffers = 2;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0x1234567890ab));
	ck_assert_uint_eq(decoded.payloadLen, 2);
	ck_assert_int_eq(decoded.siglevel, -50);
	ck_assert(!memcmp(decoded.payload, "\x1a" "b", 2));
	ck_assert_uint_eq(frame.rawLen, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

START_TEST(test_buffer_end_escape_fail)
{
	uint8_t frm[46];
	struct TEST_Buffer buf;
	RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC,
			UINT64_C(0x1234567890ab), -50, "\x1a" "b", 2);
	buf.payload = frm;
	buf.length = 10;
	test.buffers = &buf;
	test.nBuffers = 1;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(!ret);
}
END_TEST

START_TEST(test_synchronize_peek_unsync)
{
	uint8_t frm[46 + 4];
	struct TEST_Buffer buf = { .payload = frm };
	frm[0] = 'a';
	frm[1] = '\x1a';
	frm[2] = '\x1a';
	frm[3] = 'b';
	size_t len = RC_INPUT_buildFrame(frm + 4,
			OPENSKY_FRAME_TYPE_MODE_S_LONG, UINT64_C(0xcafebabedead), -128,
			"abcdefghijklmn", 14);
	buf.length = len + 4;
	test.buffers = &buf;
	test.nBuffers = 1;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xcafebabedead));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, -128);
	ck_assert(!memcmp(decoded.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.rawLen, len);
	ck_assert(!memcmp(frame.raw, frm + 4, len));
	struct RADARCAPE_Statistics stats;
	RADARCAPE_getStatistics(&stats);
	ck_assert_uint_eq(stats.outOfSync, 1);
}
END_TEST

START_TEST(test_synchronize_peek_unsync_at_end)
{
	uint8_t frm[2];
	uint8_t frm2[46 + 1];
	struct TEST_Buffer buf[2] = { { .payload = frm }, { .payload = frm2 } };
	frm[0] = 'a';
	frm[1] = '\x1a';
	size_t len = RC_INPUT_buildFrame(frm2 + 1,
			OPENSKY_FRAME_TYPE_MODE_S_LONG, UINT64_C(0xcafebabedead), -128,
			"abcdefghijklmn", 14);
	frm2[0] = '\x1a';
	buf[0].length = sizeof frm;
	buf[1].length = len + 1;
	test.buffers = buf;
	test.nBuffers = 2;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xcafebabedead));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, -128);
	ck_assert(!memcmp(decoded.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.rawLen, len);
	ck_assert(!memcmp(frame.raw, frm2 + 1, len));
	struct RADARCAPE_Statistics stats;
	RADARCAPE_getStatistics(&stats);
	ck_assert_uint_eq(stats.outOfSync, 1);
}
END_TEST

START_TEST(test_synchronize_peek_sync_at_end)
{
	uint8_t frm[2];
	uint8_t frm2[46];
	struct TEST_Buffer buf[2] = { { .payload = frm },
			{ .payload = frm2 + 1 } };
	frm[0] = 'a';
	frm[1] = '\x1a';
	size_t len = RC_INPUT_buildFrame(frm2, OPENSKY_FRAME_TYPE_MODE_S_LONG,
			UINT64_C(0xcafebabedead), -128, "abcdefghijklmn", 14);
	buf[0].length = sizeof frm;
	buf[1].length = len - 1;
	test.buffers = buf;
	test.nBuffers = 2;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xcafebabedead));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, -128);
	ck_assert(!memcmp(decoded.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.rawLen, len);
	ck_assert(!memcmp(frame.raw, frm2, len));
	struct RADARCAPE_Statistics stats;
	RADARCAPE_getStatistics(&stats);
	ck_assert_uint_eq(stats.outOfSync, 1);
}
END_TEST

START_TEST(test_synchronize_fail)
{
	char * frm = "a\x1a";
	struct TEST_Buffer buf = { .payload = (uint8_t*)frm, .length = 2 };
	test.buffers = &buf;
	test.nBuffers = 1;

	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = INPUT_getFrame(&frame, &decoded);
	ck_assert(!ret);
}
END_TEST

static Suite * RADARCAPE_suite()
{
	Suite * s = suite_create("Radarcape");

	TCase * tc = tcase_create("InitDestruct");
	tcase_add_checked_fixture(tc, setup, NULL);
	tcase_add_test(tc, test_init_destruct);
	suite_add_tcase(s, tc);

	tc = tcase_create("ConnectConfig");
	tcase_add_checked_fixture(tc, setup, NULL);
	tcase_add_test(tc, test_connect);
	tcase_add_test(tc, test_config_bin);
	tcase_add_test(tc, test_config_frameFilter_0);
	tcase_add_test(tc, test_config_frameFilter_1);
	tcase_add_test(tc, test_config_crc_0);
	tcase_add_test(tc, test_config_crc_1);
	tcase_add_test(tc, test_config_gps);
	tcase_add_test(tc, test_config_rtscts);
	tcase_add_test(tc, test_config_fec_0);
	tcase_add_test(tc, test_config_fec_1);
	tcase_add_test(tc, test_config_modeac);
	tcase_add_test(tc, test_config_R);
	tcase_add_test(tc, test_config_Y);
	tcase_add_test(tc, test_config_fail);
	suite_add_tcase(s, tc);

	tc = tcase_create("Decode");
	tcase_add_checked_fixture(tc, setup, NULL);
	tcase_add_test(tc, test_decode_modeac);
	tcase_add_test(tc, test_decode_modesshort);
	tcase_add_test(tc, test_decode_modeslong);
	tcase_add_test(tc, test_decode_status);
	tcase_add_test(tc, test_decode_unknown);
	tcase_add_test(tc, test_decode_unknown_next);
	tcase_add_test(tc, test_decode_escape);
	tcase_add_test(tc, test_decode_unsynchronized_start);
	tcase_add_test(tc, test_decode_unsynchronized_type);
	tcase_add_test(tc, test_decode_unsynchronized_header);
	tcase_add_test(tc, test_decode_unsynchronized_payload);
	suite_add_tcase(s, tc);

	tc = tcase_create("Synchronizer");
	tcase_add_checked_fixture(tc, setup, NULL);
	tcase_add_test(tc, test_synchronize_peek_unsync);
	tcase_add_test(tc, test_synchronize_peek_unsync_at_end);
	tcase_add_test(tc, test_synchronize_peek_sync_at_end);
	suite_add_tcase(s, tc);

	tc = tcase_create("Buffering");
	tcase_add_checked_fixture(tc, setup, NULL);
	tcase_add_test(tc, test_buffer_two_frames);
	tcase_add_test(tc, test_buffer_fail_start);
	tcase_add_test(tc, test_buffer_fail_type);
	tcase_add_test(tc, test_buffer_fail_header);
	tcase_add_test(tc, test_buffer_fail_payload);
	tcase_add_test(tc, test_buffer_fail_escape);
	tcase_add_test(tc, test_buffer_end_start);
	tcase_add_loop_test(tc, test_buffer_end, 1, 46);
	tcase_add_test(tc, test_buffer_end_escape);
	tcase_add_test(tc, test_buffer_end_escape_fail);
	tcase_add_test(tc, test_synchronize_fail);
	suite_add_tcase(s, tc);
	return s;
}

int main()
{
	SRunner * sr = srunner_create(RADARCAPE_suite());
	//srunner_set_fork_status(sr, CK_NOFORK);
	srunner_set_tap(sr, "-");
	srunner_run_all(sr, CK_NORMAL);
	srunner_free(sr);
	return EXIT_SUCCESS;
}
