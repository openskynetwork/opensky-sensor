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

START_TEST(test_decode_modeac)
{
	test.frmMlat = 0x123456;
	test.frmType = ADSB_FRAME_TYPE_MODE_AC;
	test.frmMsgLen = 2;
	test.frmMsg = "ab";
	test.frmRaw = false;
	test.frmSigLevel = -50;
	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, test.frmType);
	ck_assert_uint_eq(frame.mlat, test.frmMlat);
	ck_assert_uint_eq(frame.payloadLen, test.frmMsgLen);
	ck_assert_int_eq(frame.siglevel, test.frmSigLevel);
	ck_assert(!memcmp(frame.payload, test.frmMsg, test.frmMsgLen));
	ck_assert_uint_eq(frame.raw_len, test.rawLen);
	ck_assert(!memcmp(frame.raw, test.raw, test.rawLen));
}
END_TEST

START_TEST(test_decode_modesshort)
{
	test.frmMlat = 0x234567;
	test.frmType = ADSB_FRAME_TYPE_MODE_S_SHORT;
	test.frmMsgLen = 7;
	test.frmMsg = "abcdefg";
	test.frmRaw = false;
	test.frmSigLevel = 126;
	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, test.frmType);
	ck_assert_uint_eq(frame.mlat, test.frmMlat);
	ck_assert_uint_eq(frame.payloadLen, test.frmMsgLen);
	ck_assert_int_eq(frame.siglevel, test.frmSigLevel);
	ck_assert(!memcmp(frame.payload, test.frmMsg, test.frmMsgLen));
	ck_assert_uint_eq(frame.raw_len, test.rawLen);
	ck_assert(!memcmp(frame.raw, test.raw, test.rawLen));
}
END_TEST

START_TEST(test_decode_modeslong)
{
	test.frmMlat = 0xdeadbee;
	test.frmType = ADSB_FRAME_TYPE_MODE_S_LONG;
	test.frmMsgLen = 14;
	test.frmMsg = "abcdefghijklmn";
	test.frmRaw = false;
	test.frmSigLevel = 0;
	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, test.frmType);
	ck_assert_uint_eq(frame.mlat, test.frmMlat);
	ck_assert_uint_eq(frame.payloadLen, test.frmMsgLen);
	ck_assert_int_eq(frame.siglevel, test.frmSigLevel);
	ck_assert(!memcmp(frame.payload, test.frmMsg, test.frmMsgLen));
	ck_assert_uint_eq(frame.raw_len, test.rawLen);
	ck_assert(!memcmp(frame.raw, test.raw, test.rawLen));
}
END_TEST

START_TEST(test_decode_status)
{
	test.frmMlat = 0xdeadbee;
	test.frmType = ADSB_FRAME_TYPE_STATUS;
	test.frmMsgLen = 14;
	test.frmMsg = "abcdefghijklmn";
	test.frmRaw = false;
	test.frmSigLevel = 0;
	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, test.frmType);
	ck_assert_uint_eq(frame.mlat, test.frmMlat);
	ck_assert_uint_eq(frame.payloadLen, test.frmMsgLen);
	ck_assert_int_eq(frame.siglevel, test.frmSigLevel);
	ck_assert(!memcmp(frame.payload, test.frmMsg, test.frmMsgLen));
	ck_assert_uint_eq(frame.raw_len, test.rawLen);
	ck_assert(!memcmp(frame.raw, test.raw, test.rawLen));
}
END_TEST

/*START_TEST(test_decode_unknown)
{
	test.frmMlat = 0xdeadbee;
	test.frmType = '5';
	test.frmMsgLen = 14;
	test.frmMsg = "abcdefghijklmn";
	test.frmRaw = false;
	test.frmSigLevel = 0;
	ADSB_init(&cfg);
	ADSB_connect();

	struct ADSB_Frame frame;
	bool ret = ADSB_getFrame(&frame);
	ck_assert(ret);
	ck_assert_uint_eq(frame.frameType, test.frmType);
	ck_assert_uint_eq(frame.mlat, test.frmMlat);
	ck_assert_uint_eq(frame.payloadLen, test.frmMsgLen);
	ck_assert_int_eq(frame.siglevel, test.frmSigLevel);
	ck_assert(!memcmp(frame.payload, test.frmMsg, test.frmMsgLen));
	ck_assert_uint_eq(frame.raw_len, test.rawLen);
	ck_assert(!memcmp(frame.raw, test.raw, test.rawLen));
}
END_TEST*/

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
	//tcase_add_test(tc, test_decode_unknown);
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
