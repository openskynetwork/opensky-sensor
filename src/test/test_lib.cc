/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <ostream>
#include <cstring>
#include <check.h>
#include <unistd.h>
#include <endian.h>
#include <signal.h>
#include "radarcape/lib/opensky.hh"
#include "core/buffer.h"
#include "core/openskytypes.h"
#include "core/login.h"
#include "core/openskytypes.h"

static enum OPENSKY_DEVICE_TYPE deviceType;

extern "C" {

void LOGIN_setDeviceType(enum OPENSKY_DEVICE_TYPE type)
{
	deviceType = type;
}

}

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

static const struct OPENSKY_DecodedFrame frame = {
	/*.frameType =*/ OPENSKY_FRAME_TYPE_MODE_S_LONG,
	/*.mlat =*/ 123456789012345,
	/*.siglevel =*/ 100,
	/*.payloadLen =*/ 14,
	/*.payload =*/ { 0x8a }
};

static void fillFrame(struct OPENSKY_DecodedFrame * copy)
{
	memcpy(copy, &frame, sizeof frame);
}

static void syncFrame()
{
	OpenSky::setGpsTimeStatus(UsingGpsTime);
}

static void sendFrame(const struct OPENSKY_DecodedFrame * frame)
{
	uint8_t msg[21];
	uint64_t mlat = htobe64(frame->mlat) >> 16;
	memcpy(msg, &mlat, 6);
	msg[6] = frame->siglevel;
	memcpy(&msg[7], frame->payload, frame->payloadLen);
	OpenSky::output_message(msg, (enum MessageType_T)(frame->frameType));
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
		if (chr == BEAST_SYNC) {
			ck_assert_uint_ne(*rawLen, 0);
			chr = *((*raw)++);
			ck_assert_uint_eq(chr, BEAST_SYNC);
			--*rawLen;
		}
	}
}

static void checkRaw(const struct OPENSKY_RawFrame * raw,
	const struct OPENSKY_DecodedFrame * frame)
{
	/* must be at least the header */
	ck_assert_uint_ge(raw->rawLen, 9);

	/* sync */
	ck_assert_uint_eq(raw->raw[0], OPENSKY_SYNC);

	/* frame type */
	enum OPENSKY_FRAME_TYPE frameType = (enum OPENSKY_FRAME_TYPE)raw->raw[1];
	ck_assert_uint_ge(frameType, OPENSKY_FRAME_TYPE_MODE_AC);
	ck_assert_uint_le(frameType, OPENSKY_FRAME_TYPE_STATUS);

	const uint8_t * rawptr = &raw->raw[2];
	size_t rawLen = raw->rawLen - 2;

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

START_TEST(test_deviceId)
{
	OpenSky::init();
	ck_assert_uint_eq(deviceType, OPENSKY_DEVICE_TYPE_RADARCAPE_LIB);
}
END_TEST

START_TEST(test_start_stop)
{
	OpenSky::init();
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

	const struct OPENSKY_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_ne(r, NULL);
	checkRaw(r, &frame);
}
END_TEST

START_TEST(test_frame_siglevel)
{
	syncFrame();

	struct OPENSKY_DecodedFrame frame;
	fillFrame(&frame);

	frame.siglevel = BEAST_SYNC;

	sendFrame(&frame);

	const struct OPENSKY_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_ne(r, NULL);
	checkRaw(r, &frame);
}
END_TEST

START_TEST(test_frame_mlat_begin)
{
	syncFrame();

	struct OPENSKY_DecodedFrame frame;
	fillFrame(&frame);

	const uint8_t mlat[] = { '1', 'x', 3, 2, 4, BEAST_SYNC };
	memcpy(&frame.mlat, mlat, 6);

	sendFrame(&frame);

	const struct OPENSKY_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_ne(r, NULL);
	checkRaw(r, &frame);
}
END_TEST

START_TEST(test_frame_mlat_begin2)
{
	syncFrame();

	struct OPENSKY_DecodedFrame frame;
	fillFrame(&frame);

	const uint8_t mlat[] = { '1', 'x', 3, 2, BEAST_SYNC, BEAST_SYNC };
	memcpy(&frame.mlat, mlat, 6);

	sendFrame(&frame);

	const struct OPENSKY_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_ne(r, NULL);
	checkRaw(r, &frame);
}
END_TEST

START_TEST(test_frame_mlat_middle)
{
	syncFrame();

	struct OPENSKY_DecodedFrame frame;
	fillFrame(&frame);

	const uint8_t mlat[] = { '1', 'x', 3, BEAST_SYNC, 1, 2 };
	memcpy(&frame.mlat, mlat, 6);

	sendFrame(&frame);

	const struct OPENSKY_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_ne(r, NULL);
	checkRaw(r, &frame);
}
END_TEST

START_TEST(test_frame_mlat_middle3)
{
	syncFrame();

	struct OPENSKY_DecodedFrame frame;
	fillFrame(&frame);

	const uint8_t mlat[] = { 'x', BEAST_SYNC, BEAST_SYNC, BEAST_SYNC, 3, 0 };
	memcpy(&frame.mlat, mlat, 6);

	sendFrame(&frame);

	const struct OPENSKY_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_ne(r, NULL);
	checkRaw(r, &frame);
}
END_TEST

START_TEST(test_frame_mlat_end)
{
	syncFrame();

	struct OPENSKY_DecodedFrame frame;
	fillFrame(&frame);

	const uint8_t mlat[] = { BEAST_SYNC, 'x', 3, 0, 2, 1 };
	memcpy(&frame.mlat, mlat, 6);

	sendFrame(&frame);

	const struct OPENSKY_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_ne(r, NULL);
	checkRaw(r, &frame);
}
END_TEST

START_TEST(test_frame_mlat_end2)
{
	syncFrame();

	struct OPENSKY_DecodedFrame frame;
	fillFrame(&frame);

	const uint8_t mlat[] = { BEAST_SYNC, BEAST_SYNC, 'x', 3, 0, 2 };
	memcpy(&frame.mlat, mlat, 6);

	sendFrame(&frame);

	const struct OPENSKY_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_ne(r, NULL);
	checkRaw(r, &frame);
}
END_TEST

START_TEST(test_frame_mlat_begin_middle_end)
{
	syncFrame();

	struct OPENSKY_DecodedFrame frame;
	fillFrame(&frame);

	const uint8_t mlat[] = { BEAST_SYNC, 'x', BEAST_SYNC, 1, 0, BEAST_SYNC };
	memcpy(&frame.mlat, mlat, 6);

	sendFrame(&frame);

	const struct OPENSKY_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_ne(r, NULL);
	checkRaw(r, &frame);
}
END_TEST

START_TEST(test_frame_payload)
{
	syncFrame();

	struct OPENSKY_DecodedFrame frame;
	fillFrame(&frame);

	frame.payload[_i] = BEAST_SYNC;

	sendFrame(&frame);

	const struct OPENSKY_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_ne(r, NULL);
	checkRaw(r, &frame);
}
END_TEST

START_TEST(test_filter_unsync)
{
	sendFrame(&frame);

	const struct OPENSKY_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(r, NULL);

	/* TODO: check statistics */
}
END_TEST

START_TEST(test_filter_nosquitter)
{
	syncFrame();

	struct OPENSKY_DecodedFrame frame;
	fillFrame(&frame);

	frame.payload[0] = 3;

	sendFrame(&frame);

	const struct OPENSKY_RawFrame * r = BUF_getFrameTimeout(0);
	ck_assert_ptr_eq(r, NULL);

	/* TODO: check statistics */
}
END_TEST

static Suite * lib_suite()
{
	Suite * s = suite_create("Lib");

	TCase * tc = tcase_create("StartStop");
	tcase_add_test_raise_signal(tc, test_start_fail, SIGABRT);
	tcase_add_test(tc, test_start_stop);
	tcase_add_test(tc, test_deviceId);
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
