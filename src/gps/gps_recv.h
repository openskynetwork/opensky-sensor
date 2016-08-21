/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_GPS_RECV_H
#define _HAVE_GPS_RECV_H

#include <component.h>
#include <stdint.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

extern struct Component GPS_RECV_comp;

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

/*inline enum GPS_TIME_FLAGS GPS_getTimeFlags()
{
	extern _Atomic enum GPS_TIME_FLAGS GPS_timeFlags;
	return atomic_load_explicit(&GPS_timeFlags, memory_order_relaxed);
}*/

#ifdef __cplusplus
}
#endif

#endif
