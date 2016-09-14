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
#include "../buffer.h"
#include "../component.h"
#include "../network.h"
#include "../relay.h"
#include "../filter.h"
#include "../util.h"
#include "../cfgfile.h"
#include "../log.h"
#include "../gps.h"
#include "../tb.h"
#include "../openskytypes.h"

extern "C" {

void INPUT_reconfigure() {}

}

namespace OpenSky {

static const char PFX[] = "OpenSky";

static inline size_t encode(uint8_t * out, const uint8_t * in, size_t len);

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
	out->raw_len = encode(out->raw + 2, msg, msgLen) + 2;

	BUF_commitFrame(out);
}

void setGpsPosition(double latitude, double longitude, double altitude)
{
	GPS_setPosition(latitude, longitude, altitude);
}

#pragma GCC visibility pop

/** Escape all \x1a characters in a packet to comply with the beast frame
 *   format.
 * \param out output buffer, must be large enough (twice the length)
 * \param in input buffer
 * \param len input buffer length
 * \note input and output buffers may not overlap
 */
static inline size_t encode(uint8_t * __restrict out,
	const uint8_t * __restrict in, size_t len)
{
	if (unlikely(!len))
		return 0;

	uint8_t * ptr = out;
	const uint8_t * end = in + len;

	/* first time: search for escape from in up to len */
	const uint8_t * esc = (uint8_t*) memchr(in, '\x1a', len);
	while (true) {
		if (unlikely(esc)) {
			/* if esc found: copy up to (including) esc */
			memcpy(ptr, in, esc + 1 - in);
			ptr += esc + 1 - in;
			in = esc; /* important: in points to the esc now */
		} else {
			/* no esc found: copy rest, fast return */
			memcpy(ptr, in, end - in);
			return ptr + (end - in) - out;
		}
		if (likely(in + 1 != end)) {
			esc = (uint8_t*) memchr(in + 1, '\x1a', end - in - 1);
		} else {
			/* nothing more to do, but the last \x1a is still to be copied.
			 * instead of setting esc = NULL and re-iterating, we do things
			 * faster here.
			 */
			*ptr = '\x1a';
			return ptr + 1 - out;
		}
	}
}

}
