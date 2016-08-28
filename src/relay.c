/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdbool.h>
#include "relay.h"
#include "openskytypes.h"
#include "buffer.h"
#include "network.h"
#include "cfgfile.h"
#include "threads.h"
#include "util.h"

static void mainloop();

struct Component RELAY_comp = {
	.description = "RELAY",
	.main = &mainloop
};

static void cleanup(struct OPENSKY_RawFrame * frame)
{
	BUF_putFrame(frame);
}

static void mainloop()
{
	while (true) {
		/* synchronize with the network (i.e. wait for a connection) */
		NET_waitConnected();

		/* Now we have a new connection to the server */

		if (!CFG_config.buf.history) /* flush buffer if history is disabled */
			BUF_flush();

		bool success;
		do {
			/* read a frame from the buffer */
			const struct OPENSKY_RawFrame * frame =
				BUF_getFrameTimeout(CFG_config.net.timeout);
			if (!frame) {
				/* timeout */
				success = NET_sendTimeout();
			} else {
				CLEANUP_PUSH(&cleanup, frame);
				/* got a message */
				success = NET_sendFrame(frame);
				if (likely(success))
					BUF_releaseFrame(frame);
				else
					BUF_putFrame(frame);
				CLEANUP_POP0();
			}
		} while(likely(success));
		/* if sending failed, synchronize with the network */
	}
}

