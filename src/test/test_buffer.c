/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <check.h>
#include <signal.h>
#include <sys/resource.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../buffer.h"
#include "../statistics.h"
#include "../component.h"
#include "../cfgfile.h"

static void setup()
{
	COMP_register(&BUF_comp);
	COMP_fixup();
	COMP_setSilent(true);

	CFG_setBoolean("BUFFER", "GC", false);
	CFG_setInteger("BUFFER", "GCInterval", 120);
	CFG_setInteger("BUFFER", "GCLevel", 10);
	CFG_setBoolean("BUFFER", "History", false);
	CFG_setInteger("BUFFER", "StaticBacklog", 10);
	CFG_setInteger("BUFFER", "DynamicBacklog", 100);
	CFG_setInteger("BUFFER", "DynamicIncrements", 10);
}

START_TEST(test_start_stop)
{
	COMP_initAll();
	COMP_startAll();
	COMP_stopAll();
	COMP_destructAll();
}
END_TEST

#ifndef NDEBUG
START_TEST(test_param_backlog)
{
	CFG_setInteger("BUFFER", "StaticBacklog", 1);
	COMP_initAll();
	ck_abort_msg("Test should have failed");
}
END_TEST
#endif

START_TEST(test_newMsg)
{
	COMP_initAll();
	struct OPENSKY_RawFrame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
}
END_TEST

#ifndef NDEBUG
START_TEST(test_newMsgFail)
{
	COMP_initAll();
	struct OPENSKY_RawFrame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	frame = BUF_newFrame();
	ck_abort_msg("Test should have failed earlier");
}
END_TEST
#endif

START_TEST(test_abort)
{
	COMP_initAll();
	struct OPENSKY_RawFrame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	BUF_abortFrame(frame);
	const struct OPENSKY_RawFrame * frame2 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame2, NULL);
}
END_TEST

START_TEST(test_commit)
{
	COMP_initAll();
	struct OPENSKY_RawFrame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	BUF_commitFrame(frame);
}
END_TEST

START_TEST(test_get)
{
	COMP_initAll();
	struct OPENSKY_RawFrame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	BUF_commitFrame(frame);
	const struct OPENSKY_RawFrame * frame2 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame2, frame);
}
END_TEST

START_TEST(test_get_null)
{
	COMP_initAll();
	const struct OPENSKY_RawFrame * frame = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame, NULL);
}
END_TEST

#ifndef NDEBUG
START_TEST(test_get_fail)
{
	COMP_initAll();
	struct OPENSKY_RawFrame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	BUF_commitFrame(frame);
	const struct OPENSKY_RawFrame * frame2 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame2, frame);
	BUF_getFrameTimeout(0);
	ck_abort_msg("Test should have failed earlier");
}
END_TEST
#endif

START_TEST(test_release)
{
	COMP_initAll();
	struct OPENSKY_RawFrame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	BUF_commitFrame(frame);
	const struct OPENSKY_RawFrame * frame2 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame2, frame);
	BUF_releaseFrame(frame2);
}
END_TEST

#ifndef NDEBUG
START_TEST(test_release_fail)
{
	COMP_initAll();
	struct OPENSKY_RawFrame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	BUF_commitFrame(frame);
	const struct OPENSKY_RawFrame * frame2 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame2, frame);
	BUF_releaseFrame(NULL);
	ck_abort_msg("Test should have failed earlier");
}
END_TEST
#endif

START_TEST(test_queue_n)
{
	struct OPENSKY_RawFrame * frames[10];

	COMP_initAll();
	uint32_t j;
	for (j = 0; j < _i; ++j) {
		frames[j] = BUF_newFrame();
		ck_assert_ptr_ne(frames[j], NULL);
		BUF_commitFrame(frames[j]);
	}
	for (j = 0; j < _i; ++j) {
		const struct OPENSKY_RawFrame * frame = BUF_getFrameTimeout(0);
		ck_assert_ptr_ne(frame, NULL);
		ck_assert_ptr_eq(frame, frames[j]);
		BUF_releaseFrame(frame);
	}
}
END_TEST

START_TEST(test_put)
{
	COMP_initAll();
	struct OPENSKY_RawFrame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	BUF_commitFrame(frame);

	const struct OPENSKY_RawFrame * frame2 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame2, frame);
	BUF_putFrame(frame2);
	frame2 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame2, frame);
}
END_TEST

START_TEST(test_queue_put)
{
	COMP_initAll();
	struct OPENSKY_RawFrame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	BUF_commitFrame(frame);
	struct OPENSKY_RawFrame * frame2 = BUF_newFrame();
	ck_assert_ptr_ne(frame2, NULL);
	BUF_commitFrame(frame2);

	const struct OPENSKY_RawFrame * frame3 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame3, frame);
	BUF_putFrame(frame3);

	frame3 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame3, frame);
	BUF_releaseFrame(frame3);

	frame3 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame3, frame2);
}
END_TEST

START_TEST(test_put_new)
{
	CFG_setInteger("BUFFER", "StaticBacklog", 2);
	COMP_initAll();

	struct OPENSKY_RawFrame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	BUF_commitFrame(frame);

	const struct OPENSKY_RawFrame * frame2 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame2, frame);

	struct OPENSKY_RawFrame * frame3 = BUF_newFrame();
	ck_assert_ptr_ne(frame3, frame2);
	ck_assert_ptr_ne(frame3, NULL);
	BUF_commitFrame(frame3);

	BUF_putFrame(frame2);
	const struct OPENSKY_RawFrame * frame4 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame4, frame);
	ck_assert_ptr_eq(frame4, frame2);
	BUF_releaseFrame(frame4);

	frame4 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame4, frame3);
	BUF_releaseFrame(frame4);

	frame4 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame4, NULL);
}
END_TEST

START_TEST(test_sacrifice)
{
	CFG_setInteger("BUFFER", "StaticBacklog", 2);
	COMP_initAll();

	struct OPENSKY_RawFrame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	BUF_commitFrame(frame);

	struct OPENSKY_RawFrame * frame2 = BUF_newFrame();
	ck_assert_ptr_ne(frame2, NULL);
	ck_assert_ptr_ne(frame2, frame);
	BUF_commitFrame(frame2);

	struct OPENSKY_RawFrame * frame3 = BUF_newFrame();
	ck_assert_ptr_eq(frame3, frame);
	BUF_commitFrame(frame3);

	const struct OPENSKY_RawFrame * frame4 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame4, frame2);
	BUF_releaseFrame(frame4);

	frame4 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame4, frame3);
	BUF_releaseFrame(frame4);

	frame4 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame4, NULL);

	BUF_fillStatistics();
	ck_assert_uint_eq(STAT_stats.BUF_sacrifices, 1);
}
END_TEST

START_TEST(test_sacrifice_n)
{
	CFG_setInteger("BUFFER", "StaticBacklog", _i);
	COMP_initAll();
	uint32_t j;
	for (j = 0; j < _i * 2; ++j) {
		struct OPENSKY_RawFrame * frame = BUF_newFrame();
		ck_assert_ptr_ne(frame, NULL);
		BUF_commitFrame(frame);
	}

	for (j = _i; j < _i * 2; ++j) {
		const struct OPENSKY_RawFrame * frame = BUF_getFrameTimeout(0);
		ck_assert_ptr_ne(frame, NULL);
		BUF_releaseFrame(frame);
	}

	const struct OPENSKY_RawFrame * frame = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame, NULL);

	BUF_fillStatistics();
	ck_assert_uint_eq(STAT_stats.BUF_sacrifices, _i);
}
END_TEST

START_TEST(test_sacrifice_get)
{
	CFG_setInteger("BUFFER", "StaticBacklog", 2);
	COMP_initAll();

	struct OPENSKY_RawFrame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	BUF_commitFrame(frame);

	const struct OPENSKY_RawFrame * frame2 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame2, frame);

	struct OPENSKY_RawFrame * frame3 = BUF_newFrame();
	ck_assert_ptr_ne(frame3, frame2);
	ck_assert_ptr_ne(frame3, NULL);
	BUF_commitFrame(frame3);

	struct OPENSKY_RawFrame * frame4 = BUF_newFrame();
	ck_assert_ptr_eq(frame4, frame3);
	BUF_commitFrame(frame4);

	BUF_releaseFrame(frame2);
	frame2 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame2, frame3);
	BUF_releaseFrame(frame2);

	frame2 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame2, NULL);

	BUF_fillStatistics();
	ck_assert_uint_eq(STAT_stats.BUF_sacrifices, 1);
}
END_TEST

START_TEST(test_sacrifice_put_get)
{
	CFG_setInteger("BUFFER", "StaticBacklog", 2);
	COMP_initAll();

	struct OPENSKY_RawFrame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	BUF_commitFrame(frame);

	const struct OPENSKY_RawFrame * frame2 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame2, frame);

	struct OPENSKY_RawFrame * frame3 = BUF_newFrame();
	ck_assert_ptr_ne(frame3, frame2);
	ck_assert_ptr_ne(frame3, NULL);
	BUF_commitFrame(frame3);

	struct OPENSKY_RawFrame * frame4 = BUF_newFrame();
	ck_assert_ptr_eq(frame4, frame3);
	BUF_commitFrame(frame4);

	BUF_putFrame(frame2);
	const struct OPENSKY_RawFrame * frame5 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame5, frame2);
	BUF_releaseFrame(frame5);

	frame5 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame5, frame4);
	BUF_releaseFrame(frame5);

	frame5 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame5, NULL);

	BUF_fillStatistics();
	ck_assert_uint_eq(STAT_stats.BUF_sacrifices, 1);
}
END_TEST

START_TEST(test_dynamic)
{
	CFG_setInteger("BUFFER", "StaticBacklog", 2);
	CFG_setInteger("BUFFER", "DynamicIncrements", 1);
	CFG_setInteger("BUFFER", "DynamicBacklog", 2);
	CFG_setBoolean("BUFFER", "History", true);
	COMP_initAll();

	uint32_t i;
	for (i = 0; i < 4; ++i) {
		struct OPENSKY_RawFrame * frame = BUF_newFrame();
		ck_assert_ptr_ne(frame, NULL);
		BUF_commitFrame(frame);
	}

	for (i = 0; i < 4; ++i) {
		const struct OPENSKY_RawFrame * frame = BUF_getFrameTimeout(0);
		ck_assert_ptr_ne(frame, NULL);
		BUF_releaseFrame(frame);
	}
	COMP_destructAll();
}
END_TEST

START_TEST(test_dynamic_sacrifice)
{
	CFG_setInteger("BUFFER", "StaticBacklog", 2);
	CFG_setInteger("BUFFER", "DynamicIncrements", 2);
	CFG_setInteger("BUFFER", "DynamicBacklog", 2);
	CFG_setBoolean("BUFFER", "History", true);
	COMP_initAll();

	uint32_t i;
	for (i = 0; i < 7; ++i) {
		struct OPENSKY_RawFrame * frame = BUF_newFrame();
		ck_assert_ptr_ne(frame, NULL);
		BUF_commitFrame(frame);
	}

	for (i = 0; i < 6; ++i) {
		const struct OPENSKY_RawFrame * frame = BUF_getFrameTimeout(0);
		ck_assert_ptr_ne(frame, NULL);
		BUF_releaseFrame(frame);
	}
	const struct OPENSKY_RawFrame * frame = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame, NULL);

	BUF_fillStatistics();
	ck_assert_uint_eq(STAT_stats.BUF_sacrifices, 1);

	COMP_destructAll();
}
END_TEST

START_TEST(test_dynamic_exhaust)
{
	struct rlimit limit;
	limit.rlim_cur = 20 << 20;
	limit.rlim_max = 20 << 20;
	setrlimit(RLIMIT_AS, &limit);

	CFG_setInteger("BUFFER", "StaticBacklog", 10);
	CFG_setInteger("BUFFER", "DynamicIncrements", 1000000);
	CFG_setInteger("BUFFER", "DynamicBacklog", 1000);
	CFG_setBoolean("BUFFER", "History", true);
	COMP_initAll();

	struct OPENSKY_RawFrame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	BUF_commitFrame(frame);

	while (true) {
		struct OPENSKY_RawFrame * frame2 = BUF_newFrame();
		ck_assert_ptr_ne(frame2, NULL);
		BUF_commitFrame(frame2);
		if (frame2 == frame)
			break;
	}

	const struct OPENSKY_RawFrame * frame2;
	while ((frame2 = BUF_getFrameTimeout(0)) != NULL)
		BUF_releaseFrame(frame2);

	BUF_fillStatistics();
	ck_assert_uint_eq(STAT_stats.BUF_sacrifices, 1);

	COMP_destructAll();
}
END_TEST

START_TEST(test_dynamic_uncollect)
{
	CFG_setInteger("BUFFER", "StaticBacklog", 2);
	CFG_setInteger("BUFFER", "DynamicIncrements", 1);
	CFG_setInteger("BUFFER", "DynamicBacklog", 10);
	CFG_setBoolean("BUFFER", "History", true);
	COMP_initAll();

	struct OPENSKY_RawFrame * static1 = BUF_newFrame();
	ck_assert_ptr_ne(static1, NULL);
	BUF_commitFrame(static1);

	struct OPENSKY_RawFrame * static2 = BUF_newFrame();
	ck_assert_ptr_ne(static2, NULL);
	BUF_commitFrame(static2);

	struct OPENSKY_RawFrame * dynamic1 = BUF_newFrame();
	ck_assert_ptr_ne(dynamic1, NULL);
	ck_assert_ptr_ne(dynamic1, static1);
	ck_assert_ptr_ne(dynamic1, static2);
	BUF_commitFrame(dynamic1);

	const struct OPENSKY_RawFrame * static1_1 = BUF_getFrame();
	ck_assert_ptr_eq(static1_1, static1);
	BUF_releaseFrame(static1_1);

	ck_assert_uint_eq(STAT_stats.BUF_uncollects, 0);
	BUF_runGC();

	struct OPENSKY_RawFrame * static1_2 = BUF_newFrame();
	ck_assert_ptr_eq(static1_2, static1);
	BUF_commitFrame(static1_2);

	struct OPENSKY_RawFrame * dynamic2 = BUF_newFrame();
	ck_assert_ptr_ne(dynamic2, NULL);
	ck_assert_ptr_ne(dynamic2, static1);
	ck_assert_ptr_ne(dynamic2, static2);
	ck_assert_ptr_ne(dynamic2, dynamic1);
	BUF_commitFrame(dynamic2);
	ck_assert_uint_eq(STAT_stats.BUF_uncollects, 1);

	const struct OPENSKY_RawFrame * frame;
	frame = BUF_getFrame();
	ck_assert_ptr_eq(frame, static2);
	BUF_releaseFrame(frame);
	frame = BUF_getFrame();
	ck_assert_ptr_eq(frame, dynamic1);
	BUF_releaseFrame(frame);
	frame = BUF_getFrame();
	ck_assert_ptr_eq(frame, static1_2);
	BUF_releaseFrame(frame);
	frame = BUF_getFrame();
	ck_assert_ptr_eq(frame, dynamic2);
	BUF_releaseFrame(frame);

	ck_assert_ptr_eq(BUF_getFrameTimeout(0), NULL);
	COMP_destructAll();
}
END_TEST

START_TEST(test_dynamic_destroy)
{
	CFG_setInteger("BUFFER", "StaticBacklog", 2);
	CFG_setInteger("BUFFER", "DynamicIncrements", 1);
	CFG_setInteger("BUFFER", "DynamicBacklog", 10);
	CFG_setBoolean("BUFFER", "History", true);
	COMP_initAll();

	struct OPENSKY_RawFrame * static1 = BUF_newFrame();
	ck_assert_ptr_ne(static1, NULL);
	BUF_commitFrame(static1);

	struct OPENSKY_RawFrame * static2 = BUF_newFrame();
	ck_assert_ptr_ne(static2, NULL);
	BUF_commitFrame(static2);

	BUF_fillStatistics();
	ck_assert_uint_eq(STAT_stats.BUF_pools, 0);
	struct OPENSKY_RawFrame * dynamic1 = BUF_newFrame();
	BUF_fillStatistics();
	ck_assert_uint_eq(STAT_stats.BUF_pools, 1);
	ck_assert_ptr_ne(dynamic1, NULL);
	ck_assert_ptr_ne(dynamic1, static1);
	ck_assert_ptr_ne(dynamic1, static2);
	BUF_commitFrame(dynamic1);

	BUF_fillStatistics();
	ck_assert_uint_eq(STAT_stats.BUF_pools, 1);

	BUF_runGC();

	BUF_fillStatistics();
	ck_assert_uint_eq(STAT_stats.BUF_pools, 1);

	const struct OPENSKY_RawFrame * frame;
	frame = BUF_getFrame();
	ck_assert_ptr_eq(frame, static1);
	BUF_releaseFrame(frame);
	frame = BUF_getFrame();
	ck_assert_ptr_eq(frame, static2);
	BUF_releaseFrame(frame);
	frame = BUF_getFrame();
	ck_assert_ptr_eq(frame, dynamic1);
	BUF_releaseFrame(frame);

	ck_assert_ptr_eq(BUF_getFrameTimeout(0), NULL);

	BUF_fillStatistics();
	ck_assert_uint_eq(STAT_stats.BUF_pools, 1);
	BUF_runGC();
	BUF_fillStatistics();
	ck_assert_uint_eq(STAT_stats.BUF_pools, 0);

	COMP_destructAll();
}
END_TEST

START_TEST(test_dynamic_destroy_2nd)
{
	CFG_setInteger("BUFFER", "StaticBacklog", 2);
	CFG_setInteger("BUFFER", "DynamicIncrements", 2);
	CFG_setInteger("BUFFER", "DynamicBacklog", 2);
	CFG_setBoolean("BUFFER", "History", true);
	COMP_initAll();

	uint32_t i;
	for (i = 0; i < 4; ++i) {
		struct OPENSKY_RawFrame * frame = BUF_newFrame();
		ck_assert_ptr_ne(frame, NULL);
		BUF_commitFrame(frame);
	}

	struct OPENSKY_RawFrame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);

	for (i = 0; i < 4; ++i) {
		const struct OPENSKY_RawFrame * frame = BUF_getFrame();
		ck_assert_ptr_ne(frame, NULL);
		BUF_releaseFrame(frame);
	}

	BUF_commitFrame(frame);

	BUF_fillStatistics();
	ck_assert_uint_eq(STAT_stats.BUF_pools, 2);

	BUF_runGC();

	BUF_fillStatistics();
	ck_assert_uint_eq(STAT_stats.BUF_pools, 1);

	COMP_destructAll();
}
END_TEST

START_TEST(test_flush)
{
	COMP_initAll();

	uint32_t i;
	for (i = 0; i < 10 - _i; ++i) {
		struct OPENSKY_RawFrame * frame = BUF_newFrame();
		ck_assert_ptr_ne(frame, NULL);
		BUF_commitFrame(frame);
	}

	BUF_fillStatistics();
	ck_assert_uint_eq(STAT_stats.BUF_queue, 10 - _i);
	ck_assert_uint_eq(STAT_stats.BUF_flushes, 0);

	BUF_flush();

	BUF_fillStatistics();
	ck_assert_uint_eq(STAT_stats.BUF_queue, 0);
	ck_assert_uint_eq(STAT_stats.BUF_flushes, 1);

	ck_assert_ptr_eq(BUF_getFrameTimeout(0), NULL);
}
END_TEST

static Suite * buffer_suite()
{
	Suite * s = suite_create("Buffer");

	TCase * tc = tcase_create("StartStop");
	tcase_add_checked_fixture(tc, setup, NULL);
	tcase_add_test(tc, test_start_stop);
#ifndef NDEBUG
	tcase_add_test_raise_signal(tc, test_param_backlog, SIGABRT);
#endif
	suite_add_tcase(s, tc);

	tc = tcase_create("Producer");
	tcase_add_checked_fixture(tc, setup, NULL);
	tcase_add_test(tc, test_newMsg);
#ifndef NDEBUG
	tcase_add_test_raise_signal(tc, test_newMsgFail, SIGABRT);
#endif
	tcase_add_test(tc, test_abort);
	tcase_add_test(tc, test_commit);
	suite_add_tcase(s, tc);

	tc = tcase_create("Consumer");
	tcase_add_checked_fixture(tc, setup, NULL);
	tcase_add_test(tc, test_get);
	tcase_add_test(tc, test_get_null);
#ifndef NDEBUG
	tcase_add_test_raise_signal(tc, test_get_fail, SIGABRT);
#endif
	tcase_add_test(tc, test_release);
#ifndef NDEBUG
	tcase_add_test_raise_signal(tc, test_release_fail, SIGABRT);
#endif
	suite_add_tcase(s, tc);

	tc = tcase_create("Queue");
	tcase_add_checked_fixture(tc, setup, NULL);
	tcase_add_loop_test(tc, test_queue_n, 1, 10);
	tcase_add_test(tc, test_put);
	tcase_add_test(tc, test_queue_put);
	tcase_add_test(tc, test_put_new);
	suite_add_tcase(s, tc);

	tc = tcase_create("Sacrifice");
	tcase_add_checked_fixture(tc, setup, NULL);
	tcase_add_test(tc, test_sacrifice);
	tcase_add_loop_test(tc, test_sacrifice_n, 2, 20);
	tcase_add_test(tc, test_sacrifice_get);
	tcase_add_test(tc, test_sacrifice_put_get);
	suite_add_tcase(s, tc);

	tc = tcase_create("Dynamic");
	tcase_add_checked_fixture(tc, setup, NULL);
	tcase_add_test(tc, test_dynamic);
	tcase_add_test(tc, test_dynamic_sacrifice);
	tcase_add_test(tc, test_dynamic_exhaust);
	tcase_add_test(tc, test_dynamic_uncollect);
	tcase_add_test(tc, test_dynamic_destroy);
	tcase_add_test(tc, test_dynamic_destroy_2nd);
	suite_add_tcase(s, tc);

	tc = tcase_create("Flush");
	tcase_add_checked_fixture(tc, setup, NULL);
	tcase_add_loop_test(tc, test_flush, 0, 2);
	suite_add_tcase(s, tc);

	return s;
}

int main()
{
	SRunner * sr = srunner_create(buffer_suite());
	srunner_set_tap(sr, "-");
	srunner_run_all(sr, CK_NORMAL);
	srunner_free(sr);
	return EXIT_SUCCESS;
}
