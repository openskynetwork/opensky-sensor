/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#define _GNU_SOURCE
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <error.h>
#include <sys/time.h>
#include <stdbool.h>
#include <stdio.h>
#include <getopt.h>
#include "radarcape/gpio.h"
#include "radarcape/watchdog.h"
#include "radarcape/fpga.h"
#include "buffer.h"
#include "network.h"
#include "cfgfile.h"
#include "statistics.h"
#include "tb.h"
#include "recv.h"
#include "relay.h"
#include "log.h"
#include "gps.h"
#include "util.h"

static const char PFX[] = "MAIN";

#if (defined(ECLIPSE) || defined(LOCAL_FILES)) && !defined(SYSCONFDIR)
#define SYSCONFDIR "."
#endif

static void sigint(int sig);

static bool run;
static pthread_mutex_t sigmutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sigcond = PTHREAD_COND_INITIALIZER;

const char * MAIN_progName;

static struct option opts[] = {
#ifdef STANDALONE
	{ .name = "black", .has_arg = no_argument, .val = 'b' },
#endif
	{ .name = "save", .has_arg = required_argument, .val = 's' },
	{ .name = "help", .has_arg = no_argument, .val = 'h' },
	{}
};

int main(int argc, char * argv[])
{
	/* force flushing of stdout and stderr on newline */
	setlinebuf(stdout);

	pthread_setname_np(pthread_self(), "MAIN");

	MAIN_progName = argv[0];

#ifdef STANDALONE
	FPGA_bbwhite = true;
#endif

	const char * savefile = NULL;

	int opt;
	while ((opt = getopt_long(argc, argv, "h", opts, NULL)) != -1) {
		switch (opt) {
#ifdef STANDALONE
		case 'b':
			FPGA_bbwhite = false;
			break;
#endif
		case 's':
			savefile = optarg;
			break;
		default:
			fprintf(stderr, "Usage: %s [--black] [--save <file>] [--help|-h]\n",
				argv[0]);
			return EXIT_FAILURE;
		}
	}

	struct timeval tv;
	gettimeofday(&tv, NULL);
	srand(tv.tv_sec + tv.tv_usec);

	if (!UTIL_getSerial(NULL))
		LOG_log(LOG_LEVEL_EMERG, PFX, "No serial number configured");

#ifdef STANDALONE
	COMP_register(&FPGA_comp);
	COMP_register(&WD_comp);
#endif
	COMP_register(&TB_comp);
	COMP_register(&RELAY_comp);
	COMP_register(&RECV_comp);
	COMP_register(&STAT_comp);
	COMP_register(&GPS_RECV_comp);

	COMP_fixup();

	/* read & check configuration */
	CFG_loadDefaults();
	if (!CFG_readFile(SYSCONFDIR "/openskyd.conf")) {
		LOG_log(LOG_LEVEL_WARN, PFX,
			"Could not read configuration, using defaults");
	}
	if (!CFG_check()) {
		LOG_log(LOG_LEVEL_EMERG, PFX,
			"Configuration inconsistent, quitting");
	}

	if (savefile) {
		LOG_logf(LOG_LEVEL_INFO, PFX, "Writing configuration file '%s'",
			savefile);
		CFG_write(savefile);
		COMP_unregisterAll();
		CFG_unregisterAll();
		return EXIT_SUCCESS;
	}

	if (!COMP_initAll()) {
		LOG_log(LOG_LEVEL_EMERG, PFX, "Could not initialize all components, "
			"quitting");
	}

	if (!COMP_startAll()) {
		LOG_log(LOG_LEVEL_EMERG, PFX, "Could not start all components, "
			"quitting");
	}

	run = true;
	pthread_mutex_lock(&sigmutex);
#ifdef CLEANUP_ROUTINES
	signal(SIGINT, &sigint);
#endif
	while (run)
		pthread_cond_wait(&sigcond, &sigmutex);
	signal(SIGINT, SIG_DFL);
	pthread_mutex_unlock(&sigmutex);

	COMP_stopAll();
	COMP_destructAll();

	COMP_unregisterAll();
	CFG_unregisterAll();

	return EXIT_SUCCESS;
}

#ifdef CLEANUP_ROUTINES
static void sigint(int sig)
{
	pthread_mutex_lock(&sigmutex);
	run = false;
	pthread_cond_broadcast(&sigcond);
	pthread_mutex_unlock(&sigmutex);
}
#endif
