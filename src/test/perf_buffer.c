/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <check.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include "input_perf.h"
#include "../openskytypes.h"
#include "../buffer.h"
#include "../recv.h"
#include "../statistics.h"
#include "../cfgfile.h"

int main()
{
	uint8_t buf[46];
	size_t len = RC_INPUT_buildFrame(buf, OPENSKY_FRAME_TYPE_MODE_S_LONG,
		0xdeadbe, -10, "abcdefghijklmn", 14);
	RC_INPUT_setBuffer(buf, len);

	COMP_register(&BUF_comp);
	COMP_register(&RECV_comp);
	COMP_fixup();
	COMP_setSilent(true);

	CFG_loadDefaults();
	CFG_setBoolean("BUFFER", "GC", false);
	CFG_setBoolean("BUFFER", "History", false);
	CFG_setInteger("BUFFER", "StaticBacklog", 100);
	CFG_setBoolean("FILTER", "CRC", false);
	CFG_setBoolean("FILTER", "ModeSExtSquitterOnly", false);
	CFG_setBoolean("FILTER", "SyncFilter", false);

	COMP_initAll();
	COMP_startAll();

	struct timespec start, end;
	clock_gettime(CLOCK_REALTIME, &start);
	size_t i;
	for (i = 0; i < 1000000; ++i) {
		BUF_putFrame(BUF_getFrame());
	}
	clock_gettime(CLOCK_REALTIME, &end);

	COMP_stopAll();
	COMP_destructAll();

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
