/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#include "util/util.h"

bool UTIL_getSerial(uint32_t * serial)
{
	if (serial)
		*serial = 0xdeadbeef;
	return true;
}
