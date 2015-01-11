
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

typedef void*(*PTHREAD_FN)(void*);

int main()
{
	/* force flushing of stdout and stderr on newline */
	setlinebuf(stdout);

	struct CFG_Config config;
	/* read & check configuration */
	CFG_read("cape.cfg", &config);

	/* initialize GPIO subsystem */
	GPIO_init();


	/* initialize and start statistics */
	if (config.stats.enabled) {
		STAT_init(config.stats.interval);
		pthread_t stats;
		if (pthread_create(&stats, NULL, (PTHREAD_FN)&STAT_main, NULL))
			error(-1, errno, "Could not create watchdog thread");
	}

	/* Watchdog: initialize and start */
	if (config.wd.enabled) {
		WATCHDOG_init();
		pthread_t watchdog;
		if (pthread_create(&watchdog, NULL, (PTHREAD_FN)&WATCHDOG_main, NULL))
			error(-1, errno, "Could not create watchdog thread");
	}

	/* FPGA: initialize and reprogram */
	if (config.fpga.configure) {
		FPGA_init();
		FPGA_program(config.fpga.file, config.fpga.timeout,
			config.fpga.retries);
	}

	/* Buffer: initialize buffer, setup garbage collection and filtering */
	BUF_init(config.buf.statBacklog, config.buf.dynBacklog,
		config.buf.history ? config.buf.dynIncrement : 0);
	BUF_initGC(config.buf.gcInterval, config.buf.gcLevel);
	if (config.buf.gcEnabled) {
		pthread_t buf;
		if (pthread_create(&buf, NULL, (PTHREAD_FN)&BUF_main, NULL))
			error(-1, errno, "Could not start buffering garbage collector");
	}

	/* ADSB: initialize and setup receiver */
	ADSB_init(config.adsb.uart, config.adsb.rts);
	ADSB_setup(config.adsb.crc, config.adsb.fec, config.adsb.frameFilter,
		config.adsb.modeAC, config.adsb.rts, config.adsb.timestampGPS);
	enum ADSB_FRAME_TYPE frameFilter = ADSB_FRAME_TYPE_NONE;
	if (config.adsb.modeAC)
		frameFilter |= ADSB_FRAME_TYPE_MODE_AC;
	if (config.adsb.modeSShort)
		frameFilter |= ADSB_FRAME_TYPE_MODE_S_SHORT;
	if (config.adsb.modeSLong)
		frameFilter |= ADSB_FRAME_TYPE_MODE_S_LONG;
	ADSB_setFilter(frameFilter);
	/* ADSB: start receiver mainloop */
	pthread_t adsb;
	if (pthread_create(&adsb, NULL, (PTHREAD_FN)&ADSB_main, NULL))
		error(-1, errno, "Could not create adsb main loop");

	while (1) {
		/* try to connect to the server */
		while (!NET_connect(config.net.host, config.net.port))
			sleep(config.net.reconnectInterval); /* in case of failure: retry */

		/* send serial number */
		if (!NET_sendSerial(config.dev.serial))
			continue; /* on error: reconnect */

		if (!config.buf.history) /* flush buffer if history is disabled */
			BUF_flush();

		bool success;
		do {
			/* read a frame from the buffer */
			const struct ADSB_Frame * frame =
				BUF_getFrameTimeout(config.net.timeout);
			if (!frame) {
				/* timeout */
				success = NET_sendTimeout();
			} else {
				/* got a frame */
#if 0
				printf("Mode-S long: mlat %15" PRIu64 ", level %+3" PRIi8 ": ",
					frame->mlat, frame->siglevel);
				int i;
				for (i = 0; i < 14; ++i)
					printf("%02x", frame->payload[i]);
				putchar('\n');
#endif
				success = NET_sendFrame(frame);
				if (success)
					BUF_releaseFrame(frame);
				else
					BUF_putFrame(frame);
			}
		} while(success);
		/* if sending failed, reconnect to the server */
	}

	return 0;
}

