/* Copyright (c) 2015 Sero Systems <contact at sero-systems dot de> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <check.h>
#include <adsb.h>
#include <buffer.h>
#include <recv.h>
#include <input_perf.h>
#include <statistics.h>
#include <cfgfile.h>
#include <inttypes.h>
#include <stdio.h>

struct CFG_Config CFG_config;

int main()
{
	uint8_t buf[46];
	size_t len = INPUT_buildFrame(buf, ADSB_FRAME_TYPE_MODE_S_LONG, 0xdeadbe,
		-10, "abcdefghijklmn", 14);
	INPUT_setBuffer(buf, len);

	COMP_register(&BUF_comp, NULL);
	COMP_register(&RECV_comp, NULL);
	COMP_setSilent(true);

	CFG_config.buf.gcEnabled = false;
	CFG_config.buf.history = false;
	CFG_config.buf.statBacklog = 100;

	COMP_initAll();
	COMP_startAll();

	struct timespec start, end;
	clock_gettime(CLOCK_REALTIME, &start);
	size_t i;
	for (i = 0; i < 1000000; ++i) {
		BUF_putFrame(BUF_getFrame());
	}
	clock_gettime(CLOCK_REALTIME, &end);

	int64_t s = end.tv_sec - start.tv_sec;
	int64_t ns = end.tv_nsec - start.tv_nsec;
	if (ns < 0) {
		ns += 1000000000;
		s -= 1;
	}
	double d = s + ns / 1e9;
	printf("Took %.2fs -> %.2fns per message\n", d, d / 1000000 * 1e9);

	return EXIT_SUCCESS;
}
