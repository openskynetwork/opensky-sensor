/* Copyright (c) 2015-2018 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <assert.h>
#include <string.h>
#include <error.h>
#include <stdlib.h>
#include "../gplib/opensky.hh"
#include "core/buffer.h"
#include "core/network.h"
#include "core/relay.h"
#include "core/filter.h"
#include "core/gps.h"
#include "core/tb.h"
#include "core/openskytypes.h"
#include "core/beast.h"
#include "core/login.h"
#include "core/serial.h"
#include "util/component.h"
#include "util/util.h"
#include "util/cfgfile.h"
#include "util/log.h"
#include "util/serial_eth.h"

namespace OpenSky {

/** Serial number */
static uint32_t serial;

}

extern "C" {

/** Reconfigure the input from the filter configuration */
void INPUT_reconfigure() {
}

/** Get serial number: generated from MAC.
 * @param serial buffer for serial number
 * @return status of serial number
 */
enum SERIAL_RETURN SERIAL_getSerial(uint32_t * serial) {
	*serial = OpenSky::serial;
	return SERIAL_RETURN_SUCCESS;
}

}

namespace OpenSky {

/** Component: Prefix */
static const char PFX[] = "OpenSky";

/** Library state: configure called */
static bool configured = false;
/** Library state: currently feeding */
static bool running = false;

#pragma GCC visibility push(default)

void init() {
	COMP_register(&TB_comp);
	COMP_register(&RELAY_comp);
	COMP_register(&FILTER_comp);
	COMP_fixup();

	CFG_loadDefaults();

	GPS_reset();

	COMP_setSilent(true);

	COMP_initAll();
}

void configure(OPENSKY_DEVICE_TYPE deviceType, uint32_t serialNumber) {
	LOGIN_setDeviceType(deviceType);

	OpenSky::serial = serialNumber;

	configured = true;
}

// TODO: race between disable and output_message

void enable() {
	if (unlikely(!configured)) {
		LOG_log(LOG_LEVEL_EMERG, PFX,
				"Feeder was not be initialized properly, did you call configure?");
		return;
	}

	FILTER_reset();
	FILTER_setSynchronized(true);
	GPS_resetNeedFlag();

	BUF_flush();
	COMP_startAll();

	running = true;
}

void disable() {
	if (running) {
		COMP_stopAll();

		running = false;
	}
}

void setGpsPosition(double latitude, double longitude, double altitude) {
	GPS_setPosition(latitude, longitude, altitude);
	GPS_setHasFix(true);
}

void unsetGpsPosition() {
	GPS_setHasFix(false);
}

void submitFrame(std::uint64_t timestamp, bool timingUTC, bool timingSync,
		std::int8_t rssi, const uint8_t * payload, std::size_t payloadLen) {
	if (unlikely(!configured || !running))
		return;

	enum OPENSKY_FRAME_TYPE frameType;

	switch (payloadLen) {
	case 7:
		frameType = OPENSKY_FRAME_TYPE_MODE_S_SHORT;
		break;
	case 14:
		frameType = OPENSKY_FRAME_TYPE_MODE_S_LONG;
		break;
	default:
		return;
	}

	if (!timingUTC)
		return;

	if (!FILTER_filter(frameType, payload[0]))
		return;

	std::uint64_t secondsOfDay = (timestamp / UINT64_C(1000000000)) % (24u * 3600u);
	std::uint64_t nanoseconds = timestamp % UINT64_C(1000000000);
	std::uint64_t mlat = htobe64((secondsOfDay << 30) | nanoseconds) >> 16;

	struct OPENSKY_RawFrame * out = BUF_newFrame();
	assert(out != nullptr);
	out->raw[0] = BEAST_SYNC;
	out->raw[1] = static_cast<std::uint8_t>(frameType);
	std::size_t len = 2;
	len += BEAST_encode(out->raw + len, reinterpret_cast<std::uint8_t*>(&mlat),
			6);
	len += BEAST_encode(out->raw + len, reinterpret_cast<std::uint8_t*>(&rssi),
			sizeof rssi);
	len += BEAST_encode(out->raw + len, payload, payloadLen);
	out->rawLen = len;

	BUF_commitFrame(out);
}

#pragma GCC visibility pop

}
