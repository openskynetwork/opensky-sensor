/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <check.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include "input_perf.h"
#include "core/openskytypes.h"
#include "core/input.h"
#include "util/statistics.h"
#include "util/cfgfile.h"
#include "util/component.h"

int main()
{
	struct OPENSKY_RawFrame raw;
	struct OPENSKY_DecodedFrame decoded;
	uint8_t buf[46];
	size_t len = RC_INPUT_buildFrame(buf, OPENSKY_FRAME_TYPE_MODE_S_LONG,
		0xdeadbe, -10, "abcdefghijklmn", 14);
	RC_INPUT_setBuffer(buf, len);

	COMP_setSilent(true);
	COMP_register(&INPUT_comp);
	COMP_initAll();
	COMP_startAll();
	INPUT_connect();

	struct timespec start, end;
	clock_gettime(CLOCK_REALTIME, &start);
	size_t i;
	for (i = 0; i < 10000000; ++i) {
		INPUT_getFrame(&raw, &decoded);
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
	printf("Took %.2fs -> %.2fns per message\n", d, d / 10000000 * 1e9);

	return EXIT_SUCCESS;
}
