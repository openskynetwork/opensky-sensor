
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

static bool TEST = false;

typedef void*(*PTHREAD_FN)(void*);

static void mainloop(struct CFG_Config * cfg);
static void mainloop_test(struct CFG_Config * cfg);

#ifdef DEVELOPMENT
#ifndef FWDIR
#define FWDIR "."
#endif
#ifndef SYSCONFDIR
#define SYSCONFDIR "."
#endif
#endif

int main(int argc, char * argv[])
{
	if (argc == 1 && !strcasecmp(argv[0], "--test"))
		TEST = true;

	/* force flushing of stdout and stderr on newline */
	setlinebuf(stdout);

	struct CFG_Config config;
	/* read & check configuration */
	CFG_read(SYSCONFDIR "/openskyd.conf", &config);

	/* initialize GPIO subsystem */
	GPIO_init();


	/* initialize and start statistics */
	if (config.stats.enabled) {
		STAT_init(&config.stats);
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
	ADSB_setup(config.adsb.crc, config.adsb.fec, config.adsb.frameFilter,
		config.adsb.modeAC, config.adsb.rts, config.adsb.timestampGPS);
	enum ADSB_FRAME_TYPE frameFilter = ADSB_FRAME_TYPE_NONE;
	if (config.adsb.modeAC)
		frameFilter |= ADSB_FRAME_TYPE_MODE_AC;
	if (config.adsb.modeSShort)
		frameFilter |= ADSB_FRAME_TYPE_MODE_S_SHORT;
	if (config.adsb.modeSLong)
		frameFilter |= ADSB_FRAME_TYPE_MODE_S_LONG;
	if (TEST)
		frameFilter |= ADSB_FRAME_TYPE_STATUS;
	ADSB_setFilter(frameFilter);
	if (!TEST) {
		/* relay frames only if they're GPS timestamped */
		ADSB_setSynchronizationFilter(true);
	}

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

	if (TEST)
		mainloop_test(&config);
	else
		mainloop(&config);

	return 0;
}

static void mainloop_test(struct CFG_Config * config)
{
	do {
		/* read a frame from the buffer */
		const struct ADSB_Frame * frame = BUF_getFrame();
		switch (frame->frameType) {
		case ADSB_FRAME_TYPE_MODE_S_LONG:
			printf("Mode-S long: mlat %15" PRIu64 ", level %+3" PRIi8 ": ",
				frame->mlat, frame->siglevel);
			int i;
			for (i = 0; i < 14; ++i)
				printf("%02x", frame->payload[i]);
			putchar('\n');
			break;
		case ADSB_FRAME_TYPE_STATUS:
			printf("Status: mlat %15" PRIu64 ", options %02" PRIx8
				", Offset %+.2f ns [%+02" PRId8 "]\n",
				frame->mlat, frame->options,
				frame->offset / 64. * 1000.,
				frame->offset);
			break;
		default:
			break;
		}
		BUF_releaseFrame(frame);
	} while (true);
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
