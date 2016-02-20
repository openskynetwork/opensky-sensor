
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

extern "C" {
struct CFG_Config CFG_config;
}

namespace OpenSky {

static std::ostream * msgLog = NULL;
static std::ostream * errLog = NULL;

static inline void append(uint8_t ** out, uint8_t c);
static inline void encode(uint8_t ** out, const uint8_t * in, size_t len);

static bool configured = false;
static bool running = false;

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

void output_message(const unsigned char * const msg,
	const enum MessageType_T messageType)
{
	if (unlikely(!configured))
		return;

	struct ADSB_Frame * out;

	if (messageType != MessageType_ModeSLong)
		return;

	out = BUF_newFrame();
	assert(out);
	out->frameType = ADSB_FRAME_TYPE_MODE_S_LONG;
	memcpy(&out->mlat, msg, 6);
	out->mlat = be64toh(out->mlat) >> 16;
	out->siglevel = msg[6];
	out->payloadLen = 14;
	memcpy(out->payload, msg + 7, 14);

	if (!FILTER_filter(out)) {
		BUF_abortFrame(out);
		return;
	}
	uint8_t * raw = out->raw;
	raw[0] = '\x1a';
	raw[1] = out->frameType + '1';

	uint8_t * ptr = raw + 2;
	uint64_t mlat = htobe64(out->mlat);
	encode(&ptr, ((uint8_t*)&mlat) + 2, 6);

	append(&ptr, out->siglevel);
	encode(&ptr, out->payload, out->payloadLen);

	out->raw_len = ptr - raw;

	BUF_commitFrame(out);
}

#pragma GCC visibility pop

static inline void append(uint8_t ** out, uint8_t c)
{
	if ((*(*out)++ = c) == 0x1a)
		*(*out)++ = 0x1a;
}

static inline void encode(uint8_t ** out, const uint8_t * in, size_t len)
{
	if (unlikely(!len))
		return;

	uint8_t * ptr = *out;

	/* first time: search for escape from in up to len */
	uint8_t * esc = (uint8_t*)memchr(in, '\x1a', len);
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
			*out = ptr + len + 1;
			break;
		}
		if (likely(len)) {
			esc = (uint8_t*)memchr(in + 1, '\x1a', len);
		} else {
			/* nothing more to do, but the last \x1a is still to be copied.
			 * instead of setting esc = NULL and re-iterating, we do things
			 * faster here.
			 */
			*ptr = '\x1a';
			*out = ptr + 1;
			break;
		}
	}
}

}
