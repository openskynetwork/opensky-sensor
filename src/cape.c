
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

typedef void*(*PTHREAD_FN)(void*);

int main()
{
	/* initialize GPIO subsystem */
	GPIO_init();

	/* initialize and start watchdog */
	WATCHDOG_init();
	pthread_t watchdog;
	if (pthread_create(&watchdog, NULL, (PTHREAD_FN)&WATCHDOG_main, NULL)) {
		error(-1, errno, "Could not create watchdog thread");
	}

	/* initialize and reprogram FPGA */
	FPGA_init();
	FPGA_program("cape.rbf");

	/* buffer up to 10 + 1000 * 1080 frames */
	BUF_init(10, 1000, 1080);

	/* start Buffer Garbage Collection */
	pthread_t buf;
	if (pthread_create(&buf, NULL, (PTHREAD_FN)&BUF_main, NULL)) {
		error(-1, errno, "Could not start buffering garbage collector");
	}

	/* start ADSB mainloop */
	ADSB_init("/dev/ttyO5");
	pthread_t adsb;
	if (pthread_create(&adsb, NULL, (PTHREAD_FN)&ADSB_main, NULL)) {
		error(-1, errno, "Could not create adsb main loop");
	}

	/* only Mode-S long frames */
	BUF_setFilter(3);

	while (1) {
		/* try to connect to the server */
		while (!NET_connect("mrks", 30003))
			sleep(10); /* in case of failure: retry every 10 seconds */

		/* send serial number */
		if (!NET_sendSerial())
			continue; /* on error: reconnect */

		bool success;
		do {
			/* read a frame from the buffer */
			const struct ADSB_Frame * frame = BUF_getFrameTimeout(500);
			if (!frame) {
				/* timeout */
				success = NET_sendTimeout();
			} else {
				/* got a frame */
				printf("Mode-S long: mlat %15" PRIu64 ", level %+3" PRIi8 ": ",
					frame->mlat, frame->siglevel);
				int i;
				for (i = 0; i < 14; ++i)
					printf("%02x", frame->payload[i]);
				putchar('\n');
				success = NET_sendFrame(frame);
				if (success)
					BUF_releaseFrame(frame);
				else
					BUF_putFrame(frame);
			}
		} while(success);
		/* if sending failed, reconnect to the server */
	}

	/* wait until watchdog and adsb finish
	 * (they'll never do, so wait endless) */
	pthread_join(watchdog, NULL);
	pthread_join(adsb, NULL);

	return 0;
}

