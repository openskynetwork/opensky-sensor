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
#include <adsb.h>
#include <inttypes.h>
#include <stdio.h>
#include <buffer.h>
#include <network.h>
#include <unistd.h>
#include <cfgfile.h>
#include <statistics.h>
#include <string.h>
#include <tb.h>

typedef void*(*PTHREAD_FN)(void*);

static void mainloop(struct CFG_Config * cfg);

#if defined(DEVELOPMENT) && !defined(SYSCONFDIR)
#define SYSCONFDIR "."
#endif

int main(int argc, char * argv[])
{
	/* force flushing of stdout and stderr on newline */
	setlinebuf(stdout);

	struct CFG_Config config;
	/* read & check configuration */
	CFG_read(SYSCONFDIR "/openskyd.conf", &config);

	if (config.wd.enabled || config.fpga.configure) {
		/* initialize GPIO subsystem */
		GPIO_init();
	}

	/* initialize and start statistics */
	if (config.stats.enabled) {
		STAT_init(&config.stats);
		pthread_t stats;
		if (pthread_create(&stats, NULL, (PTHREAD_FN)&STAT_main, NULL))
			error(-1, errno, "Could not create statistics thread");
	}

	/* Watchdog: initialize and start */
	if (config.wd.enabled) {
		WATCHDOG_init();
		pthread_t watchdog;
		if (pthread_create(&watchdog, NULL, (PTHREAD_FN)&WATCHDOG_main, NULL))
			error(-1, errno, "Could not create watchdog thread");
	}

	/* Talkback: configure restart */
	TB_init(argv);

	/* FPGA: initialize and reprogram */
	if (config.fpga.configure) {
		FPGA_init();
		FPGA_program(&config.fpga);
	}

	/* Buffer: initialize buffer, setup garbage collection and filtering */
	BUF_init(&config.buf);
	if (config.buf.gcEnabled) {
		pthread_t buf;
		if (pthread_create(&buf, NULL, (PTHREAD_FN)&BUF_main, NULL))
			error(-1, errno, "Could not start buffering garbage collector");
	}

	/* Network: configure network */
	NET_init(&config.net, config.dev.serial);

	/* ADSB: initialize and setup receiver */
	ADSB_init(&config.adsb);
	enum ADSB_FRAME_TYPE frameFilter = ADSB_FRAME_TYPE_NONE;
	if (config.adsb.modeAC)
		frameFilter |= ADSB_FRAME_TYPE_MODE_AC;
	if (config.adsb.modeSShort)
		frameFilter |= ADSB_FRAME_TYPE_MODE_S_SHORT;
	if (config.adsb.modeSLong)
		frameFilter |= ADSB_FRAME_TYPE_MODE_S_LONG;
	ADSB_setFilter(frameFilter);
	/* for the moment, only extended squitter are relevant */
	ADSB_setFilterLong(config.adsb.modeSLongExtSquitter ?
		ADSB_LONG_FRAME_TYPE_EXTENDED_SQUITTER_ALL :
		ADSB_LONG_FRAME_TYPE_ALL);
	/* relay frames only if they're GPS timestamped */
	ADSB_setSynchronizationFilter(true);

	/* Network: start network mainloop */
	pthread_t net;
	if (pthread_create(&net, NULL, (PTHREAD_FN)&NET_main, NULL))
		error(-1, errno, "Could not create tb main loop");

	/* TB: start talkback mainloop */
	pthread_t tb;
	if (pthread_create(&tb, NULL, (PTHREAD_FN)&TB_main, NULL))
		error(-1, errno, "Could not create tb main loop");

	/* ADSB: start receiver mainloop */
	pthread_t adsb;
	if (pthread_create(&adsb, NULL, (PTHREAD_FN)&ADSB_main, NULL))
		error(-1, errno, "Could not create adsb main loop");

	mainloop(&config);

	return 0;
}

static void mainloop(struct CFG_Config * config)
{
	while (1) {
		/* synchronize with the network (i.e. wait for a connection) */
		NET_sync_send();

		/* Now we have a new connection to the server */

		if (!config->buf.history) /* flush buffer if history is disabled */
			BUF_flush();

		bool success;
		do {
			/* read a frame from the buffer */
			const struct ADSB_Frame * frame =
				BUF_getFrameTimeout(config->net.timeout);
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
