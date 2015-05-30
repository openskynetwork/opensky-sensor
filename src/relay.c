#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <relay.h>
#include <stdbool.h>
#include <message.h>
#include <buffer.h>
#include <network.h>
#include <cfgfile.h>
#include <threads.h>

static void mainloop();

struct Component RELAY_comp = {
	.description = "RELAY",
	.main = &mainloop
};

static void cleanup(struct MSG_Message * msg)
{
	if (msg)
		BUF_putMessage(msg);
}

static void mainloop()
{
	while (RELAY_comp.run) {
		/* synchronize with the network (i.e. wait for a connection) */
		NET_sync_send();

		/* Now we have a new connection to the server */

		if (!CFG_config.buf.history) /* flush buffer if history is disabled */
			BUF_flush();

		bool success;
		do {
			/* read a message from the buffer */
			const struct MSG_Message * msg =
				BUF_getMessageTimeout(CFG_config.net.timeout);
			if (!msg) {
				/* timeout */
				success = NET_sendTimeout();
			} else {
				CLEANUP_PUSH(&cleanup, msg);
				/* got a message */
				success = NET_sendFrame(&msg->adsb);
				if (success)
					BUF_releaseMessage(msg);
				else
					BUF_putMessage(msg);
				CLEANUP_POP0();
			}
		} while(success && RELAY_comp.run);
		/* if sending failed, synchronize with the network */
	}
}

