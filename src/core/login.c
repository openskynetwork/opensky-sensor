/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "login.h"
#include "network.h"
#include "beast.h"
#include "gps.h"
#include "util/util.h"
#include "util/log.h"

#define PFX "LOGIN"

static bool sendSerial();

bool LOGIN_login()
{
	return sendSerial() && GPS_sendPosition();
}

static bool sendSerial()
{
	uint8_t buf[10] = { BEAST_SYNC, BEAST_TYPE_SERIAL };

	uint32_t serialno;
	if (!UTIL_getSerial(&serialno)) {
		LOG_log(LOG_LEVEL_ERROR, PFX, "No serial number configured");
	}

	union {
		uint8_t ca[4];
		uint32_t serialbe;
	} serial;
	serial.serialbe = htobe32(serialno);
	size_t len = 2 + BEAST_encode(buf + 2, serial.ca, sizeof serial.ca);

	return NET_send(buf, len);
}



