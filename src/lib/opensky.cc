
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

extern "C" {
struct CFG_Config CFG_config;
}

namespace OpenSky {

static std::ostream * msgLog = NULL;
static std::ostream * errLog = NULL;

static inline size_t encode(uint8_t * out, const uint8_t * in, size_t len);

static bool configured = false;
static bool running = false;

static GpsTimeStatus_t gpsTimeStatus = GpsTimeInvalid;

#pragma GCC visibility push(default)

void configure()
{
	strncpy(CFG_config.net.host, "collector.opensky-network.org",
		sizeof CFG_config.net.host);
	CFG_config.net.port = 10004;
	CFG_config.net.reconnectInterval = 10;
	CFG_config.net.timeout = 1500;

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

	CFG_config.dev.serialSet = UTIL_getSerial(&CFG_config.dev.serial);

	configured = true;

	FILTER_init();
}

void setLogStreams(std::ostream & msgLog, std::ostream & errLog)
{
	OpenSky::msgLog = &msgLog;
	OpenSky::errLog = &errLog;
}

void enable()
{
	if (unlikely(!configured))
		error(EXIT_FAILURE, 0, "OpenSky: call OPENSKY_configure first");

	FILTER_reset();
	FILTER_setSynchronized(gpsTimeStatus != GpsTimeInvalid);

	COMP_register(&BUF_comp, NULL);
	COMP_register(&NET_comp, NULL);
	COMP_register(&RELAY_comp, NULL);

	COMP_setSilent(true);

	COMP_initAll();
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
	if (unlikely(!configured))
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

	enum ADSB_FRAME_TYPE frameType = (enum ADSB_FRAME_TYPE)(messageType - '1');

	if (!FILTER_filter(frameType, msg[7]))
		return;

	struct ADSB_RawFrame * out = BUF_newFrame();
	assert(out);
	out->raw[0] = '\x1a';
	out->raw[1] = (uint8_t)messageType;
	out->raw_len = encode(out->raw + 2, msg, msgLen) + 2;

	BUF_commitFrame(out);
}

#pragma GCC visibility pop

static inline size_t encode(uint8_t * out, const uint8_t * in, size_t len)
{
	if (unlikely(!len))
		return 0;

	uint8_t * ptr = out;

	/* first time: search for escape from in up to len */
	const uint8_t * esc = (uint8_t*)memchr(in, '\x1a', len);
	--len;
	while (true) {
		if (unlikely(esc)) {
			/* if esc found: copy up to (including) esc */
			memcpy(ptr, in, esc + 1 - in);
			ptr += esc + 1 - in;
			len -= esc - in;
			in = esc; /* important: in points to the esc now */
		} else {
			/* no esc found: copy rest, fast return */
			memcpy(ptr, in, len + 1);
			return ptr + len + 1 - out;
		}
		if (likely(len)) {
			esc = (uint8_t*)memchr(in + 1, '\x1a', len);
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
