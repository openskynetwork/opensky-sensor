/* Copyright (c) 2015-2016 Sero Systems <contact at sero-systems dot de> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gpio.h>
#include <watchdog.h>
#include <fpga.h>
#include <cfgfile.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <util.h>
#include <string.h>

#if defined(DEVELOPMENT) && !defined(SYSCONFDIR)
#define SYSCONFDIR "."
#endif

static struct option opts[] = {
	{ .name = "fpga", .has_arg = optional_argument, .val = 'f' },
	{ .name = "black", .has_arg = no_argument, .val = 'b' },
	{ .name = "help", .has_arg = no_argument, .val = 'h' },
	{}
};

int main(int argc, char * argv[])
{
	int opt;
	bool fpgaConfig = false;
	bool bbwhite = true;
	const char * fpgaFile = NULL;

	while ((opt = getopt_long(argc, argv, "h", opts, NULL)) != -1) {
		switch (opt) {
		case 'f':
			fpgaConfig = true;
			fpgaFile = optarg;
			break;
		case 'b':
			bbwhite = false;
			break;
		default:
			fprintf(stderr,
				"Usage: %s [--fpga [<file>]] [--black] [--help|-h]\n", argv[0]);
			return EXIT_FAILURE;
		}
	}

	/* initialize GPIO subsystem */
	GPIO_comp.construct(NULL);

	if (fpgaConfig) {
		if (fpgaFile) {
			strncpy(CFG_config.fpga.file, fpgaFile,
				sizeof CFG_config.fpga.file);
			CFG_config.fpga.retries = 3;
			CFG_config.fpga.timeout = 10;
		} else {
			/* read & check configuration */
			CFG_read(SYSCONFDIR "/openskyd.conf");
		}

		/* overwrite cfg */
		CFG_config.fpga.configure = true;

		FPGA_comp.construct(&bbwhite);
		if (!FPGA_comp.start(&FPGA_comp, NULL)) {
			fprintf(stderr,
				"Could not configure fpga, triggering watchdog only\n");
		}
	}

	/* drop privileges */
	UTIL_dropPrivileges();

	WD_comp.construct(NULL);
	WD_comp.main();

	return EXIT_SUCCESS;
}
