/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdbool.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <stdio.h>
#include "tb_standalone.h"
#include "fpga.h"
#include "util/proc.h"
#include "util/log.h"

/** Component: Prefix */
static const char PFX[] = "TB";

/** Start reverse connect to given server
 * @param payload packet containing the server address */
void TB_reverseShell(const uint8_t * payload)
{
	/* extract ip and port */
	const struct {
		in_addr_t ip;
		in_port_t port;
	} __attribute__((packed)) * revShell = (const void*)payload;

	char ip[INET_ADDRSTRLEN];
	uint16_t port;

	struct in_addr ipaddr;
	ipaddr.s_addr = revShell->ip;
	inet_ntop(AF_INET, &ipaddr, ip, INET_ADDRSTRLEN);
	port = ntohs(revShell->port);

	char addr[INET_ADDRSTRLEN + 6];
	snprintf(addr, sizeof addr, "%s:%" PRIu16, ip, port);

	/* start rcc in background */
	LOG_logf(LOG_LEVEL_INFO, PFX, "Starting rcc to %s", addr);

	char * argv[] = { "/usr/bin/rcc", "-t", ":22", "-r", addr, "-n", NULL };
	PROC_forkAndExec(argv); /* returns while executing in the background */
}

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

