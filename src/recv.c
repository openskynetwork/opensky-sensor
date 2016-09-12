/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdbool.h>
#include "recv.h"
#include "input.h"
#include "message.h"
#include "openskytypes.h"
#include "buffer.h"
#include "threads.h"
#include "util.h"
#include "filter.h"

static bool construct();
static void destruct();
static void mainloop();

const struct Component RECV_comp = {
	.description = "RECV",
	.onConstruct = &construct,
	.onDestruct = &destruct,
	.main = &mainloop,
	.dependencies = { &BUF_comp, &INPUT_comp, &FILTER_comp, NULL }
};

static bool construct()
{
	INPUT_init();

	FILTER_init();

	return true;
}

static void destruct()
{
	INPUT_destruct();
}

static void cleanup(struct OPENSKY_RawFrame ** frame)
{
	if (*frame)
		BUF_abortFrame(*frame);
}

static void mainloop()
{
	struct OPENSKY_RawFrame * frame = NULL;
	struct OPENSKY_DecodedFrame decoded;

	CLEANUP_PUSH(&cleanup, &frame);
	while (true) {
		INPUT_connect();

		FILTER_reset();

		frame = BUF_newFrame();
		while (true) {
			bool success = INPUT_getFrame(frame, &decoded);
			if (likely(success)) {
				if (decoded.frameType == OPENSKY_FRAME_TYPE_STATUS &&
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
