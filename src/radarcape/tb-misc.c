/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include "tb-misc.h"
#include "fpga.h"
#include "util/proc.h"
#include "util/log.h"

/** Component: Prefix */
static const char PFX[] = "TB";

/** Restart Daemon.
 * @param payload dummy (not used) */
void TB_restartDaemon(const uint8_t * payload)
{
	LOG_log(LOG_LEVEL_INFO, PFX, "restarting daemon");
	LOG_flush();

	extern char * MAIN_progName;

	char * argv[] = {
		MAIN_progName,
		NULL
	};
	char * argvBlack[] = {
		MAIN_progName,
		"--black",
		NULL
	};

	/* replace daemon by new instance */
	PROC_execRaw(FPGA_bbwhite ? argv : argvBlack);
}

#ifdef WITH_SYSTEMD
/** Reboot system using systemd
 * @param payload dummy (not used)
 */
void TB_rebootSystem(const uint8_t * payload)
{
	char *argv[] = { WITH_SYSTEMD "/systemctl", "reboot", NULL };
	PROC_forkAndExec(argv); /* returns while executing in the background */
}
#endif

#ifdef WITH_PACMAN
/** Upgrade daemon using pacman and restart daemon using systemd.
 * @param payload dummy (not used) */
void TB_upgradeDaemon(const uint8_t * payload)
{
	if (!PROC_fork())
		return; /* parent: return immediately */

	char *argv[] = { "/usr/bin/pacman", "--noconfirm", "--needed", "-Sy",
		"openskyd", "rcc", NULL };
	if (!PROC_execAndReturn(argv)) {
		LOG_log(LOG_LEVEL_WARN, PFX, "Upgrade failed");
	} else {
#ifdef WITH_SYSTEMD
		char *argv1[] = { WITH_SYSTEMD "/systemctl", "daemon-reload", NULL };
		PROC_execAndReturn(argv1);

		char *argv2[] = { WITH_SYSTEMD "/systemctl", "restart", "openskyd",
			NULL };
		PROC_execAndFinalize(argv2);
#endif
	}
}
#endif

