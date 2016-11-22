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
#include "gpio.h"
#include "watchdog.h"
#include "fpga.h"
#include "tb_standalone.h"
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
#include "util/serial_eth.h"

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

bool SERIAL_getSerial(uint32_t * serial)
{
	return SERIAL_ETH_getSerial(serial);
}

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

	if (!SERIAL_ETH_getSerial(NULL))
		LOG_log(LOG_LEVEL_EMERG, PFX, "No serial number configured");

#if defined(INPUT_RADARCAPE_UART)
	LOGIN_setDeviceID(LOGIN_DEVICE_ID_RADARCAPE);
#elif defined(INPUT_RADARCAPE_NETWORK)
	LOGIN_setDeviceID(LOGIN_DEVICE_ID_RADARCAPE_NET);
#elif defined(INPUT_RADARCAPE_DUMMY)
	LOGIN_setDeviceID(LOGIN_DEVICE_ID_BOGUS);
#else
#error "Input Layer unknown"
#endif

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
	if (!CFG_readFile(SYSCONFDIR "/openskyd.conf", true, true, false)) {
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

#ifdef STANDALONE
	TB_register(TB_PACKET_TYPE_REVERSE_SHELL, 6, &TB_reverseShell);
	TB_register(TB_PACKET_TYPE_RESTART, 0, &TB_restartDaemon);
#ifdef WITH_SYSTEMD
	TB_register(TB_PACKET_TYPE_REBOOT, 0, &TB_rebootSystem);
#endif
#ifdef WITH_PACMAN
	TB_register(TB_PACKET_TYPE_UPGRADE_DAEMON, 0, &TB_upgradeDaemon);
#endif
#endif

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
