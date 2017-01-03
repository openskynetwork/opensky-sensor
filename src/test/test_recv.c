/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <check.h>
#include <signal.h>
#include <ctype.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include "input_test.h"
#include "core/openskytypes.h"
#include "core/recv.h"
#include "core/buffer.h"
#include "core/filter.h"
#include "util/cfgfile.h"

static void setup()
{
	COMP_register(&RECV_comp);
	COMP_register(&BUF_comp);
	COMP_fixup();
	CFG_loadDefaults();

	CFG_setBoolean("BUFFER", "GC", false);
	CFG_setBoolean("BUFFER", "History", false);
	CFG_setInteger("BUFFER", "StaticBacklog", 10);
	CFG_setInteger("BUFFER", "DynamicBacklog", 100);
	CFG_setInteger("BUFFER", "DynamicIncrements", 10);

	CFG_setBoolean("FILTER", "CRC", false);
	CFG_setBoolean("FILTER", "ModeSExtSquitterOnly", false);
	CFG_setBoolean("FILTER", "SyncFilter", false);

	COMP_setSilent(true);
}

START_TEST(test_recv_frame)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_S_LONG, 0xdeadbe, 0,
		"abcdefghijklmn", 14);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;
	test.noRet = true;

	CFG_setBoolean("FILTER", "SyncFilter", false);

	COMP_initAll();
	COMP_startAll();

	const struct OPENSKY_RawFrame * frame = BUF_getFrameTimeout(250);
	ck_assert_ptr_ne(frame, NULL);
	ck_assert_uint_eq(frame->rawLen, len);
	ck_assert(!memcmp(frame->raw, frm, len));
	struct FILTER_Statistics stats;
	FILTER_getStatistics(&stats);
	ck_assert_uint_eq(stats.framesByType[OPENSKY_FRAME_TYPE_MODE_S_LONG], 1);
	ck_assert_uint_eq(stats.modeSByType[('a' >> 3) & 0x1f], 1);
	ck_assert_uint_eq(stats.unsynchronized, 1);

	COMP_stopAll();
	COMP_destructAll();
}
END_TEST

START_TEST(test_filter_unsynchronized)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_S_LONG, 0xdeadbe, 0,
		"abcdefghijklmn", 14);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;
	test.noRet = true;

	CFG_setBoolean("FILTER", "SyncFilter", true);

	COMP_initAll();
	COMP_startAll();

	const struct OPENSKY_RawFrame * frame = BUF_getFrameTimeout(250);
	ck_assert_ptr_eq(frame, NULL);
	struct FILTER_Statistics stats;
	FILTER_getStatistics(&stats);
	ck_assert_uint_eq(stats.unsynchronized, 1);

	COMP_stopAll();
	COMP_destructAll();
}
END_TEST

START_TEST(test_filter_synchronized)
{
	uint8_t frm[46], frm2[46];
	struct TEST_Buffer buf[2] = { { .payload = frm }, { .payload = frm2  } };
	size_t len1 = RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_STATUS, 0x123456, 0,
		"abcdefghijklmn", 14);
	buf[0].length = len1;
	size_t len2 = RC_INPUT_buildFrame(frm2, OPENSKY_FRAME_TYPE_MODE_S_LONG, 0xdeadbe,
		0, "abcdefghijklmn", 14);
	buf[1].length = len2;
	test.buffers = buf;
	test.nBuffers = 2;
	test.noRet = true;

	CFG_setBoolean("FILTER", "SyncFilter", true);

	COMP_initAll();
	COMP_startAll();

	const struct OPENSKY_RawFrame * frame = BUF_getFrameTimeout(250);
	ck_assert_ptr_ne(frame, NULL);
	ck_assert_uint_eq(frame->rawLen, len2);
	ck_assert(!memcmp(frame->raw, frm2, len2));
	struct FILTER_Statistics stats;
	FILTER_getStatistics(&stats);
	ck_assert_uint_eq(stats.unsynchronized, 0);

	COMP_stopAll();
	COMP_destructAll();
}
END_TEST

START_TEST(test_filter_modeac)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_AC, 0xdeadbe, 0,
		"ab", 2);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;
	test.noRet = true;

	CFG_setBoolean("FILTER", "SyncFilter", true);

	COMP_initAll();
	COMP_startAll();

	const struct OPENSKY_RawFrame * frame = BUF_getFrameTimeout(250);
	ck_assert_ptr_eq(frame, NULL);
	struct FILTER_Statistics stats;
	FILTER_getStatistics(&stats);
	ck_assert_uint_eq(stats.filtered, 1);

	COMP_stopAll();
	COMP_destructAll();
}
END_TEST

START_TEST(test_filter_type)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	size_t len = RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_S_LONG,
		0xdeadbe, 0, "abcdefghijklmn", 14);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;
	test.noRet = true;

	CFG_setBoolean("FILTER", "ModeSExtSquitterOnly", true);

	COMP_initAll();
	COMP_startAll();

	const struct OPENSKY_RawFrame * frame = BUF_getFrameTimeout(250);
	ck_assert_ptr_eq(frame, NULL);
	struct FILTER_Statistics stats;
	FILTER_getStatistics(&stats);
	ck_assert_uint_eq(stats.filtered, 1);
	ck_assert_uint_eq(stats.modeSfiltered, 1);

	COMP_stopAll();
	COMP_destructAll();
}
END_TEST

START_TEST(test_filter_extsquitter)
{
	uint8_t frm[46];
	struct TEST_Buffer buf = { .payload = frm };
	char payload[14] = "abcdefghijklmn";
	payload[0] = _i << 3;
	size_t len = RC_INPUT_buildFrame(frm, OPENSKY_FRAME_TYPE_MODE_S_LONG,
		0xdeadbe, 0, payload, 14);
	buf.length = len;
	test.buffers = &buf;
	test.nBuffers = 1;
	test.noRet = true;

	CFG_setBoolean("FILTER", "ModeSExtSquitterOnly", true);

	COMP_initAll();
	COMP_startAll();

	const struct OPENSKY_RawFrame * frame = BUF_getFrameTimeout(250);
	ck_assert_ptr_ne(frame, NULL);

	struct FILTER_Statistics stats;
	FILTER_getStatistics(&stats);
	ck_assert_uint_eq(stats.filtered, 0);
	ck_assert_uint_eq(stats.modeSByType[_i], 1);

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
