/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <assert.h>
#include <string.h>
#include <error.h>
#include <stdlib.h>
#include "opensky.hh"
#include "MessageTypes.h"
#include "core/buffer.h"
#include "core/network.h"
#include "core/relay.h"
#include "core/filter.h"
#include "core/gps.h"
#include "core/tb.h"
#include "core/openskytypes.h"
#include "core/beast.h"
#include "util/component.h"
#include "util/util.h"
#include "util/cfgfile.h"
#include "util/log.h"

extern "C" {

void INPUT_reconfigure() {}

}

namespace OpenSky {

static const char PFX[] = "OpenSky";

static bool configured = false;
static bool running = false;

static GpsTimeStatus_t gpsTimeStatus = GpsTimeInvalid;

#pragma GCC visibility push(default)

void init()
{
	COMP_register(&TB_comp);
	COMP_register(&RELAY_comp);
	COMP_register(&FILTER_comp);
	COMP_fixup();

	CFG_loadDefaults();

	GPS_reset();

	configure();

	COMP_setSilent(true);

	COMP_initAll();
}

void configure()
{
	if (!UTIL_getSerial(NULL)) {
		LOG_log(LOG_LEVEL_EMERG, PFX, "No serial number configured");
	} else {
		configured = true;
	}
}

// TODO: race between disable and output_message
//TODO: message log levels -> exit or no exit

void enable()
{
	if (unlikely(!configured)) {
		LOG_log(LOG_LEVEL_EMERG, PFX,
		    "Feeder could not be initialized properly");
		return;
	}

	FILTER_reset();
	FILTER_setSynchronized(gpsTimeStatus != GpsTimeInvalid);
	GPS_reset();

	BUF_flush();
	COMP_startAll();

	running = true;
}

void disable()
{
	if (running) {
		COMP_stopAll();

		running = false;
	}
}

void setGpsTimeStatus(const GpsTimeStatus_t gpsTimeStatus)
{
	OpenSky::gpsTimeStatus = gpsTimeStatus;
	FILTER_setSynchronized(gpsTimeStatus != GpsTimeInvalid);
}

void output_message(const unsigned char * const msg,
    const enum MessageType_T messageType)
{
	if (unlikely(!configured || !running))
		return;

	size_t msgLen;
	switch (messageType) {
	case MessageType_ModeAC:
		msgLen = 7 + 2;
		break;
	case MessageType_ModeSShort:
		msgLen = 7 + 7;
		break;
	case MessageType_ModeSLong:
		msgLen = 7 + 14;
		break;
	default:
		return;
	}

	enum OPENSKY_FRAME_TYPE frameType = (enum OPENSKY_FRAME_TYPE) (messageType - '1');

	if (!FILTER_filter(frameType, msg[7]))
		return;

	struct OPENSKY_RawFrame * out = BUF_newFrame();
	assert(out);
	out->raw[0] = '\x1a';
	out->raw[1] = (uint8_t) messageType;
	out->raw_len = BEAST_encode(out->raw + 2, msg, msgLen) + 2;

	BUF_commitFrame(out);
}

void setGpsPosition(double latitude, double longitude, double altitude)
{
	GPS_setPosition(latitude, longitude, altitude);
}

}
