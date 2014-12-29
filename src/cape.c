
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

	/* buffer up to 100 frames */
	BUF_init(100);

	/* start ADSB mainloop */
	ADSB_init("/dev/ttyO5");
	pthread_t adsb;
	if (pthread_create(&adsb, NULL, (PTHREAD_FN)&ADSB_main, NULL)) {
		error(-1, errno, "Could not create adsb main loop");
	}

	/* only Mode-S long frames */
	BUF_setFilter(3);

	struct ADSB_Frame * frame;
	while (1) {
		frame = BUF_getFrameTimeout(500);
		if (!frame) {
			puts("Timeout");
			continue;
		}
		printf("Got Mode-S long frame with mlat %15" PRIu64 " and level %+3" PRIi8 ": ",
			frame->mlat, frame->siglevel);
		int i;
		for (i = 0; i < 14; ++i)
			printf("%02x", frame->payload[i]);
		putchar('\n');
	}

	/* wait until watchdog and adsb finish
	 * (they'll never do, so wait endless) */
	pthread_join(watchdog, NULL);
	pthread_join(adsb, NULL);

	return 0;
}

