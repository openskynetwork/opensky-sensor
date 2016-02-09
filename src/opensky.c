/* Copyright (c) 2015-2016 OpenSky Network <engel at opensky-network dot org> */

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

__attribute__((visibility("default")))
void OPENSKY_configure()
{

}

__attribute__((visibility("default")))
void OPENSKY_start()
{
	COMP_register(&BUF_comp, NULL);
	COMP_register(&NET_comp, NULL);
	COMP_register(&RELAY_comp, NULL);

	COMP_initAll();
	COMP_startAll();
}

__attribute__((visibility("default")))
void OPENSKY_stop()
{
	COMP_stopAll();
	COMP_destructAll();
}

static inline void append(uint8_t ** buf, uint8_t c)
{
	if ((*(*buf)++ = c) == 0x1a)
		*(*buf)++ = 0x1a;
}

static inline void encode(uint8_t ** buf, const uint8_t * src, size_t len)
{
	while (len--)
		append(buf, *src++);
}

__attribute__((visibility("default")))
void OPENSKY_frame(const struct OPENSKY_Frame * frame)
{
	if (FILTER_filter((const struct ADSB_Frame*)frame))
		return;

	struct ADSB_Frame * out = BUF_newFrame();
	assert(out);
	memcpy(out, frame, sizeof * frame);

	uint8_t * buf = out->raw;
	buf[0] = '\x1a';
	buf[1] = frame->frameType + '1';

	uint8_t * ptr = buf + 2;
	uint64_t mlat = htobe64(frame->mlat);
	encode(&ptr, ((uint8_t*)&mlat) + 2, 6);

	append(&ptr, frame->siglevel);
	encode(&ptr, (const uint8_t*)frame->payload, frame->payloadLen);

	out->raw_len = ptr - buf;

	BUF_commitFrame(out);
}
