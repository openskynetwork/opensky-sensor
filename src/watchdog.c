/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gpio.h>
#include <watchdog.h>
#include <stdio.h>
#include <unistd.h>
#include <statistics.h>

/** Watchdog GPIO number */
#define WD_GPIO 60
/** Watchdog repetition rate in seconds */
#define WD_REPEAT 30

static void construct();
static void mainloop();

struct Component WD_comp = {
	.description = "WD",
	.construct = &construct,
	.main = &mainloop
};

/** Initialize watchdog.
 * \note: GPIO_init() must be called prior to that function!
 */
static void construct()
{
	GPIO_setDirection(WD_GPIO, GPIO_DIRECTION_OUT);
	GPIO_clear(WD_GPIO);
}

/** Watchdog mainloop: tell the watchdog we are still here in an endless loop */
static void mainloop()
{
	while (1) {
		GPIO_set(WD_GPIO);
		GPIO_clear(WD_GPIO);
#ifndef WD_ONLY
		++STAT_stats.WD_events;
#endif
		sleep(WD_REPEAT);
	}
}
