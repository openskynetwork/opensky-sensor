#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <relay.h>
#include <stdbool.h>
#include <adsb.h>
#include <buffer.h>
#include <network.h>
#include <cfgfile.h>
#include <threads.h>

static void mainloop();

struct Component RELAY_comp = {
	.description = "RELAY",
	.main = &mainloop
};

static void cleanup(struct ADSB_Frame * frame)
{
	BUF_putFrame(frame);
}

static void mainloop()
{
	while (true) {
		/* synchronize with the network (i.e. wait for a connection) */
		NET_sync_send();

		/* Now we have a new connection to the server */

		if (!CFG_config.buf.history) /* flush buffer if history is disabled */
			BUF_flush();

		bool success;
		do {
			/* read a frame from the buffer */
			const struct ADSB_Frame * frame =
				BUF_getFrameTimeout(CFG_config.net.timeout);
			if (!frame) {
				/* timeout */
				success = NET_sendTimeout();
			} else {
				CLEANUP_PUSH(&cleanup, frame);
				/* got a message */
				success = NET_sendFrame(frame);
				if (success)
					BUF_releaseFrame(frame);
				else
					BUF_putFrame(frame);
				CLEANUP_POP0();
			}
		} while(success);
		/* if sending failed, synchronize with the network */
	}
}

