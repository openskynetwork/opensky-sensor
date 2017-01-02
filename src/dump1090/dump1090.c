/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#define _GNU_SOURCE
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <error.h>
#include <errno.h>
#include <sys/time.h>
#include <stdbool.h>
#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef HAVE_DIRECT_H
#include <direct.h>
#endif
#include "core/buffer.h"
#include "core/network.h"
#include "core/tb.h"
#include "core/recv.h"
#include "core/relay.h"
#include "core/gps.h"
#include "core/login.h"
#include "core/serial.h"
#include "core/beast.h"
#include "core/filter.h"
#include "util/cfgfile.h"
#include "util/statistics.h"
#include "util/log.h"
#include "util/util.h"
#include "util/port/socket.h"
#include "req-serial.h"
#include "position.h"

/** Component: Prefix */
static const char PFX[] = "MAIN";

#if (defined(ECLIPSE) || defined(LOCAL_FILES)) && !defined(SYSCONFDIR)
#define SYSCONFDIR "."
#endif
#if (defined(ECLIPSE) || defined(LOCAL_FILES)) && !defined(LOCALSTATEDIR)
#define LOCALSTATEDIR "var"
#endif

/** Configuration: user name */
static char username[BEAST_MAX_USERNAME + 1];

/** Configuration Descriptor */
static struct CFG_Section cfgDesc =
{
	.name = "IDENT",
	.n_opt = 1,
	.options = {
		{
			.name = "Username",
			.type = CFG_VALUE_TYPE_STRING,
			.var = &username,
			.maxlen = BEAST_MAX_USERNAME,
		}
	}
};

/** Make sure, that a given directory exists.
 * @param path path to the directory
 * @note will try to create the directory if it does not exist
 * @note will abort the process if something goes wrong
 */
static void ensureDir(const char * path)
{
	struct stat st;
	int rc = stat(path, &st);
	if (rc == -1) {
		if (errno == ENOENT) {
#if !defined(__WIN32__) && !defined(__WIN64__)
			rc = mkdir(path, 0755);
#else
			rc = _mkdir(path);
#endif
			if (rc == -1) {
				LOG_errno(LOG_LEVEL_EMERG, PFX,
					"Could not create configuration directory '%s'", path);
			}
		} else if (errno == EACCES) {
			LOG_errno(LOG_LEVEL_EMERG, PFX,
				"Could not access configuration directory '%s'", path);
		}
	} else if (!S_ISDIR(st.st_mode)) {
		LOG_errno(LOG_LEVEL_EMERG, PFX,
			"Configuration path '%s' is not a directory", path);
	}
}

/** Entry point
 * @param argc argument count
 * @param argv argument vector
 * @return 0 on successful exit
 */
int main(int argc, char * argv[])
{
#ifdef HAVE_SETLINEBUF
	/* force flushing of stdout and stderr on newline */
	setlinebuf(stdout);
#endif
	SOCK_init();

	struct timeval tv;
	gettimeofday(&tv, NULL);
	srand(tv.tv_sec + tv.tv_usec);

	COMP_register(&TB_comp);
	COMP_register(&RELAY_comp);
	COMP_register(&RECV_comp);
	COMP_register(&STAT_comp);
	COMP_register(&SERIAL_comp);
	COMP_register(&GPS_comp);

	COMP_fixup();

	CFG_registerSection(&cfgDesc);

	ensureDir(LOCALSTATEDIR);
	ensureDir(LOCALSTATEDIR "/conf.d");

	/* read & check configuration */
	CFG_loadDefaults();
	CFG_setPort("INPUT", "Port", 30005);
	FILTER_setSynchronizedFilter(false);
	bool cfgVar = CFG_readDirectory(LOCALSTATEDIR "/conf.d", true, true, true,
		false);
	bool cfgEtc = CFG_readDirectory(SYSCONFDIR "/conf.d", true, true, true,
		false);
	if (!cfgEtc || !cfgVar)
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

	LOGIN_setDeviceType(BEAST_DEVICE_TYPE_FEEDER);
	LOGIN_setUsername(username);

	if (!COMP_startAll()) {
		LOG_log(LOG_LEVEL_EMERG, PFX, "Could not start all components, "
			"quitting");
	}

	UTIL_waitSigInt();

	COMP_stopAll();
	COMP_destructAll();

	COMP_unregisterAll();
	CFG_unregisterAll();

	SOCK_cleanup();

	return EXIT_SUCCESS;
}
