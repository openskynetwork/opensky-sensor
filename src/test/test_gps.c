/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <check.h>
#include <stdlib.h>
#include <stdbool.h>
#include "core/gps.h"
#include "core/openskytypes.h"
#include "core/network.h"
#include "util/endec.h"

static bool sent;
static bool sendRC;
static uint64_t sentPosition[3];

bool NET_send(const void * buf, size_t len)
{
	ck_assert_uint_eq(len, 2 + sizeof(uint64_t) * 3);

	const uint8_t * b = buf;
	ck_assert_uint_eq(b[0], OPENSKY_SYNC);
	ck_assert_uint_eq(b[1], OPENSKY_FRAME_TYPE_GPS_POSITION);

	memcpy(sentPosition, b + 2, len - 2);

	sent = true;

	return sendRC;
}

void setup()
{
	sent = false;
	sendRC = true;
	GPS_reset();
}

static void getRawPos(double latitude, double longitude, double altitude,
	uint64_t * pos)
{
	ENDEC_fromdouble(latitude, (uint8_t*)pos);
	ENDEC_fromdouble(longitude, (uint8_t*)&pos[1]);
	ENDEC_fromdouble(altitude, (uint8_t*)&pos[2]);
}

static void setAndGetRawPos(double latitude, double longitude, double altitude,
	uint64_t * pos)
{
	getRawPos(latitude, longitude, altitude, pos);
	GPS_setPosition(latitude, longitude, altitude);
}

START_TEST(test_send_position_without_pos)
{
	ck_assert(GPS_sendPosition());
	ck_assert(!sent);
}
END_TEST

START_TEST(test_send_position_with_need)
{
	ck_assert(GPS_sendPosition());
	ck_assert(!sent);

	uint64_t rawPos[3];
	setAndGetRawPos(10.0, 20.0, 100.0, rawPos);

	ck_assert(sent);
	ck_assert(!memcmp(sentPosition, rawPos, sizeof rawPos));
}
END_TEST

START_TEST(test_nosend_position_without_need)
{
	uint64_t rawPos[3];
	setAndGetRawPos(10.0, 20.0, 100.0, rawPos);

	ck_assert(!sent);
}
END_TEST

START_TEST(test_send_position_with_pos)
{
	uint64_t rawPos[3];
	setAndGetRawPos(10.0, 20.0, 100.0, rawPos);
	ck_assert(!sent);

	ck_assert(GPS_sendPosition());
	ck_assert(sent);
	ck_assert(!memcmp(sentPosition, rawPos, sizeof rawPos));
}
END_TEST

START_TEST(test_send_position_until_success)
{
	uint64_t rawPos[3];
	setAndGetRawPos(10.0, 20.0, 100.0, rawPos);
	ck_assert(!sent);

	sendRC = false;
	ck_assert(!GPS_sendPosition());
	ck_assert(sent);

	sent = false;
	ck_assert(!GPS_sendPosition());
	ck_assert(sent);

	sent = false;
	sendRC = true;
	ck_assert(GPS_sendPosition());
	ck_assert(sent);

	ck_assert(!memcmp(sentPosition, rawPos, sizeof rawPos));
}
END_TEST

START_TEST(test_send_position_with_need_reset)
{
	ck_assert(GPS_sendPosition());
	ck_assert(!sent);

	uint64_t rawPos[3];
	setAndGetRawPos(10.0, 20.0, 100.0, rawPos);

	ck_assert(sent);
	ck_assert(!memcmp(sentPosition, rawPos, sizeof rawPos));

	sent = false;
	GPS_setPosition(10.0, 20.0, 100.0);
	ck_assert(!sent);
}
END_TEST

START_TEST(test_send_after_reset)
{
	ck_assert(GPS_sendPosition());
	ck_assert(!sent);

	uint64_t rawPos[3];
	setAndGetRawPos(10.0, 20.0, 100.0, rawPos);
	ck_assert(sent);
	ck_assert(!memcmp(sentPosition, rawPos, sizeof rawPos));

	sent = false;
	GPS_reset();

	ck_assert(GPS_sendPosition());
	ck_assert(!sent);

	GPS_setPosition(10.0, 20.0, 100.0);
	ck_assert(sent);
	ck_assert(!memcmp(sentPosition, rawPos, sizeof rawPos));
}
END_TEST

START_TEST(test_send_without_pos_twice)
{
	ck_assert(GPS_sendPosition());
	ck_assert(!sent);

	ck_assert(GPS_sendPosition());
	ck_assert(!sent);
}
END_TEST

START_TEST(test_send_with_pos_twice)
{
	ck_assert(GPS_sendPosition());
	ck_assert(!sent);

	uint64_t rawPos[3];
	setAndGetRawPos(10.0, 20.0, 100.0, rawPos);
	ck_assert(sent);
	ck_assert(!memcmp(sentPosition, rawPos, sizeof rawPos));

	sent = false;
	ck_assert(GPS_sendPosition());
	ck_assert(sent);
	ck_assert(!memcmp(sentPosition, rawPos, sizeof rawPos));
}
END_TEST

static Suite * gps_suite()
{
	Suite * s = suite_create("GPS");

	TCase * tc = tcase_create("sendOnDemand");
	tcase_add_checked_fixture(tc, &setup, NULL);
	tcase_add_test(tc, test_send_position_without_pos);
	tcase_add_test(tc, test_send_position_with_need);
	tcase_add_test(tc, test_nosend_position_without_need);
	tcase_add_test(tc, test_send_position_with_pos);
	tcase_add_test(tc, test_send_position_until_success);
	tcase_add_test(tc, test_send_position_with_need_reset);
	tcase_add_test(tc, test_send_after_reset);
	tcase_add_test(tc, test_send_without_pos_twice);
	tcase_add_test(tc, test_send_with_pos_twice);
	suite_add_tcase(s, tc);

	return s;
}

int main()
{
	SRunner * sr = srunner_create(gps_suite());
	srunner_set_tap(sr, "-");
	srunner_run_all(sr, CK_NORMAL);
	srunner_free(sr);
	return EXIT_SUCCESS;
}

