#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gpio.h>
#include <watchdog.h>
#include <fpga.h>
#include <pthread.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <buffer.h>
#include <network.h>
#include <unistd.h>
#include <cfgfile.h>
#include <statistics.h>
#include <string.h>
#include <tb.h>
#include <recv.h>

static void mainloop();

#if defined(DEVELOPMENT) && !defined(SYSCONFDIR)
#define SYSCONFDIR "."
#endif

int main(int argc, char * argv[])
{
	/* force flushing of stdout and stderr on newline */
	setlinebuf(stdout);

	/* read & check configuration */
	CFG_read(SYSCONFDIR "/openskyd.conf");

	if (CFG_config.wd.enabled || CFG_config.fpga.configure)
		COMP_register(&GPIO_comp, NULL);
	if (CFG_config.stats.enabled)
		COMP_register(&STAT_comp, NULL);
	if (CFG_config.wd.enabled)
		COMP_register(&WD_comp, NULL);
	if (CFG_config.fpga.configure)
		COMP_register(&FPGA_comp, NULL);
	COMP_register(&BUF_comp, NULL);
	COMP_register(&NET_comp, NULL);
	COMP_register(&TB_comp, argv);
	COMP_register(&RECV_comp, NULL);

	COMP_initAll();

	if (!COMP_startAll())
		return EXIT_FAILURE;

	mainloop();

	COMP_stopAll();
	COMP_destructAll();

	return EXIT_SUCCESS;
}

static void mainloop()
{
	while (1) {
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
				/* got a frame */
				success = NET_sendFrame(frame);
				if (success)
					BUF_releaseFrame(frame);
				else
					BUF_putFrame(frame);
			}
		} while(success);
		/* if sending failed, synchronize with the network */
	}
}
