/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "opensky.h"
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

struct CFG_Config CFG_config;

static bool configured = false;
static bool running = false;

__attribute__((visibility("default")))
void OPENSKY_configure()
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

__attribute__((visibility("default")))
void OPENSKY_start()
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

__attribute__((visibility("default")))
void OPENSKY_stop()
{
	if (running) {
		COMP_stopAll();
		COMP_destructAll();

		running = false;
	}
}

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
	uint8_t * esc = memchr(in, '\x1a', len);
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
			esc = memchr(in + 1, '\x1a', len);
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

__attribute__((visibility("default")))
void OPENSKY_frame(const struct OPENSKY_Frame * frame)
{
	if (unlikely(!configured))
		return;

	struct ADSB_Frame * out;
	if (unlikely(frame->payloadLen > ((sizeof out->raw) >> 1) - 9))
		return;

	if (!FILTER_filter((const struct ADSB_Frame*)frame))
		return;

	out = BUF_newFrame();
	assert(out);
	memcpy(out, frame, sizeof * frame);

	uint8_t * raw = out->raw;
	raw[0] = '\x1a';
	raw[1] = frame->frameType + '1';

	uint8_t * ptr = raw + 2;
	uint64_t mlat = htobe64(frame->mlat);
	encode(&ptr, ((uint8_t*)&mlat) + 2, 6);

	append(&ptr, frame->siglevel);
	encode(&ptr, frame->payload, frame->payloadLen);

	out->raw_len = ptr - raw;

	BUF_commitFrame(out);
}
