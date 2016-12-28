/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <unistd.h>
#include "trimble_input.h"

void TRIMBLE_INPUT_register()
{
}

void TRIMBLE_INPUT_init()
{
}

void TRIMBLE_INPUT_disconnect()
{
}

void TRIMBLE_INPUT_connect()
{
}

size_t TRIMBLE_INPUT_read(uint8_t * buf, size_t bufLen)
{
	while (true) {
		sleep(10);
	}
}

size_t TRIMBLE_INPUT_write(const uint8_t * buf, size_t bufLen)
{
	return bufLen;
}
