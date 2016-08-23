/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <check.h>
#include <gps.h>
#include <gps/gps_parser.h>
#include <endec.h>
#include <stdlib.h>

static bool sendPositionCalled;
static bool sendPositionRet;
static struct GPS_RawPosition sendPositionPos;

void setup()
{
	sendPositionCalled = false;
	sendPositionRet = true;
	GPS_reset();
}

bool NET_sendPosition(const struct GPS_RawPosition * pos)
{
	sendPositionCalled = true;
	memcpy(&sendPositionPos, pos, sizeof sendPositionPos);
	return sendPositionRet;
}

static void getRawPos(double latitude, double longitude, double altitude,
	struct GPS_RawPosition * pos)
{
	ENDEC_fromdouble(latitude, (uint8_t*)&pos->latitude);
	ENDEC_fromdouble(longitude, (uint8_t*)&pos->longitute);
	ENDEC_fromdouble(altitude, (uint8_t*)&pos->altitude);
}

static void setAndGetRawPos(double latitude, double longitude, double altitude,
	struct GPS_RawPosition * pos)
{
	getRawPos(latitude, longitude, altitude, pos);
	GPS_setPosition(latitude, longitude, altitude);
}

START_TEST(test_get_position_without_pos)
{
	struct GPS_RawPosition pos;
	bool hasPosition = GPS_getRawPosition(&pos);
	ck_assert(!hasPosition);
}
END_TEST

START_TEST(test_get_position_with_pos)
{
	struct GPS_RawPosition pos;
	struct GPS_RawPosition rawPos;

	setAndGetRawPos(10.0, 20.0, 100.0, &rawPos);

	bool hasPosition = GPS_getRawPosition(&pos);
	ck_assert(hasPosition);

	ck_assert(!memcmp(&pos, &rawPos, sizeof pos));
}
END_TEST

START_TEST(test_gps_nosendWithoutNeed)
{
	GPS_setPosition(10.0, 20.0, 100.0);

	ck_assert(!sendPositionCalled);
}
END_TEST

START_TEST(test_gps_sendWithNeedOnce)
{
	struct GPS_RawPosition rawPos;

	GPS_setNeedPosition();
	setAndGetRawPos(10.0, 20.0, 100.0, &rawPos);
	ck_assert(sendPositionCalled);

	ck_assert(!memcmp(&rawPos, &sendPositionPos, sizeof rawPos));

	sendPositionCalled = false;
	GPS_setPosition(10.0, 20.0, 100.0);
	ck_assert(!sendPositionCalled);
}
END_TEST

START_TEST(test_gps_sendWithNeedUntilSuccess)
{
	struct GPS_RawPosition rawPos;

	sendPositionRet = false;
	GPS_setNeedPosition();
	setAndGetRawPos(10.0, 20.0, 100.0, &rawPos);
	ck_assert(sendPositionCalled);
	ck_assert(!memcmp(&rawPos, &sendPositionPos, sizeof rawPos));

	sendPositionCalled = false;
	GPS_setPosition(10.0, 20.0, 100.0);
	ck_assert(sendPositionCalled);
	ck_assert(!memcmp(&rawPos, &sendPositionPos, sizeof rawPos));

	sendPositionCalled = false;
	sendPositionRet = true;
	GPS_setPosition(10.0, 20.0, 100.0);
	ck_assert(sendPositionCalled);
	ck_assert(!memcmp(&rawPos, &sendPositionPos, sizeof rawPos));

	sendPositionCalled = false;
	GPS_setPosition(10.0, 20.0, 100.0);
	ck_assert(!sendPositionCalled);
}
END_TEST

START_TEST(test_gps_resendAfterReset)
{
	struct GPS_RawPosition rawPos;

	GPS_setNeedPosition();
	setAndGetRawPos(10.0, 20.0, 100.0, &rawPos);
	ck_assert(sendPositionCalled);
	ck_assert(!memcmp(&rawPos, &sendPositionPos, sizeof rawPos));

	sendPositionCalled = false;
	GPS_reset();

	GPS_setNeedPosition();
	GPS_setPosition(10.0, 20.0, 100.0);
	ck_assert(sendPositionCalled);
	ck_assert(!memcmp(&rawPos, &sendPositionPos, sizeof rawPos));
}
END_TEST

START_TEST(test_gps_needMulti)
{
	struct GPS_RawPosition rawPos;

	GPS_setNeedPosition();
	setAndGetRawPos(10.0, 20.0, 100.0, &rawPos);
	ck_assert(sendPositionCalled);
	ck_assert(!memcmp(&rawPos, &sendPositionPos, sizeof rawPos));

	sendPositionCalled = false;

	GPS_setNeedPosition();
	GPS_setPosition(10.0, 20.0, 100.0);
	ck_assert(sendPositionCalled);
	ck_assert(!memcmp(&rawPos, &sendPositionPos, sizeof rawPos));
}
END_TEST

static Suite * gps_suite()
{
	Suite * s = suite_create("GPS");

	TCase * tc = tcase_create("setget");
	tcase_add_checked_fixture(tc, &setup, NULL);
	tcase_add_test(tc, test_get_position_without_pos);
	tcase_add_test(tc, test_get_position_with_pos);
	suite_add_tcase(s, tc);

	tc = tcase_create("need");
	tcase_add_checked_fixture(tc, &setup, NULL);
	tcase_add_test(tc, test_gps_nosendWithoutNeed);
	tcase_add_test(tc, test_gps_sendWithNeedOnce);
	tcase_add_test(tc, test_gps_sendWithNeedUntilSuccess);
	tcase_add_test(tc, test_gps_resendAfterReset);
	tcase_add_test(tc, test_gps_needMulti);
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

