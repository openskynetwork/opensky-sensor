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
#include "core/buffer.h"
#include "core/network.h"
#include "core/tb.h"
#include "core/recv.h"
#include "core/relay.h"
#include "core/gps.h"
#include "core/login.h"
#include "core/serial.h"
#include "util/cfgfile.h"
#include "util/statistics.h"
#include "util/log.h"
#include "util/util.h"

static const char PFX[] = "MAIN";

#if (defined(ECLIPSE) || defined(LOCAL_FILES)) && !defined(SYSCONFDIR)
#define SYSCONFDIR "."
#endif

static void sigint(int sig);

static bool run;
static pthread_mutex_t sigmutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sigcond = PTHREAD_COND_INITIALIZER;

int main(int argc, char * argv[])
{
	/* force flushing of stdout and stderr on newline */
	setlinebuf(stdout);

	pthread_setname_np(pthread_self(), "MAIN");

	struct timeval tv;
	gettimeofday(&tv, NULL);
	srand(tv.tv_sec + tv.tv_usec);

	LOGIN_setDeviceID(LOGIN_DEVICE_ID_FEEDER);

	COMP_register(&TB_comp);
	COMP_register(&RELAY_comp);
	COMP_register(&RECV_comp);
	COMP_register(&STAT_comp);

	COMP_fixup();

	/* read & check configuration */
	CFG_loadDefaults();
	bool cfg = CFG_readDirectory("/var/lib/openskyd/conf.d", true, true, true, false);
	cfg = cfg && CFG_readDirectory("/etc/openskyd/conf.d", true, true, true, false);
	if (!cfg)
		LOG_log(LOG_LEVEL_WARN, PFX,
			"Could not read all configuration files");

	if (!CFG_check()) {
		LOG_log(LOG_LEVEL_EMERG, PFX,
			"Configuration inconsistent, quitting");
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
