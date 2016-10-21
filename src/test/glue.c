/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#include "core/network.h"
#include "util/util.h"

bool NET_sendPosition(const struct GPS_RawPosition * position)
{
	return true;
}

bool UTIL_getSerial(uint32_t * serial)
{
	if (serial)
		*serial = 0xdeadbeef;
	return true;
}
