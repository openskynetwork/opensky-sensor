#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <check.h>
#include <adsb.h>
#include <input_test.h>
#include <statistics.h>
#include <cfgfile.h>
#include <ctype.h>

struct CFG_Config CFG_config;

static struct ADSB_CONFIG cfg;

static void setup()
{
	cfg.frameFilter = false;
	cfg.crc = false;
	cfg.timestampGPS = false;
	cfg.rtscts = false;
	cfg.fec = false;
	cfg.modeAC = false;

	test.testAck = -1;
}

START_TEST(test_init_destruct)
{
	ADSB_init(&cfg);
	ck_assert(test.init);
	ADSB_destruct();
	ck_assert(test.destruct);
}
END_TEST

START_TEST(test_connect)
{
	ADSB_init(NULL);
	ck_assert(test.init);
	ADSB_connect();
	ck_assert_uint_eq(test.write, 0);
}
END_TEST

static void configAssert(char testC)
{
	char lower = tolower(testC);
	bool expect = testC == lower ? false : true;

	test.params[lower - 'c'] = !expect;
	ADSB_init(&cfg);
	ck_assert(test.init);
	ADSB_connect();
	ck_assert_uint_eq(test.write, 8);
	ck_assert(!!test.params[lower - 'c'] == !!expect);
}

START_TEST(test_config_bin)
{
	configAssert('C');
}
END_TEST

START_TEST(test_config_frameFilter_0)
{
	cfg.frameFilter = false;
	configAssert('d');
}
END_TEST

START_TEST(test_config_frameFilter_1)
{
	cfg.frameFilter = true;
	configAssert('D');
}
END_TEST

START_TEST(test_config_crc_0)
{
	cfg.crc = false;
	configAssert('F');
}
END_TEST

START_TEST(test_config_crc_1)
{
	cfg.crc = true;
	configAssert('f');
}
END_TEST

START_TEST(test_config_gps_0)
{
	cfg.timestampGPS = false;
	configAssert('g');
}
END_TEST

START_TEST(test_config_gps_1)
{
	cfg.timestampGPS = true;
	configAssert('G');
}
END_TEST

START_TEST(test_config_rtscts_0)
{
	cfg.rtscts = false;
	configAssert('h');
}
END_TEST

START_TEST(test_config_rtscts_1)
{
	cfg.rtscts = true;
	configAssert('H');
}
END_TEST

START_TEST(test_config_fec_0)
{
	cfg.fec = false;
	configAssert('I');
}
END_TEST

START_TEST(test_config_fec_1)
{
	cfg.fec = true;
	configAssert('i');
}
END_TEST

START_TEST(test_config_modeac_0)
{
	cfg.modeAC = false;
	configAssert('j');
}
END_TEST

START_TEST(test_config_modeac_1)
{
	cfg.modeAC = true;
	configAssert('J');
}
END_TEST

START_TEST(test_config_fail)
{
	test.testAck = 3;
	ADSB_init(&cfg);
	ck_assert(test.init);
	ADSB_connect();
	ck_assert_uint_eq(test.write, 8);
	ck_assert_uint_eq(test.connect, 4);
}
END_TEST

START_TEST(test_decode_modeac)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_AC, 0x123456, -50, "ab",
		2);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, ADSB_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(frame.mlat, 0x123456);
	ck_assert_uint_eq(frame.payloadLen, 2);
	ck_assert_int_eq(frame.siglevel, -50);
	ck_assert(!memcmp(frame.payload, "ab", 2));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

START_TEST(test_decode_modesshort)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_S_SHORT, 0x234567, 127,
		"abcdefg", 7);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, ADSB_FRAME_TYPE_MODE_S_SHORT);
	ck_assert_uint_eq(frame.mlat, 0x234567);
	ck_assert_uint_eq(frame.payloadLen, 7);
	ck_assert_int_eq(frame.siglevel, 127);
	ck_assert(!memcmp(frame.payload, "abcdefg", 7));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

START_TEST(test_decode_modeslong)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_S_LONG, 0xdeadbe, 0,
		"abcdefghijklmn", 14);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, ADSB_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(frame.mlat, 0xdeadbe);
	ck_assert_uint_eq(frame.payloadLen, 14);
	ck_assert_int_eq(frame.siglevel, 0);
	ck_assert(!memcmp(frame.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

START_TEST(test_decode_status)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_STATUS, 0xdeadbe, 0,
		"abcdefghijklmn", 14);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, ADSB_FRAME_TYPE_STATUS);
	ck_assert_uint_eq(frame.mlat, 0xdeadbe);
	ck_assert_uint_eq(frame.payloadLen, 14);
	ck_assert_int_eq(frame.siglevel, 0);
	ck_assert(!memcmp(frame.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

START_TEST(test_decode_unknown)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, '5', 0xcafeba, 0, "abcdefghijklmn", 14);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(!ret);
}
END_TEST

START_TEST(test_decode_unknown_next)
{
	uint8_t frm[46 * 2];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len1 = INPUT_buildFrame(frm, '5', 0xcafeba, 0, "abcdefghijklmn", 14);
	size_t len2 = INPUT_buildFrame(frm + len1, ADSB_FRAME_TYPE_MODE_AC, 0x123456, 0,
		"ab", 2);
	buf.length = len1 + len2;
	test.buffers = &buf;
	test.nBuffers = 1;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, ADSB_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(frame.mlat, 0x123456);
	ck_assert_uint_eq(frame.payloadLen, 2);
	ck_assert_int_eq(frame.siglevel, 0);
	ck_assert(!memcmp(frame.payload, "ab", 2));
	ck_assert_uint_eq(frame.raw_len, len2);
	ck_assert(!memcmp(frame.raw, frm + len1, len2));
}
END_TEST

START_TEST(test_decode_escape)
{
	uint8_t frm[46];
		struct TEST_Buffer buf = { .payload = frm };
	const uint8_t msg[] =
		"\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a";
	size_t len = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_S_LONG, 0x1a1a1a,
		0x1a, msg, 14);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, ADSB_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(frame.mlat, 0x1a1a1a);
	ck_assert_uint_eq(frame.payloadLen, 14);
	ck_assert_int_eq(frame.siglevel, 0x1a);
	ck_assert(!memcmp(frame.payload, msg, 14));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

START_TEST(test_decode_unsynchronized_start)
{
	uint8_t frm[48] = { 'a', 'b' };
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm + 2, ADSB_FRAME_TYPE_MODE_AC, 0x123456, -50,
		"ab", 2);
	buf.length = len + 2;
	test.buffers = &buf;
	test.nBuffers = 1;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, ADSB_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(frame.mlat, 0x123456);
	ck_assert_uint_eq(frame.payloadLen, 2);
	ck_assert_int_eq(frame.siglevel, -50);
	ck_assert(!memcmp(frame.payload, "ab", 2));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm + 2, len));
	ck_assert_uint_eq(STAT_stats.ADSB_outOfSync, 1);
}
END_TEST

START_TEST(test_decode_unsynchronized_type)
{
	uint8_t frm[46 + 1];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_AC, 0x123456, -50,
		"ab", 2);
	len = INPUT_buildFrame(frm + 1, ADSB_FRAME_TYPE_MODE_S_LONG, 0xabcdef, -128,
		"abcdefghijklmn", 14);
	buf.length = len + 1;
	test.buffers = &buf;
	test.nBuffers = 1;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, ADSB_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(frame.mlat, 0xabcdef);
	ck_assert_uint_eq(frame.payloadLen, 14);
	ck_assert_int_eq(frame.siglevel, -128);
	ck_assert(!memcmp(frame.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm + 1, len));
	ck_assert_uint_eq(STAT_stats.ADSB_outOfSync, 1);
}
END_TEST

START_TEST(test_decode_unsynchronized_header)
{
	uint8_t frm[46 + 4];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_AC, 0x123456, -50,
		"ab", 2);
	len = INPUT_buildFrame(frm + 4, ADSB_FRAME_TYPE_MODE_S_LONG, 0xabcdef, -128,
		"abcdefghijklmn", 14);
	buf.length = len + 4;
	test.buffers = &buf;
	test.nBuffers = 1;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, ADSB_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(frame.mlat, 0xabcdef);
	ck_assert_uint_eq(frame.payloadLen, 14);
	ck_assert_int_eq(frame.siglevel, -128);
	ck_assert(!memcmp(frame.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm + 4, len));
	ck_assert_uint_eq(STAT_stats.ADSB_outOfSync, 1);
}
END_TEST

START_TEST(test_decode_unsynchronized_payload)
{
	uint8_t frm[46 + 8];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_AC, 0x123456, -50,
		"ab", 2);
	len = INPUT_buildFrame(frm + 8, ADSB_FRAME_TYPE_MODE_S_LONG, 0xabcdef, -128,
		"abcdefghijklmn", 14);
	buf.length = len + 8;
	test.buffers = &buf;
	test.nBuffers = 1;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, ADSB_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(frame.mlat, 0xabcdef);
	ck_assert_uint_eq(frame.payloadLen, 14);
	ck_assert_int_eq(frame.siglevel, -128);
	ck_assert(!memcmp(frame.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm + 8, len));
	ck_assert_uint_eq(STAT_stats.ADSB_outOfSync, 1);
}
END_TEST

START_TEST(test_buffer_two_frames)
{
	uint8_t frm[46 * 2];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len1 = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_AC, 0x123456, -50,
		"ab", 2);
	size_t len2 = INPUT_buildFrame(frm + len1, ADSB_FRAME_TYPE_MODE_S_LONG, 0xabcdef,
		127, "abcdefghijklmn", 14);
	buf.length = len1 + len2;
	test.buffers = &buf;
	test.nBuffers = 1;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, ADSB_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(frame.mlat, 0x123456);
	ck_assert_uint_eq(frame.payloadLen, 2);
	ck_assert_int_eq(frame.siglevel, -50);
	ck_assert(!memcmp(frame.payload, "ab", 2));
	ck_assert_uint_eq(frame.raw_len, len1);
	ck_assert(!memcmp(frame.raw, frm, len1));

	ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, ADSB_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(frame.mlat, 0xabcdef);
	ck_assert_uint_eq(frame.payloadLen, 14);
	ck_assert_int_eq(frame.siglevel, 127);
	ck_assert(!memcmp(frame.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.raw_len, len2);
	ck_assert(!memcmp(frame.raw, frm + len1, len2));
}
END_TEST

START_TEST(test_buffer_fail_start)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_AC, 0x123456, -50, "ab",
		2);
	buf.length = 0;
	test.buffers = &buf;
	test.nBuffers = 1;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(!ret);
}
END_TEST

START_TEST(test_buffer_fail_type)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_AC, 0x123456, -50, "ab",
		2);
	buf.length = 1;
	test.buffers = &buf;
	test.nBuffers = 1;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(!ret);
}
END_TEST

START_TEST(test_buffer_fail_header)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_AC, 0x123456, -50, "ab",
		2);
	buf.length = 4;
	test.buffers = &buf;
	test.nBuffers = 1;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(!ret);
}
END_TEST

START_TEST(test_buffer_fail_payload)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_AC, 0x123456, -50, "ab",
		2);
	buf.length = 10;
	test.buffers = &buf;
	test.nBuffers = 1;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(!ret);
}
END_TEST

START_TEST(test_buffer_fail_escape)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_AC, 0x123456, -50,
		"\x1a" "b", 2);
	buf.length = 9;
	test.buffers = &buf;
	test.nBuffers = 1;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(!ret);
}
END_TEST

START_TEST(test_buffer_end_start)
{
	uint8_t frm[46];
	uint8_t frm2[46];
	struct TEST_Buffer buf[2] = { { .payload = frm }, { .payload = frm2 } };
	size_t len1 = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_AC, 0x123456, -50, "ab",
		2);
	size_t len2 = INPUT_buildFrame(frm2, ADSB_FRAME_TYPE_MODE_S_LONG, 0xabcdef, 26,
		"abcdefghijklmn", 14);
	buf[0].length = len1;
	buf[1].length = len2;
	test.buffers = buf;
	test.nBuffers = 2;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, ADSB_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(frame.mlat, 0x123456);
	ck_assert_uint_eq(frame.payloadLen, 2);
	ck_assert_int_eq(frame.siglevel, -50);
	ck_assert(!memcmp(frame.payload, "ab", 2));
	ck_assert_uint_eq(frame.raw_len, len1);
	ck_assert(!memcmp(frame.raw, frm, len1));

	ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, ADSB_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(frame.mlat, 0xabcdef);
	ck_assert_uint_eq(frame.payloadLen, 14);
	ck_assert_int_eq(frame.siglevel, 26);
	ck_assert(!memcmp(frame.payload, "abcdefghijklmn", 14));
	ck_assert_uint_eq(frame.raw_len, len2);
	ck_assert(!memcmp(frame.raw, frm2, len2));
}
END_TEST

START_TEST(test_buffer_end)
{
	uint8_t frm[46];
	struct TEST_Buffer buf[2];
	size_t len = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_AC, 0x123456, -50, "ab",
		2);
	buf[0].payload = frm;
	buf[0].length = _i;
	buf[1].payload = frm + _i;
	buf[1].length = len < _i ? 0 : len - _i;
	test.buffers = buf;
	test.nBuffers = len < _i ? 1 : 2;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, ADSB_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(frame.mlat, 0x123456);
	ck_assert_uint_eq(frame.payloadLen, 2);
	ck_assert_int_eq(frame.siglevel, -50);
	ck_assert(!memcmp(frame.payload, "ab", 2));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

START_TEST(test_buffer_end_escape)
{
	uint8_t frm[46];
	struct TEST_Buffer buf[2];
	size_t len = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_AC, 0x123456, -50,
		"\x1a" "b", 2);
	buf[0].payload = frm;
	buf[0].length = 9;
	buf[1].payload = frm + 9;
	buf[1].length = len - 9;
	test.buffers = buf;
	test.nBuffers = 2;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, ADSB_FRAME_TYPE_MODE_AC);
	ck_assert_uint_eq(frame.mlat, 0x123456);
	ck_assert_uint_eq(frame.payloadLen, 2);
	ck_assert_int_eq(frame.siglevel, -50);
	ck_assert(!memcmp(frame.payload, "\x1a" "b", 2));
	ck_assert_uint_eq(frame.raw_len, len);
	ck_assert(!memcmp(frame.raw, frm, len));
}
END_TEST

static Suite * adsb_suite()
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
	tcase_add_test(tc, test_config_modeac_0);
	tcase_add_test(tc, test_config_modeac_1);
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
	suite_add_tcase(s, tc);
	return s;
}

int main()
{
	SRunner * sr = srunner_create(adsb_suite());
	srunner_set_tap(sr, "-");
	srunner_run_all(sr, CK_NORMAL);
	srunner_free(sr);
	return EXIT_SUCCESS;
}
