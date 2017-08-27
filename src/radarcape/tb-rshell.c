/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdbool.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <stdio.h>
#include "tb-rshell.h"
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

