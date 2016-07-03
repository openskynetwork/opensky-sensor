/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <opensky.hh>
#include <cstring>
#include <check.h>
#include <buffer.h>
#include <adsb.h>
#include <unistd.h>
#include <component.h>
#include <endian.h>

struct Component TB_comp = {};
struct Component NET_comp = {};
struct Component RELAY_comp = {};

static void setup()
{
	OpenSky::init();
	OpenSky::enable();
	usleep(100);
}

static void teardown()
{
	OpenSky::disable();
}

static const struct ADSB_DecodedFrame frame = {
	/*.frameType =*/ ADSB_FRAME_TYPE_MODE_S_LONG,
	/*.mlat =*/ 123456789012345,
	/*.siglevel =*/ 100,
	/*.payloadLen =*/ 14,
	/*.payload =*/ { 0x8a }
};

static void fillFrame(struct ADSB_DecodedFrame * copy)
{
	memcpy(copy, &frame, sizeof frame);
}

static void syncFrame()
{
	OpenSky::setGpsTimeStatus(UsingGpsTime);
}

static void sendFrame(const struct ADSB_DecodedFrame * frame)
{
	uint8_t msg[21];
	uint64_t mlat = htobe64(frame->mlat) >> 16;
	memcpy(msg, &mlat, 6);
	msg[6] = frame->siglevel;
	memcpy(&msg[7], frame->payload, frame->payloadLen);
	OpenSky::output_message(msg, (enum MessageType_T)(frame->frameType + '1'));
}

static void unraw(uint8_t * buf, size_t len, const uint8_t ** raw,
	size_t * rawLen)
{
	size_t i;
	for (i = 0; i < len; ++i) {
		ck_assert_uint_ne(*rawLen, 0);
		uint8_t chr = *((*raw)++);
		*buf++ = chr;
		--*rawLen;
		if (chr == '\x1a') {
			ck_assert_uint_ne(*rawLen, 0);
			chr = *((*raw)++);
			ck_assert_uint_eq(chr, '\x1a');
			--*rawLen;
		}
	}
}

static void checkRaw(const struct ADSB_RawFrame * raw,
	const struct ADSB_DecodedFrame * frame)
{
	/* must be at least the header */
	ck_assert_uint_ge(raw->raw_len, 9);

	/* sync */
	ck_assert_uint_eq(raw->raw[0], '\x1a');

	/* frame type */
	enum ADSB_FRAME_TYPE frameType = (enum ADSB_FRAME_TYPE)(raw->raw[1] - '1');
	ck_assert_uint_ge(frameType, ADSB_FRAME_TYPE_MODE_AC);
	ck_assert_uint_le(frameType, ADSB_FRAME_TYPE_STATUS);

	const uint8_t * rawptr = &raw->raw[2];
	size_t rawLen = raw->raw_len - 2;

	uint8_t buf[14 * 2];

	/* mlat */
	unraw(buf, 6, &rawptr, &rawLen);
	uint64_t mlat = 0;
	memcpy(&mlat, buf, 6);
	mlat = be64toh(mlat) >> 16;
	ck_assert_uint_eq(mlat, frame->mlat);

	/* siglevel */
	unraw(buf, 1, &rawptr, &rawLen);
	int8_t sigLevel = buf[0];
	ck_assert_int_eq(sigLevel, frame->siglevel);

	/* payload */
	unraw(buf, frame->payloadLen, &rawptr, &rawLen);
	ck_assert(!memcmp(buf, frame->payload, frame->payloadLen));
	ck_assert_uint_eq(rawLen, 0);
}

START_TEST(test_start_fail)
{
	OpenSky::enable();
	ck_abort_msg("Test should have failed earlier");
}
END_TEST

START_TEST(test_start_stop)
{
	OpenSky::configure();
	OpenSky::enable();
	usleep(100);
	OpenSky::disable();
}
END_TEST

START_TEST(test_frame)
{
	syncFrame();

	sendFrame(&frame);

	const struct ADSB_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_ne(r, NULL);
	checkRaw(r, &frame);
}
END_TEST

START_TEST(test_frame_siglevel)
{
	syncFrame();

	struct ADSB_DecodedFrame frame;
	fillFrame(&frame);

	frame.siglevel = '\x1a';

	sendFrame(&frame);

	const struct ADSB_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_ne(r, NULL);
	checkRaw(r, &frame);
}
END_TEST

START_TEST(test_frame_mlat_begin)
{
	syncFrame();

	struct ADSB_DecodedFrame frame;
	fillFrame(&frame);

	const uint8_t mlat[] = { '1', 'x', 3, 2, 4, '\x1a' };
	memcpy(&frame.mlat, mlat, 6);

	sendFrame(&frame);

	const struct ADSB_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_ne(r, NULL);
	checkRaw(r, &frame);
}
END_TEST

START_TEST(test_frame_mlat_begin2)
{
	syncFrame();

	struct ADSB_DecodedFrame frame;
	fillFrame(&frame);

	const uint8_t mlat[] = { '1', 'x', 3, 2, '\x1a', '\x1a' };
	memcpy(&frame.mlat, mlat, 6);

	sendFrame(&frame);

	const struct ADSB_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_ne(r, NULL);
	checkRaw(r, &frame);
}
END_TEST

START_TEST(test_frame_mlat_middle)
{
	syncFrame();

	struct ADSB_DecodedFrame frame;
	fillFrame(&frame);

	const uint8_t mlat[] = { '1', 'x', 3, '\x1a', 1, 2 };
	memcpy(&frame.mlat, mlat, 6);

	sendFrame(&frame);

	const struct ADSB_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_ne(r, NULL);
	checkRaw(r, &frame);
}
END_TEST

START_TEST(test_frame_mlat_middle3)
{
	syncFrame();

	struct ADSB_DecodedFrame frame;
	fillFrame(&frame);

	const uint8_t mlat[] = { 'x', '\x1a', '\x1a', '\x1a', 3, 0 };
	memcpy(&frame.mlat, mlat, 6);

	sendFrame(&frame);

	const struct ADSB_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_ne(r, NULL);
	checkRaw(r, &frame);
}
END_TEST

START_TEST(test_frame_mlat_end)
{
	syncFrame();

	struct ADSB_DecodedFrame frame;
	fillFrame(&frame);

	const uint8_t mlat[] = { '\x1a', 'x', 3, 0, 2, 1 };
	memcpy(&frame.mlat, mlat, 6);

	sendFrame(&frame);

	const struct ADSB_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_ne(r, NULL);
	checkRaw(r, &frame);
}
END_TEST

START_TEST(test_frame_mlat_end2)
{
	syncFrame();

	struct ADSB_DecodedFrame frame;
	fillFrame(&frame);

	const uint8_t mlat[] = { '\x1a', '\x1a', 'x', 3, 0, 2 };
	memcpy(&frame.mlat, mlat, 6);

	sendFrame(&frame);

	const struct ADSB_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_ne(r, NULL);
	checkRaw(r, &frame);
}
END_TEST

START_TEST(test_frame_mlat_begin_middle_end)
{
	syncFrame();

	struct ADSB_DecodedFrame frame;
	fillFrame(&frame);

	const uint8_t mlat[] = { '\x1a', 'x', '\x1a', 1, 0, '\x1a' };
	memcpy(&frame.mlat, mlat, 6);

	sendFrame(&frame);

	const struct ADSB_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_ne(r, NULL);
	checkRaw(r, &frame);
}
END_TEST

START_TEST(test_frame_payload)
{
	syncFrame();

	struct ADSB_DecodedFrame frame;
	fillFrame(&frame);

	frame.payload[_i] = '\x1a';

	sendFrame(&frame);

	const struct ADSB_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_ne(r, NULL);
	checkRaw(r, &frame);
}
END_TEST

START_TEST(test_filter_unsync)
{
	sendFrame(&frame);

	const struct ADSB_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(r, NULL);

	/* TODO: check statistics */
}
END_TEST

START_TEST(test_filter_nosquitter)
{
	syncFrame();

	struct ADSB_DecodedFrame frame;
	fillFrame(&frame);

	frame.payload[0] = 3;

	sendFrame(&frame);

	const struct ADSB_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(r, NULL);

	/* TODO: check statistics */
}
END_TEST

static Suite * lib_suite()
{
	Suite * s = suite_create("Lib");

	TCase * tc = tcase_create("StartStop");
	tcase_add_exit_test(tc, test_start_fail, 1);
	tcase_add_test(tc, test_start_stop);
	suite_add_tcase(s, tc);

	tc = tcase_create("Frames");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_frame);
	suite_add_tcase(s, tc);

	tc = tcase_create("SigLevel");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_frame_siglevel);
	suite_add_tcase(s, tc);

	tc = tcase_create("Mlat");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_frame_mlat_begin);
	tcase_add_test(tc, test_frame_mlat_begin2);
	tcase_add_test(tc, test_frame_mlat_middle);
	tcase_add_test(tc, test_frame_mlat_middle3);
	tcase_add_test(tc, test_frame_mlat_end);
	tcase_add_test(tc, test_frame_mlat_end2);
	tcase_add_test(tc, test_frame_mlat_begin_middle_end);
	suite_add_tcase(s, tc);

	tc = tcase_create("Payload");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_loop_test(tc, test_frame_payload, 1, 14);
	suite_add_tcase(s, tc);

	tc = tcase_create("FilterUnsync");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_filter_unsync);
	suite_add_tcase(s, tc);

	tc = tcase_create("FilterType");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_filter_nosquitter);
	suite_add_tcase(s, tc);

	/* TODO: test statistics, test payload len sanity check */

	return s;
}

int main()
{
	SRunner * sr = srunner_create(lib_suite());
	srunner_set_tap(sr, "-");
	srunner_run_all(sr, CK_NORMAL);
	srunner_free(sr);
	return EXIT_SUCCESS;
}
