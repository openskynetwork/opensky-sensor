/* Copyright (c) 2015 Sero Systems <contact at sero-systems dot de> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <net_common.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <error.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <threads.h>

static void cleanup(struct addrinfo * hosts)
{
	if (hosts)
		freeaddrinfo(hosts);
}

int NETC_connect(const char * component, const char * hostName, uint16_t port)
{
	struct addrinfo * hosts = NULL, * host;
	int sock = -1;

	/* resolve name */
	int rc = getaddrinfo(hostName, NULL, NULL, &hosts);
	CLEANUP_PUSH(&cleanup, hosts);

	if (rc) {
		NOC_fprintf(stderr, "%s: could not resolve '%s': %s\n",
			component, hostName, gai_strerror(rc));
		return -1;
	}

	uint16_t port_be = htons(port);
	for (host = hosts; host != NULL; host = host->ai_next) {
		struct sockaddr * addr = host->ai_addr;

		/* extend address info for connecting */
		switch (host->ai_family) {
		case AF_INET:
			((struct sockaddr_in*)addr)->sin_port = port_be;
		break;
		case AF_INET6:
			((struct sockaddr_in6*)addr)->sin6_port = port_be;
		break;
		default:
			NOC_printf("%s: ignoring unknown family %d for address '%s'",
				component, host->ai_family, hostName);
			continue;
		}

		/* create socket */
		sock = socket(host->ai_family, SOCK_STREAM | SOCK_CLOEXEC, 0);
		if (sock < 0)
			error(EXIT_FAILURE, errno, "socket");

		/* connect socket */
		rc = connect(sock, addr, host->ai_addrlen);
		if (rc < 0) {
			close(sock);
		} else {
			NOC_printf("%s: connected to '%s:%" PRIu16 "'\n", component,
				hostName, port);
			break;
		}
	}
	CLEANUP_POP();

	if (!host) {
		NOC_fprintf(stderr, "%s: could not connect to '%s:%" PRIu16 "': %s\n",
			component, hostName, port, strerror(errno));
		return -1;
	} else {
		return sock;
	}
}
