#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <check.h>
#include <adsb.h>
#include <input_test.h>
#include <statistics.h>
#include <cfgfile.h>
#include <signal.h>
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

static size_t buildFrame(uint8_t * buf, enum ADSB_FRAME_TYPE type,
	uint64_t mlat, int8_t siglevel, const uint8_t * payload, size_t payloadLen)
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

START_TEST(test_decode_modeac)
{
	uint8_t buf[46];
	size_t len = buildFrame(buf, ADSB_FRAME_TYPE_MODE_AC, 0x123456, -50, "ab",
		2);
	test.frmMsg = buf;
	test.frmMsgLen = len;

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
	ck_assert(!memcmp(frame.raw, buf, len));
}
END_TEST

START_TEST(test_decode_modesshort)
{
	uint8_t buf[46];
	size_t len = buildFrame(buf, ADSB_FRAME_TYPE_MODE_S_SHORT, 0x234567, 127,
		"abcdefg", 7);
	test.frmMsg = buf;
	test.frmMsgLen = len;

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
	ck_assert(!memcmp(frame.raw, buf, len));
}
END_TEST

START_TEST(test_decode_modeslong)
{
	uint8_t buf[46];
	size_t len = buildFrame(buf, ADSB_FRAME_TYPE_MODE_S_LONG, 0xdeadbe, 0,
		"abcdefghijklmn", 14);
	test.frmMsg = buf;
	test.frmMsgLen = len;

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
	ck_assert(!memcmp(frame.raw, buf, len));
}
END_TEST

START_TEST(test_decode_status)
{
	uint8_t buf[46];
	size_t len = buildFrame(buf, ADSB_FRAME_TYPE_STATUS, 0xdeadbe, 0,
		"abcdefghijklmn", 14);
	test.frmMsg = buf;
	test.frmMsgLen = len;

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
	ck_assert(!memcmp(frame.raw, buf, len));
}
END_TEST

START_TEST(test_decode_unknown)
{
	uint8_t buf[46];
	size_t len = buildFrame(buf, '5', 0xcafeba, 0, "abcdefghijklmn", 14);
	test.frmMsg = buf;
	test.frmMsgLen = len;

	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(!ret);
}
END_TEST

START_TEST(test_decode_escape)
{
	uint8_t buf[46];
	const uint8_t msg[] =
		"\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a\x1a";
	size_t len = buildFrame(buf, ADSB_FRAME_TYPE_MODE_S_LONG, 0x1a1a1a,
		0x1a, msg, 14);
	test.frmMsg = buf;
	test.frmMsgLen = len;

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
	ck_assert(!memcmp(frame.raw, buf, len));
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
	tcase_add_test(tc, test_decode_escape);
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
