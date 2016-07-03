#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "opensky.hh"
#include <buffer.h>
#include <component.h>
#include <network.h>
#include <relay.h>
#include <filter.h>
#include <assert.h>
#include <string.h>
#include <util.h>
#include <cfgfile.h>
#include <error.h>
#include <stdlib.h>
#include <MessageTypes.h>
#include <log.h>
#include <gps.h>
#include <tb.h>

extern "C" {
struct CFG_Config CFG_config;

bool GPS_getRawPosition(struct GPS_RawPosition * pos)
{
	return false;
}

void GPS_setNeedPosition()
{

}

void ADSB_reconfigure()
{

}

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
	CFG_config.dev.serialSet = false;
	configure();

	FILTER_init();

	COMP_register(&BUF_comp, NULL);
	COMP_register(&NET_comp, NULL);
	COMP_register(&RELAY_comp, NULL);
	COMP_register(&TB_comp, NULL);

	COMP_setSilent(true);

	COMP_initAll();
}

void configure()
{
#ifdef DEVELOPMENT
	strncpy(CFG_config.net.host, "localhost", sizeof CFG_config.net.host);
#else
	strncpy(CFG_config.net.host, "collector.opensky-network.org",
	    sizeof CFG_config.net.host);
#endif
	CFG_config.net.port = 10004;
	CFG_config.net.reconnectInterval = 10;
	CFG_config.net.timeout = 1500; // TODO: increase

	CFG_config.recv.modeSLongExtSquitter = true;
	CFG_config.recv.syncFilter = true;

	CFG_config.buf.history = false;
	CFG_config.buf.statBacklog = 200;
	CFG_config.buf.dynBacklog = 1000;
	CFG_config.buf.dynIncrement = 1080;
	CFG_config.buf.gcEnabled = false;
	CFG_config.buf.gcInterval = 120;
	CFG_config.buf.gcLevel = 2;

	CFG_config.stats.enabled = false;

	if (!CFG_config.dev.serialSet) { // TODO: we need something better here
		CFG_config.dev.serialSet = UTIL_getSerial("eth0",
		    &CFG_config.dev.serial);
		if (!CFG_config.dev.serialSet) {
			LOG_log(LOG_LEVEL_ERROR, PFX, "No serial number configured");
		} else {
			configured = true;
		}
	}
}

// TODO: race between disable and output_message
//TODO: message log levels -> exit or no exit

void enable()
{
	if (unlikely(!configured)) {
		LOG_log(LOG_LEVEL_ERROR, PFX,
		    "Feeder could not be initialized properly");
		return;
	}

	FILTER_reset();
	FILTER_setSynchronized(gpsTimeStatus != GpsTimeInvalid);

	BUF_flush();
	COMP_startAll();

	running = true;
}

void disable()
{
	if (running) {
		COMP_stopAll();
		COMP_destructAll();

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

	enum ADSB_FRAME_TYPE frameType = (enum ADSB_FRAME_TYPE) (messageType - '1');

	if (!FILTER_filter(frameType, msg[7]))
		return;

	struct ADSB_RawFrame * out = BUF_newFrame();
	assert(out);
	out->raw[0] = '\x1a';
	out->raw[1] = (uint8_t) messageType;
	out->raw_len = encode(out->raw + 2, msg, msgLen) + 2;

	BUF_commitFrame(out);
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
