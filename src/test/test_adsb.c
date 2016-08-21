/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <check.h>
#include <openskytypes.h>
#include <input_test.h>
#include <statistics.h>
#include <cfgfile.h>
#include <ctype.h>

struct CFG_Config CFG_config;
struct CFG_RECV * const cfg = &CFG_config.recv;

static void setup()
{
	test.testAck = -1;
}

START_TEST(test_init_destruct)
{
	INPUT_init();
	ck_assert(test.init);
	OPENSKY_destruct();
	ck_assert(test.destruct);
}
END_TEST

START_TEST(test_connect)
{
	INPUT_init();
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
	INPUT_init();
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
	cfg->modeSLongExtSquitter = true;
	configAssert('D');
}
END_TEST

START_TEST(test_config_frameFilter_1)
{
	cfg->modeSLongExtSquitter = false;
	configAssert('d');
}
END_TEST

START_TEST(test_config_crc_0)
{
	cfg->crc = false;
	configAssert('F');
}
END_TEST

START_TEST(test_config_crc_1)
{
	cfg->crc = true;
	configAssert('f');
}
END_TEST

START_TEST(test_config_gps_0)
{
	cfg->gps = false;
	configAssert('g');
}
END_TEST

START_TEST(test_config_gps_1)
{
	cfg->gps = true;
	configAssert('G');
}
END_TEST

START_TEST(test_config_rtscts_0)
{
	CFG_config.input.rtscts = false;
	configAssert('h');
}
END_TEST

START_TEST(test_config_rtscts_1)
{
	CFG_config.input.rtscts = true;
	configAssert('H');
}
END_TEST

START_TEST(test_config_fec_0)
{
	cfg->fec = false;
	configAssert('I');
}
END_TEST

START_TEST(test_config_fec_1)
{
	cfg->fec = true;
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
	INPUT_init();
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
	size_t len = INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC, UINT64_C(0x1234567890ab), -50, "ab",
		2);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0x1234567890ab));
	ck_assert_uint_eq(decoded.payloadLen, 2);
	ck_assert_int_eq(decoded.siglevel, -50);
	ck_assert(!memcmp(decoded.payload, "ab", 2));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

START_TEST(test_decode_modesshort)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_S_SHORT, UINT64_C(0xcafebabedead), 127,
		"abcdefg", 7);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_SHORT);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xcafebabedead));
	ck_assert_uint_eq(decoded.payloadLen, 7);
	ck_assert_int_eq(decoded.siglevel, 127);
	ck_assert(!memcmp(decoded.payload, "abcdefg", 7));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

START_TEST(test_decode_modeslong)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_S_LONG, UINT64_C(0xdeadbeefbabe), 0,
		"abcdefghijklmn", 14);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xdeadbeefbabe));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, 0);
	ck_assert(!memcmp(decoded.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

START_TEST(test_decode_status)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_STATUS, UINT64_C(0xdeadbeefbabe), 0,
		"abcdefghijklmn", 14);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_STATUS);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xdeadbeefbabe));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, 0);
	ck_assert(!memcmp(decoded.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

START_TEST(test_decode_unknown)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, '5', UINT64_C(0xcafebabedead), 0, "abcdefghijklmn", 14);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(!ret);
}
END_TEST

START_TEST(test_decode_unknown_next)
{
	uint8_t frm[46 * 2];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len1 = INPUT_buildFrame(frm, '5', 0xcafeba, 0, "abcdefghijklmn", 14);
	size_t len2 = INPUT_buildFrame(frm + len1, OPENSKY_FRAME_TYPE_MODE_AC, UINT64_C(0x1234567890ab), 0,
		"ab", 2);
	buf.length = len1 + len2;
	test.buffers = &buf;
	test.nBuffers = 1;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0x1234567890ab));
	ck_assert_uint_eq(decoded.payloadLen, 2);
	ck_assert_int_eq(decoded.siglevel, 0);
	ck_assert(!memcmp(decoded.payload, "ab", 2));
	ck_assert_uint_eq(frame.raw_len, len2);
	ck_assert(!memcmp(frame.raw, frm + len1, len2));
}
END_TEST

START_TEST(test_decode_escape)
{
	uint8_t frm[46];
		struct TEST_Buffer buf = { .payload = frm };
	const char msg[] =
		"\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a";
	size_t len = INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_S_LONG, UINT64_C(0x1a1a1a1a1a1a),
		0x1a, msg, 14);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0x1a1a1a1a1a1a));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, 0x1a);
	ck_assert(!memcmp(decoded.payload, msg, 14));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

START_TEST(test_decode_unsynchronized_start)
{
	uint8_t frm[48] = { 'a', 'b' };
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm + 2, OPENSKY_FRAME_TYPE_MODE_AC, UINT64_C(0x1234567890ab), -50,
		"ab", 2);
	buf.length = len + 2;
	test.buffers = &buf;
	test.nBuffers = 1;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0x1234567890ab));
	ck_assert_uint_eq(decoded.payloadLen, 2);
	ck_assert_int_eq(decoded.siglevel, -50);
	ck_assert(!memcmp(decoded.payload, "ab", 2));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm + 2, len));
	ck_assert_uint_eq(STAT_stats.RECV_outOfSync, 1);
}
END_TEST

START_TEST(test_decode_unsynchronized_type)
{
	uint8_t frm[46 + 1];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = 5;
	memcpy(frm, "\x1a\x1a" "ABC", 5);
	len = INPUT_buildFrame(frm + len, OPENSKY_FRAME_TYPE_MODE_S_LONG, UINT64_C(0xcafebabedead), -128,
		"abcdefghijklmn", 14);
	buf.length = len + 5;
	test.buffers = &buf;
	test.nBuffers = 1;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xcafebabedead));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, -128);
	ck_assert(!memcmp(decoded.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm + 5, len));
	ck_assert_uint_eq(STAT_stats.RECV_outOfSync, 1);
}
END_TEST

START_TEST(test_decode_unsynchronized_header)
{
	uint8_t frm[46 + 4];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC, UINT64_C(0x1234567890ab), -50,
		"ab", 2);
	len = INPUT_buildFrame(frm + 4, OPENSKY_FRAME_TYPE_MODE_S_LONG, UINT64_C(0xcafebabedead), -128,
		"abcdefghijklmn", 14);
	buf.length = len + 4;
	test.buffers = &buf;
	test.nBuffers = 1;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xcafebabedead));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, -128);
	ck_assert(!memcmp(decoded.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm + 4, len));
	ck_assert_uint_eq(STAT_stats.RECV_outOfSync, 1);
}
END_TEST

START_TEST(test_decode_unsynchronized_payload)
{
	uint8_t frm[46 + 9];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC, UINT64_C(0x1234567890ab), -50,
		"ab", 2);
	len = INPUT_buildFrame(frm + 9, OPENSKY_FRAME_TYPE_MODE_S_LONG, UINT64_C(0xcafebabedead), -128,
		"abcdefghijklmn", 14);
	buf.length = len + 9;
	test.buffers = &buf;
	test.nBuffers = 1;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xcafebabedead));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, -128);
	ck_assert(!memcmp(decoded.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm + 9, len));
	ck_assert_uint_eq(STAT_stats.RECV_outOfSync, 1);
}
END_TEST

START_TEST(test_buffer_two_frames)
{
	uint8_t frm[46 * 2];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len1 = INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC, UINT64_C(0x1234567890ab), -50,
		"ab", 2);
	size_t len2 = INPUT_buildFrame(frm + len1, OPENSKY_FRAME_TYPE_MODE_S_LONG, UINT64_C(0xcafebabedead),
		127, "abcdefghijklmn", 14);
	buf.length = len1 + len2;
	test.buffers = &buf;
	test.nBuffers = 1;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0x1234567890ab));
	ck_assert_uint_eq(decoded.payloadLen, 2);
	ck_assert_int_eq(decoded.siglevel, -50);
	ck_assert(!memcmp(decoded.payload, "ab", 2));
	ck_assert_uint_eq(frame.raw_len, len1);
	ck_assert(!memcmp(frame.raw, frm, len1));

	ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xcafebabedead));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, 127);
	ck_assert(!memcmp(decoded.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.raw_len, len2);
	ck_assert(!memcmp(frame.raw, frm + len1, len2));
}
END_TEST

START_TEST(test_buffer_fail_start)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC, UINT64_C(0x1234567890ab), -50, "ab", 2);
	buf.length = 0;
	test.buffers = &buf;
	test.nBuffers = 1;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(!ret);
}
END_TEST

START_TEST(test_buffer_fail_type)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC, UINT64_C(0x1234567890ab), -50, "ab", 2);
	buf.length = 1;
	test.buffers = &buf;
	test.nBuffers = 1;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(!ret);
}
END_TEST

START_TEST(test_buffer_fail_header)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC, UINT64_C(0x1234567890ab), -50, "ab", 2);
	buf.length = 4;
	test.buffers = &buf;
	test.nBuffers = 1;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(!ret);
}
END_TEST

START_TEST(test_buffer_fail_payload)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC, UINT64_C(0x1234567890ab), -50, "ab", 2);
	buf.length = 10;
	test.buffers = &buf;
	test.nBuffers = 1;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(!ret);
}
END_TEST

START_TEST(test_buffer_fail_escape)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC, UINT64_C(0x1234567890ab), -50, "\x1a" "b", 2);
	buf.length = 9;
	test.buffers = &buf;
	test.nBuffers = 1;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(!ret);
}
END_TEST

START_TEST(test_buffer_end_start)
{
	uint8_t frm[46];
	uint8_t frm2[46];
	struct TEST_Buffer buf[2] = { { .payload = frm }, { .payload = frm2 } };
	size_t len1 = INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC, UINT64_C(0x1234567890ab), -50, "ab",
		2);
	size_t len2 = INPUT_buildFrame(frm2, OPENSKY_FRAME_TYPE_MODE_S_LONG, UINT64_C(0xcafebabedead), 26,
		"abcdefghijklmn", 14);
	buf[0].length = len1;
	buf[1].length = len2;
	test.buffers = buf;
	test.nBuffers = 2;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0x1234567890ab));
	ck_assert_uint_eq(decoded.payloadLen, 2);
	ck_assert_int_eq(decoded.siglevel, -50);
	ck_assert(!memcmp(decoded.payload, "ab", 2));
	ck_assert_uint_eq(frame.raw_len, len1);
	ck_assert(!memcmp(frame.raw, frm, len1));

	ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xcafebabedead));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, 26);
	ck_assert(!memcmp(decoded.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.raw_len, len2);
	ck_assert(!memcmp(frame.raw, frm2, len2));
}
END_TEST

START_TEST(test_buffer_end)
{
	uint8_t frm[46];
	struct TEST_Buffer buf[2];
	size_t len = INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC, UINT64_C(0x1234567890ab), -50, "ab",
		2);
	buf[0].payload = frm;
	buf[0].length = _i;
	buf[1].payload = frm + _i;
	buf[1].length = len < _i ? 0 : len - _i;
	test.buffers = buf;
	test.nBuffers = len < _i ? 1 : 2;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0x1234567890ab));
	ck_assert_uint_eq(decoded.payloadLen, 2);
	ck_assert_int_eq(decoded.siglevel, -50);
	ck_assert(!memcmp(decoded.payload, "ab", 2));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

START_TEST(test_buffer_end_escape)
{
	uint8_t frm[46];
	struct TEST_Buffer buf[2];
	size_t len = INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC, UINT64_C(0x1234567890ab), -50,
		"\x1a" "b", 2);
	buf[0].payload = frm;
	buf[0].length = 10;
	buf[1].payload = frm + 10;
	buf[1].length = len - 10;
	test.buffers = buf;
	test.nBuffers = 2;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0x1234567890ab));
	ck_assert_uint_eq(decoded.payloadLen, 2);
	ck_assert_int_eq(decoded.siglevel, -50);
	ck_assert(!memcmp(decoded.payload, "\x1a" "b", 2));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

START_TEST(test_buffer_end_escape_fail)
{
	uint8_t frm[46];
	struct TEST_Buffer buf;
	INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC, UINT64_C(0x1234567890ab), -50,
		"\x1a" "b", 2);
	buf.payload = frm;
	buf.length = 10;
	test.buffers = &buf;
	test.nBuffers = 1;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
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
	size_t len = INPUT_buildFrame(frm + 4, OPENSKY_FRAME_TYPE_MODE_S_LONG, UINT64_C(0xcafebabedead), -128,
		"abcdefghijklmn", 14);
	buf.length = len + 4;
	test.buffers = &buf;
	test.nBuffers = 1;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xcafebabedead));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, -128);
	ck_assert(!memcmp(decoded.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm + 4, len));
	ck_assert_uint_eq(STAT_stats.RECV_outOfSync, 1);
}
END_TEST

START_TEST(test_synchronize_peek_unsync_at_end)
{
	uint8_t frm[2];
	uint8_t frm2[46 + 1];
	struct TEST_Buffer buf[2] = { { .payload = frm }, { .payload = frm2 } };
	frm[0] = 'a';
	frm[1] = '\x1a';
	size_t len = INPUT_buildFrame(frm2 + 1, OPENSKY_FRAME_TYPE_MODE_S_LONG, UINT64_C(0xcafebabedead), -128,
		"abcdefghijklmn", 14);
	frm2[0] = '\x1a';
	buf[0].length = sizeof frm;
	buf[1].length = len + 1;
	test.buffers = buf;
	test.nBuffers = 2;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xcafebabedead));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, -128);
	ck_assert(!memcmp(decoded.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm2 + 1, len));
	ck_assert_uint_eq(STAT_stats.RECV_outOfSync, 1);
}
END_TEST

START_TEST(test_synchronize_peek_sync_at_end)
{
	uint8_t frm[2];
	uint8_t frm2[46];
	struct TEST_Buffer buf[2] = { { .payload = frm }, { .payload = frm2 + 1} };
	frm[0] = 'a';
	frm[1] = '\x1a';
	size_t len = INPUT_buildFrame(frm2, OPENSKY_FRAME_TYPE_MODE_S_LONG, UINT64_C(0xcafebabedead), -128,
		"abcdefghijklmn", 14);
	buf[0].length = sizeof frm;
	buf[1].length = len - 1;
	test.buffers = buf;
	test.nBuffers = 2;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(ret);
	ck_assert_uint_eq(decoded.frameType, OPENSKY_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(decoded.mlat, UINT64_C(0xcafebabedead));
	ck_assert_uint_eq(decoded.payloadLen, 14);
	ck_assert_int_eq(decoded.siglevel, -128);
	ck_assert(!memcmp(decoded.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm2, len));
	ck_assert_uint_eq(STAT_stats.RECV_outOfSync, 1);
}
END_TEST

START_TEST(test_synchronize_fail)
{
	char * frm = "a\x1a";
	struct TEST_Buffer buf = { .payload = (uint8_t*)frm, .length = 2 };
	test.buffers = &buf;
	test.nBuffers = 1;

	INPUT_init(&cfg);
	INPUT_connect();

	struct OPENSKY_RawFrame frame;
	struct OPENSKY_DecodedFrame decoded;
	bool ret = OPENSKY_getFrame(&frame, &decoded);
	ck_assert(!ret);
}
END_TEST

static Suite * ADSB_suite()
{
	Suite * s = suite_create("ADSB");

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
	tcase_add_test(tc, test_config_gps_0);
	tcase_add_test(tc, test_config_gps_1);
	tcase_add_test(tc, test_config_rtscts_0);
	tcase_add_test(tc, test_config_rtscts_1);
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
	SRunner * sr = srunner_create(ADSB_suite());
	//srunner_set_fork_status(sr, CK_NOFORK);
	srunner_set_tap(sr, "-");
	srunner_run_all(sr, CK_NORMAL);
	srunner_free(sr);
	return EXIT_SUCCESS;
}
