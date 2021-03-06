/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include "gpio.h"
#include "watchdog.h"
#include "util/util.h"

/** CLI options */
static struct option opts[] = {
	{ .name = "help", .has_arg = no_argument, .val = 'h' },
	{}
};

/** Entry point
 * @param argc argument count
 * @param argv argument vector
 * @return 0 on successful exit
 */
int main(int argc, char * argv[])
{
	int opt;

	while ((opt = getopt_long(argc, argv, "h", opts, NULL)) != -1) {
		switch (opt) {
		default:
			fprintf(stderr, "Usage: %s [--help|-h]\n", argv[0]);
			return EXIT_FAILURE;
		}
	}

	/* initialize GPIO subsystem */
	GPIO_comp.onConstruct(NULL);

	/* drop privileges */
	UTIL_dropPrivileges();

	/* Start WD */
	WD_comp.onConstruct(NULL);
	WD_comp.main();

	return EXIT_SUCCESS;
}
