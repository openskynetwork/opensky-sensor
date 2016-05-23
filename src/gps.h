/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#ifndef _HAVE_GPS_H
#define _HAVE_GPS_H

#include <component.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern struct Component GPS_comp;

enum GPS_TIME_FLAGS {
	GPS_TIME_FLAG_NONE = 0,
	GPS_TIME_FLAG_NON_TEST_MODE = 1,
	GPS_TIME_FLAG_HAS_GPS_TIME = 2,
	GPS_TIME_FLAG_HAS_WEEK = 4,
	GPS_TIME_FLAG_HAS_OFFSET = 8,
	GPS_TIME_FLAG_IS_UTC = 16,
	GPS_TIME_FLAG_HAS_TIME =
		GPS_TIME_FLAG_NON_TEST_MODE |
		GPS_TIME_FLAG_HAS_GPS_TIME |
		GPS_TIME_FLAG_HAS_WEEK
};

struct GPS_RawPosition {
	uint64_t latitude;
	uint64_t longitute;
	uint64_t altitude;
};

bool GPS_getRawPosition(struct GPS_RawPosition * rawPos);

void GPS_setNeedPosition();

#ifdef __cplusplus
}
#endif

#endif
