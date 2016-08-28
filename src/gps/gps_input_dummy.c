/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <unistd.h>
#include "gps_input.h"

void GPS_INPUT_init()
{
}

void GPS_INPUT_destruct()
{
}

void GPS_INPUT_connect()
{
}

size_t GPS_INPUT_read(uint8_t * buf, size_t bufLen)
{
	while (true) {
		sleep(10);
	}
}

size_t GPS_INPUT_write(const uint8_t * buf, size_t bufLen)
{
	return bufLen;
}
