/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdbool.h>
#include "recv.h"
#include "input.h"
#include "openskytypes.h"
#include "buffer.h"
#include "filter.h"
#include "util/threads.h"
#include "util/util.h"

static void mainloop();

/** Component: Descriptor */
const struct Component RECV_comp = {
	.description = "RECV",
	.main = &mainloop,
	.dependencies = { &BUF_comp, &INPUT_comp, &FILTER_comp, NULL }
};

/** Cleanup routine: put frame back into pool upon cancellation
 * @param frame pointer to current pointer to frame. Must be done this way,
 *  since the argument is always the initial value.
 */
static void cleanup(struct OPENSKY_RawFrame ** frame)
{
	if (*frame)
		BUF_abortFrame(*frame);
}

/** Cleanup routine: disconnect the input upon cancellation */
static void cleanupInput()
{
	INPUT_disconnect();
}

/** Main loop: get a frame from the pool, fill it and buffer it */
static void mainloop()
{
	struct OPENSKY_RawFrame * frame = NULL;
	struct OPENSKY_DecodedFrame decoded;

	CLEANUP_PUSH(&cleanup, &frame);
	while (true) {
		INPUT_connect();

		CLEANUP_PUSH(&cleanupInput, NULL);

		FILTER_reset(); /* upon reconnection of the input: reset filter state */

		frame = BUF_newFrame();
		while (true) {
			/* read a frame from the input */
			bool success = INPUT_getFrame(frame, &decoded);
			if (likely(success)) {
				/* set filter state */
				if (unlikely(decoded.frameType == OPENSKY_FRAME_TYPE_STATUS &&
					decoded.mlat != 0))
					FILTER_setSynchronized(true);

				/* apply filter */
				if (FILTER_filter(decoded.frameType, decoded.payload[0])) {
					/* not filtered -> buffer it */
					BUF_commitFrame(frame);
					/* get next frame from pool */
					frame = BUF_newFrame();
				}
			} else {
				/* input failed, put frame back into the pool */
				BUF_abortFrame(frame);
				frame = NULL;
				break;
			}
		}
		CLEANUP_POP();
	}
	CLEANUP_POP();
}
