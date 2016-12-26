/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdbool.h>
#include "relay.h"
#include "openskytypes.h"
#include "buffer.h"
#include "network.h"
#include "login.h"
#include "beast.h"
#include "util/cfgfile.h"
#include "util/threads.h"
#include "util/util.h"

/** Configuration: timeout to wait for a new message in milliseconds.
 * This is also the keep-alive interval. */
static uint32_t cfgTimeout;

static void mainloop();

/** Configuration: Descriptor */
static const struct CFG_Section cfg = {
	.name = "NETWORK",
	.n_opt = 1,
	.options = {
		{
			.name = "Timeout",
			.type = CFG_VALUE_TYPE_INT,
			.var = &cfgTimeout,
			.def = { .integer = 1500 }
		}
	}
};

/** Component: Descriptor */
const struct Component RELAY_comp = {
	.description = "RELAY",
	.main = &mainloop,
	.config = &cfg,
	.dependencies = { &NET_comp, &BUF_comp, NULL }
};

/** Cleanup routine: put a frame back into the queue upon cancellation */
static void cleanup(struct OPENSKY_RawFrame * frame)
{
	BUF_putFrame(frame);
}

/** Send a frame to the OpenSky Network
 * @param frame frame to be sent
 * @return true if sending succeeded
 */
static inline bool sendFrame(const struct OPENSKY_RawFrame * frame)
{
	return NET_send(frame->raw, frame->rawLen);
}

/** Send a keep-alive to the OpenSky Network
 * @return true if sending succeeded
 */
static inline bool sendKeepalive()
{
	/* Build message */
	char buf[] = { BEAST_SYNC, BEAST_TYPE_KEEP_ALIVE };
	/* send it */
	return NET_send(buf, sizeof buf);
}

/** Main loop: get a frame from the queue and send it */
static void mainloop()
{
	while (true) {
		/* synchronize with the network (i.e. wait for a connection) */
		NET_waitConnected();

		/* here: we have a new connection to the server */

		/* log into the network */
		if (unlikely(!LOGIN_login()))
			continue; /* upon failure: wait again */

		/* flush buffer if history is disabled */
		BUF_flushUnlessHistoryEnabled();

		/* TODO: we could speed this up by setting the scope of the cleanup
		 * around the loop */
		bool success;
		do {
			/* read a frame from the buffer */
			const struct OPENSKY_RawFrame * frame =
				BUF_getFrameTimeout(cfgTimeout);
			if (!frame) {
				/* timeout -> send keep-alive */
				success = sendKeepalive();
			} else {
				CLEANUP_PUSH(&cleanup, frame);
				/* got a message -> send it */
				success = sendFrame(frame);
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

