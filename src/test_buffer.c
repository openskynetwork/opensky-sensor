
#include <check.h>
#include <buffer.h>
#include <component.h>
#include <cfgfile.h>
#include <signal.h>

struct CFG_Config CFG_config;
static struct CFG_BUF * const cfg = &CFG_config.buf;

static void setup()
{
	cfg->gcEnabled = false;
	cfg->gcInterval = 120;
	cfg->gcLevel = 10;
	cfg->history = false;
	cfg->statBacklog = 10;
	cfg->dynBacklog = 100;
	cfg->dynIncrement = 10;
	COMP_register(&BUF_comp, NULL);
	COMP_setSilent(true);
}

START_TEST(test_start_stop)
{
	COMP_initAll();
	COMP_startAll();
	COMP_stopAll();
	COMP_destructAll();
}
END_TEST

START_TEST(test_param_backlog)
{
	cfg->statBacklog = 1;
	COMP_initAll();
	ck_abort_msg("Test should have failed");
}
END_TEST

START_TEST(test_newMsg)
{
	COMP_initAll();
	struct ADSB_Frame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
}
END_TEST

START_TEST(test_newMsgFail)
{
	COMP_initAll();
	struct ADSB_Frame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	frame = BUF_newFrame();
	ck_abort_msg("Test should have failed earlier");
}
END_TEST

START_TEST(test_abort)
{
	COMP_initAll();
	struct ADSB_Frame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	BUF_abortFrame(frame);
	const struct ADSB_Frame * frame2 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame2, NULL);
}
END_TEST

START_TEST(test_commit)
{
	COMP_initAll();
	struct ADSB_Frame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	frame->siglevel = 0x56;
	BUF_commitFrame(frame);
}
END_TEST

START_TEST(test_get)
{
	COMP_initAll();
	struct ADSB_Frame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	frame->siglevel = 0x56;
	BUF_commitFrame(frame);
	const struct ADSB_Frame * frame2 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame2, frame);
	ck_assert_int_eq(frame2->siglevel, 0x56);
}
END_TEST

START_TEST(test_get_null)
{
	COMP_initAll();
	const struct ADSB_Frame * frame = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame, NULL);
}
END_TEST

START_TEST(test_get_fail)
{
	COMP_initAll();
	struct ADSB_Frame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	frame->siglevel = 0x56;
	BUF_commitFrame(frame);
	const struct ADSB_Frame * frame2 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame2, frame);
	ck_assert_int_eq(frame2->siglevel, 0x56);
	BUF_getFrameTimeout(0);
	ck_abort_msg("Test should have failed earlier");
}
END_TEST

START_TEST(test_release)
{
	COMP_initAll();
	struct ADSB_Frame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	frame->siglevel = 0x56;
	BUF_commitFrame(frame);
	const struct ADSB_Frame * frame2 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame2, frame);
	ck_assert_int_eq(frame2->siglevel, 0x56);
	BUF_releaseFrame(frame2);
}
END_TEST

START_TEST(test_release_fail)
{
	COMP_initAll();
	struct ADSB_Frame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	frame->siglevel = 0x56;
	BUF_commitFrame(frame);
	const struct ADSB_Frame * frame2 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame2, frame);
	ck_assert_int_eq(frame2->siglevel, 0x56);
	BUF_releaseFrame(NULL);
}
END_TEST

START_TEST(test_queue_n)
{
	COMP_initAll();
	uint32_t j;
	for (j = 0; j < _i; ++j) {
		struct ADSB_Frame * frame = BUF_newFrame();
		ck_assert_ptr_ne(frame, NULL);
		frame->siglevel = j;
		BUF_commitFrame(frame);
	}
	for (j = 0; j < _i; ++j) {
		const struct ADSB_Frame * frame = BUF_getFrameTimeout(0);
		ck_assert_ptr_ne(frame, NULL);
		ck_assert_int_eq(frame->siglevel, j);
		BUF_releaseFrame(frame);
	}
}
END_TEST

START_TEST(test_put)
{
	COMP_initAll();
	struct ADSB_Frame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	frame->siglevel = 0x56;
	BUF_commitFrame(frame);

	const struct ADSB_Frame * frame2 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame2, frame);
	ck_assert_int_eq(frame->siglevel, 0x56);
	BUF_putFrame(frame2);
	frame2 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame2, frame);
	ck_assert_int_eq(frame2->siglevel, 0x56);
}
END_TEST

START_TEST(test_queue_put)
{
	COMP_initAll();
	struct ADSB_Frame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	frame->siglevel = 0x56;
	BUF_commitFrame(frame);
	struct ADSB_Frame * frame2 = BUF_newFrame();
	ck_assert_ptr_ne(frame2, NULL);
	frame2->siglevel = 0x57;
	BUF_commitFrame(frame2);

	const struct ADSB_Frame * frame3 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame3, frame);
	ck_assert_int_eq(frame3->siglevel, 0x56);
	BUF_putFrame(frame3);

	frame3 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame3, frame);
	ck_assert_int_eq(frame3->siglevel, 0x56);
	BUF_releaseFrame(frame3);

	frame3 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame3, frame2);
	ck_assert_int_eq(frame3->siglevel, 0x57);
}
END_TEST

START_TEST(test_sacrifice)
{
	cfg->statBacklog = 2;
	COMP_initAll();

	struct ADSB_Frame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	frame->siglevel = 0x56;
	BUF_commitFrame(frame);

	struct ADSB_Frame * frame2 = BUF_newFrame();
	ck_assert_ptr_ne(frame2, NULL);
	ck_assert_ptr_ne(frame2, frame);
	frame2->siglevel = 0x57;
	BUF_commitFrame(frame2);

	struct ADSB_Frame * frame3 = BUF_newFrame();
	ck_assert_ptr_eq(frame3, frame);
	frame3->siglevel = 0x58;
	BUF_commitFrame(frame3);

	const struct ADSB_Frame * frame4 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame4, frame2);
	ck_assert_int_eq(frame4->siglevel, 0x57);
	BUF_releaseFrame(frame4);

	frame4 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame4, frame3);
	ck_assert_int_eq(frame4->siglevel, 0x58);
	BUF_releaseFrame(frame4);

	frame4 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame4, NULL);
}
END_TEST

START_TEST(test_sacrifice_n)
{
	cfg->statBacklog = _i;
	COMP_initAll();
	uint32_t j;
	for (j = 0; j < _i * 2; ++j) {
		struct ADSB_Frame * frame = BUF_newFrame();
		ck_assert_ptr_ne(frame, NULL);
		frame->siglevel = j;
		BUF_commitFrame(frame);
	}

	for (j = _i; j < _i * 2; ++j) {
		const struct ADSB_Frame * frame = BUF_getFrameTimeout(0);
		ck_assert_ptr_ne(frame, NULL);
		ck_assert_int_eq(frame->siglevel, j);
		BUF_releaseFrame(frame);
	}

	const struct ADSB_Frame * frame = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame, NULL);
}
END_TEST

START_TEST(test_dynamic)
{
	cfg->statBacklog = 2;
	cfg->dynIncrement = 1;
	cfg->dynBacklog = 2;
	cfg->history = true;
	COMP_initAll();

	uint32_t i;
	for (i = 0; i < 4; ++i) {
		struct ADSB_Frame * frame = BUF_newFrame();
		ck_assert_ptr_ne(frame, NULL);
		frame->siglevel = i;
		BUF_commitFrame(frame);
	}

	for (i = 0; i < 4; ++i) {
		const struct ADSB_Frame * frame = BUF_getFrameTimeout(0);
		ck_assert_ptr_ne(frame, NULL);
		ck_assert_int_eq(frame->siglevel, i);
		BUF_releaseFrame(frame);
	}
}
END_TEST

START_TEST(test_dynamic_sacrifice)
{
	cfg->statBacklog = 2;
	cfg->dynIncrement = 2;
	cfg->dynBacklog = 2;
	cfg->history = true;
	COMP_initAll();

	uint32_t i;
	for (i = 0; i < 7; ++i) {
		struct ADSB_Frame * frame = BUF_newFrame();
		ck_assert_ptr_ne(frame, NULL);
		frame->siglevel = i;
		BUF_commitFrame(frame);
	}

	for (i = 0; i < 6; ++i) {
		const struct ADSB_Frame * frame = BUF_getFrameTimeout(0);
		ck_assert_ptr_ne(frame, NULL);
		ck_assert_int_eq(frame->siglevel, i + 1);
		BUF_releaseFrame(frame);
	}
	const struct ADSB_Frame * frame = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame, NULL);
}
END_TEST

START_TEST(test_sacrifice_put)
{
	cfg->statBacklog = 2;
	COMP_initAll();

	struct ADSB_Frame * frame = BUF_newFrame();
	ck_assert_ptr_ne(frame, NULL);
	frame->siglevel = 0x56;
	BUF_commitFrame(frame);

	const struct ADSB_Frame * frame2 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame2, frame);
	ck_assert_int_eq(frame2->siglevel, 0x56);

	struct ADSB_Frame * frame3 = BUF_newFrame();
	ck_assert_ptr_ne(frame3, frame2);
	ck_assert_ptr_ne(frame3, NULL);
	frame3->siglevel = 0x57;
	BUF_commitFrame(frame3);

	BUF_putFrame(frame2);
	const struct ADSB_Frame * frame4 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame4, frame);
	ck_assert_ptr_eq(frame4, frame2);
	ck_assert_int_eq(frame4->siglevel, 0x56);
	BUF_releaseFrame(frame4);

	frame4 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame4, frame3);
	ck_assert_int_eq(frame4->siglevel, 0x57);
	BUF_releaseFrame(frame4);

	frame4 = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(frame4, NULL);
}
END_TEST

static Suite * buffer_suite()
{
	Suite * s = suite_create("Buffer");

	TCase * tc = tcase_create("StartStop");
	tcase_add_checked_fixture(tc, setup, NULL);
	tcase_add_test(tc, test_start_stop);
	tcase_add_test_raise_signal(tc, test_param_backlog, SIGABRT);
	suite_add_tcase(s, tc);

	tc = tcase_create("Producer");
	tcase_add_checked_fixture(tc, setup, NULL);
	tcase_add_test(tc, test_newMsg);
	tcase_add_test_raise_signal(tc, test_newMsgFail, SIGABRT);
	tcase_add_test(tc, test_abort);
	tcase_add_test(tc, test_commit);
	suite_add_tcase(s, tc);

	tc = tcase_create("Consumer");
	tcase_add_checked_fixture(tc, setup, NULL);
	tcase_add_test(tc, test_get);
	tcase_add_test(tc, test_get_null);
	tcase_add_test_raise_signal(tc, test_get_fail, SIGABRT);
	tcase_add_test(tc, test_release);
	tcase_add_test_raise_signal(tc, test_release_fail, SIGABRT);
	suite_add_tcase(s, tc);

	tc = tcase_create("Queue");
	tcase_add_checked_fixture(tc, setup, NULL);
	tcase_add_loop_test(tc, test_queue_n, 1, 10);
	tcase_add_test(tc, test_put);
	tcase_add_test(tc, test_queue_put);
	tcase_add_test(tc, test_sacrifice);
	tcase_add_loop_test(tc, test_sacrifice_n, 2, 20);
	tcase_add_test(tc, test_dynamic);
	tcase_add_test(tc, test_dynamic_sacrifice);
	tcase_add_test(tc, test_sacrifice_put);
	suite_add_tcase(s, tc);



	return s;
}

int main()
{
	Suite * s = buffer_suite();
	SRunner * sr = srunner_create(s);
	srunner_set_tap(sr, "-");
	srunner_run_all(sr, CK_NORMAL);
	srunner_free(sr);
	return EXIT_SUCCESS;
}
