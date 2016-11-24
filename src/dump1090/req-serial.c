/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "core/serial.h"
#include "core/beast.h"
#include "core/network.h"
#include "core/tb.h"
#include "util/threads.h"
#include "util/cfgfile.h"
#include "util/endec.h"
#include "req-serial.h"

#define PFX "SERIAL"

static bool construct();
static bool sendSerialRequest();

static pthread_mutex_t serialMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t serialReqCond = PTHREAD_COND_INITIALIZER;
static bool hasSerial;
static uint32_t serialNumber;

static const struct CFG_Section cfg =
{
	.name = "Device",
	.n_opt = 1,
	.options = {
		{
			.name = "serial",
			.type = CFG_VALUE_TYPE_INT,
			.var = &serialNumber,
			.given = &hasSerial,
		}
	}
};

const struct Component SERIAL_comp =
{
	.description = PFX,

	.onConstruct = &construct,

	.config = &cfg,

	.dependencies = { &TB_comp, NULL}
};

bool SERIAL_getSerial(uint32_t * serial)
{
	pthread_mutex_lock(&serialMutex);
	if (!hasSerial) {
		if (!sendSerialRequest()) {
			pthread_mutex_unlock(&serialMutex);
			return false;
		}
		while (!hasSerial)
			pthread_cond_wait(&serialReqCond, &serialMutex);

		/* got a new serial number -> save it */
		CFG_writeSection("/var/lib/openskyd/conf.d/10-serial.conf", &cfg);
	}
	*serial = serialNumber;
	pthread_mutex_unlock(&serialMutex);
	return true;
}

static bool sendSerialRequest()
{
	uint8_t buf[2] = { BEAST_SYNC, BEAST_TYPE_SERIAL_REQ };
	return NET_send(buf, sizeof buf);
}

static void serialResponse(const uint8_t * payload)
{
	pthread_mutex_lock(&serialMutex);
	serialNumber = ENDEC_tou32(payload);
	hasSerial = true;
	pthread_cond_broadcast(&serialReqCond);
	pthread_mutex_unlock(&serialMutex);
}

static bool construct()
{
	TB_register(TB_PACKET_TYPE_SERIAL_RES, 4, &serialResponse);
	return true;
}
