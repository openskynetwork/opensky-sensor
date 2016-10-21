/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <unistd.h>
#include <stdbool.h>
#include "watchdog.h"
#include "gpio.h"
#include "util/statistics.h"

/** Watchdog GPIO number */
#define GPIO 60
/** Watchdog repetition rate in seconds */
#define REPEAT 30

static bool construct();
static void mainloop();

struct Component WD_comp = {
	.description = "WD",
	.onConstruct = &construct,
	.main = &mainloop,
	.dependencies = {
		&GPIO_comp,
		NULL
	}
};

/** Initialize watchdog.
 * \note: GPIO_init() must be called prior to that function!
 */
static bool construct()
{
	GPIO_setDirection(GPIO, GPIO_DIRECTION_OUT);
	GPIO_clear(GPIO);

	return true;
}

/** Watchdog mainloop: tell the watchdog we are still here in an endless loop */
static void mainloop()
{
	while (1) {
		GPIO_set(GPIO);
		GPIO_clear(GPIO);
#ifndef WD_ONLY
		++STAT_stats.WD_events;
#endif
		sleep(REPEAT);
	}
}
