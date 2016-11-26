/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#include "util/serial_eth.h"

enum SERIAL_RETURN SERIAL_ETH_getSerial(uint32_t * serial)
{
	if (serial)
		*serial = 0xdeadbeef;
	return true;
}
