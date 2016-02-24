/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <net_common.h>
#include <log.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
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

int NETC_connect(const char * prefix, const char * hostName, uint16_t port)
{
	struct addrinfo * hosts = NULL, * host;
	int sock = -1;

	/* resolve name */
	int rc = getaddrinfo(hostName, NULL, NULL, &hosts);
	if (rc) {
		LOG_logf(LOG_LEVEL_WARN, prefix, "Could not resolve host '%s': %s",
			hostName, gai_strerror(rc));
		return -1;
	}

	CLEANUP_PUSH(&cleanup, hosts);

	uint16_t port_be = htons(port);
	for (host = hosts; host != NULL; host = host->ai_next) {
		struct sockaddr * addr = host->ai_addr;

		const void * inaddr;

		/* extend address info for connecting */
		switch (host->ai_family) {
		case AF_INET:
			((struct sockaddr_in*)addr)->sin_port = port_be;
			inaddr = &((struct sockaddr_in*)addr)->sin_addr;
		break;
		case AF_INET6:
			((struct sockaddr_in6*)addr)->sin6_port = port_be;
			inaddr = &((struct sockaddr_in6*)addr)->sin6_addr;
		break;
		default:
			LOG_logf(LOG_LEVEL_INFO, prefix, "Ignoring unknown address family "
				"%d for host '%s'", host->ai_family, hostName);
			continue;
		}

		char ip[INET6_ADDRSTRLEN];
		if (inet_ntop(host->ai_family, inaddr, ip, sizeof ip) != NULL)
			strcpy(ip, "??");

		LOG_logf(LOG_LEVEL_INFO, "Trying to connect to '%s': [%s]:%" PRIu16,
			hostName, ip, port);

		/* create socket */
		sock = socket(host->ai_family, SOCK_STREAM | SOCK_CLOEXEC, 0);
		if (sock < 0)
			LOG_errno(LOG_LEVEL_ERROR, prefix, "could not create socket");

		/* connect socket */
		rc = connect(sock, addr, host->ai_addrlen);
		if (rc < 0) {
			LOG_errno(LOG_LEVEL_WARN, prefix, "Could not connect");
			close(sock);
		} else {
			char ip[INET6_ADDRSTRLEN];
			if (inet_ntop(host->ai_family, inaddr, ip, sizeof ip) != NULL)
				strcpy(ip, "??");
			LOG_logf(LOG_LEVEL_INFO, prefix, "connected to '%s'", hostName);
			break;
		}
	}
	CLEANUP_POP();

	if (!host) {
		LOG_logf(LOG_LEVEL_WARN, prefix, "Tried all addresses of '%s': could "
			"not connect", hostName);
		return -1;
	} else {
		return sock;
	}
}
