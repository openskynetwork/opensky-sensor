/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <recv.h>
#include <message.h>
#include <adsb.h>
#include <buffer.h>
#include <threads.h>
#include <util.h>
#include <filter.h>

static bool construct();
static void destruct();
static void mainloop();

struct Component RECV_comp = {
	.description = "RECV",
	.construct = &construct,
	.destruct = &destruct,
	.main = &mainloop
};

static bool construct()
{
	ADSB_init();

	FILTER_init();

	return true;
}

static void destruct()
{
	ADSB_destruct();
}

static void cleanup(struct ADSB_RawFrame ** frame)
{
	if (*frame)
		BUF_abortFrame(*frame);
}

static void mainloop()
{
	struct ADSB_RawFrame * frame = NULL;
	struct ADSB_DecodedFrame decoded;

	CLEANUP_PUSH(&cleanup, &frame);
	while (true) {
		ADSB_connect();

		FILTER_reset();

		frame = BUF_newFrame();
		while (true) {
			bool success = ADSB_getFrame(frame, &decoded);
			if (likely(success)) {
				if (decoded.frameType == ADSB_FRAME_TYPE_STATUS &&
					decoded.mlat != 0)
					FILTER_setSynchronized(true);

				if (FILTER_filter(decoded.frameType, decoded.payload[0])) {
					BUF_commitFrame(frame);
					frame = BUF_newFrame();
				}
			} else {
				BUF_abortFrame(frame);
				frame = NULL;
				break;
			}
		}
	}
	CLEANUP_POP();
}
