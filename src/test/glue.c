/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "util/serial_eth.h"

enum SERIAL_RETURN SERIAL_ETH_getSerial(uint32_t * serial)
{
	if (serial)
		*serial = 0xdeadbeef;
	return true;
}
