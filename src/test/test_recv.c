#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <check.h>
#include <adsb.h>
#include <recv.h>
#include <buffer.h>
#include <input_test.h>
#include <statistics.h>
#include <cfgfile.h>
#include <signal.h>
#include <ctype.h>

struct CFG_Config CFG_config;
static struct CFG_RECV * const cfg = &CFG_config.recv;

static void setup()
{
	struct CFG_BUF * const bufCfg = &CFG_config.buf;
	bufCfg->gcEnabled = false;
	bufCfg->gcInterval = 120;
	bufCfg->gcLevel = 10;
	bufCfg->history = false;
	bufCfg->statBacklog = 10;
	bufCfg->dynBacklog = 100;
	bufCfg->dynIncrement = 10;

	cfg->crc = true;
	cfg->fec = true;
	cfg->gps = true;
	cfg->modeSLongExtSquitter = false;
	cfg->syncFilter = false;
	COMP_register(&BUF_comp, NULL);
	COMP_register(&RECV_comp, NULL);
	COMP_setSilent(true);
}

START_TEST(test_recv_frame)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_S_LONG, 0xdeadbe, 0,
		"abcdefghijklmn", 14);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;
	test.noRet = true;

	COMP_initAll();
	COMP_startAll();

	const struct ADSB_Frame * frame = BUF_getFrameTimeout(250);
	ck_assert_ptr_ne(frame, NULL);
	ck_assert_uint_eq(frame->frameType, ADSB_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(frame->mlat, 0xdeadbe);
	ck_assert_uint_eq(STAT_stats.ADSB_frameType[ADSB_FRAME_TYPE_MODE_S_LONG],
		1);
	ck_assert_uint_eq(STAT_stats.ADSB_longType[('a' >> 3) & 0x1f], 1);
	ck_assert_uint_eq(STAT_stats.ADSB_framesUnsynchronized, 1);

	COMP_stopAll();
	COMP_destructAll();
}
END_TEST

START_TEST(test_filter_unsynchronized)
{
	cfg->syncFilter = true;

	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_S_LONG, 0xdeadbe, 0,
		"abcdefghijklmn", 14);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;
	test.noRet = true;

	COMP_initAll();
	COMP_startAll();

	const struct ADSB_Frame * frame = BUF_getFrameTimeout(250);
	ck_assert_ptr_eq(frame, NULL);
	ck_assert_uint_eq(STAT_stats.ADSB_framesUnsynchronized, 1);

	COMP_stopAll();
	COMP_destructAll();
}
END_TEST

START_TEST(test_filter_synchronized)
{
	cfg->syncFilter = true;

	uint8_t frm[46], frm2[46];
	struct TEST_Buffer buf[2] = { { .payload = frm }, { .payload = frm2  } };
	size_t len1 = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_STATUS, 0x123456, 0,
		"abcdefghijklmn", 14);
	buf[0].length = len1;
	size_t len2 = INPUT_buildFrame(frm2, ADSB_FRAME_TYPE_MODE_S_LONG, 0xdeadbe,
		0, "abcdefghijklmn", 14);
	buf[1].length = len2;
	test.buffers = buf;
	test.nBuffers = 2;
	test.noRet = true;

	COMP_initAll();
	COMP_startAll();

	const struct ADSB_Frame * frame = BUF_getFrameTimeout(250);
	ck_assert_ptr_ne(frame, NULL);
	ck_assert_uint_eq(frame->frameType, ADSB_FRAME_TYPE_MODE_S_LONG);
	ck_assert_uint_eq(frame->mlat, 0xdeadbe);
	ck_assert_uint_eq(STAT_stats.ADSB_framesUnsynchronized, 0);

	COMP_stopAll();
	COMP_destructAll();
}
END_TEST

START_TEST(test_filter_modeac)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_AC, 0xdeadbe, 0,
		"ab", 2);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;
	test.noRet = true;

	COMP_initAll();
	COMP_startAll();

	const struct ADSB_Frame * frame = BUF_getFrameTimeout(250);
	ck_assert_ptr_eq(frame, NULL);
	ck_assert_uint_eq(STAT_stats.ADSB_framesFiltered, 1);

	COMP_stopAll();
	COMP_destructAll();
}
END_TEST

START_TEST(test_filter_type)
{
	cfg->modeSLongExtSquitter = true;

	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_S_LONG, 0xdeadbe, 0,
		"abcdefghijklmn", 14);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;
	test.noRet = true;

	COMP_initAll();
	COMP_startAll();

	const struct ADSB_Frame * frame = BUF_getFrameTimeout(250);
	ck_assert_ptr_eq(frame, NULL);
	ck_assert_uint_eq(STAT_stats.ADSB_framesFiltered, 1);
	ck_assert_uint_eq(STAT_stats.ADSB_framesFilteredLong, 1);

	COMP_stopAll();
	COMP_destructAll();
}
END_TEST

START_TEST(test_filter_extsquitter)
{
	cfg->modeSLongExtSquitter = true;

	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	char payload[14] = "abcdefghijklmn";
	payload[0] = _i << 3;
	size_t len = INPUT_buildFrame(frm, ADSB_FRAME_TYPE_MODE_S_LONG, 0xdeadbe, 0,
		payload, 14);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;
	test.noRet = true;

	COMP_initAll();
	COMP_startAll();

	const struct ADSB_Frame * frame = BUF_getFrameTimeout(250);
	ck_assert_ptr_ne(frame, NULL);

	ck_assert_uint_eq(STAT_stats.ADSB_longType[_i], 1);

	COMP_stopAll();
	COMP_destructAll();
}
END_TEST

START_TEST(test_reconnect)
{
	test.noRet = false;
	test.buffers = NULL;

	COMP_initAll();
	COMP_startAll();

	usleep(250 * 1000);

	COMP_stopAll();
	COMP_destructAll();
}
END_TEST

static Suite * recv_suite()
{
	Suite * s = suite_create("Receiver");

	TCase * tc = tcase_create("Receive");
	tcase_add_checked_fixture(tc, setup, NULL);
	tcase_add_test(tc, test_recv_frame);
	tcase_add_test(tc, test_filter_unsynchronized);
	tcase_add_test(tc, test_filter_synchronized);
	tcase_add_test(tc, test_filter_modeac);
	tcase_add_test(tc, test_filter_type);
	tcase_add_loop_test(tc, test_filter_extsquitter, 17, 18);
	tcase_add_test(tc, test_reconnect);
	suite_add_tcase(s, tc);

	return s;
}

int main()
{
	SRunner * sr = srunner_create(recv_suite());
	srunner_set_tap(sr, "-");
	srunner_run_all(sr, CK_NORMAL);
	srunner_free(sr);
	return EXIT_SUCCESS;
}
