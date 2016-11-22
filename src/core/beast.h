/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_BEAST_H
#define _HAVE_BEAST_H

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	BEAST_SYNC = '\x1a'
};

enum BEAST_TYPE {
	BEAST_TYPE_MODE_AC = '1',
	BEAST_TYPE_MODE_S_SHORT = '2',
	BEAST_TYPE_MODE_S_LONG = '3',
	BEAST_TYPE_STATUS = '4',

	BEAST_TYPE_SERIAL = '5',
	BEAST_TYPE_KEEP_ALIVE = '6',
	BEAST_TYPE_GPS_POSITION = '7',

	BEAST_TYPE_DEVICE_ID = 65,
	BEAST_TYPE_SERIAL_REQ = 66,
	BEAST_TYPE_USER = 67,
};

size_t BEAST_encode(uint8_t * __restrict out, const uint8_t * __restrict in,
	size_t len);

#ifdef __cplusplus
}
#endif

#endif
