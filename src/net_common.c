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
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <threads.h>
#include <util.h>
#include <assert.h>

struct Hosts {
	struct addrinfo * hosts;
	struct addrinfo ** shuffled;
};

static void cleanup(struct Hosts * hosts)
{
	if (hosts) {
		if (hosts->hosts)
			freeaddrinfo(hosts->hosts);
		free(hosts->shuffled);
		free(hosts);
	}
}

int NETC_connect(const char * prefix, const char * hostName, uint16_t port)
{
	struct addrinfo * host, hints;
	int sock = -1;

	// must be pointer for correct cleanup
	struct Hosts * hosts = malloc(sizeof *hosts);
	assert(hosts);

	hosts->hosts = NULL;
	hosts->shuffled = NULL;

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_V4MAPPED | AI_ADDRCONFIG;

	CLEANUP_PUSH(&cleanup, hosts);

	/* resolve name */
	int rc = getaddrinfo(hostName, NULL, &hints, &hosts->hosts);
	if (rc) {
		LOG_logf(LOG_LEVEL_WARN, prefix, "Could not resolve host '%s': %s",
			hostName, gai_strerror(rc));
		return -1;
	}

	/* count hosts */
	size_t nHosts = 0;
	for (host = hosts->hosts; host != NULL; host = host->ai_next)
		++nHosts;

	/* copy hosts */
	hosts->shuffled = malloc(nHosts * sizeof(struct addrinfo));
	assert(hosts->shuffled);
	size_t i;
	for (i = 0, host = hosts->hosts; host != NULL; host = host->ai_next, ++i)
		hosts->shuffled[i] = host;

	/* shuffle hosts */
	for (i = nHosts; i; --i) {
		size_t j = UTIL_randInt(i);
		struct addrinfo * tmp = hosts->shuffled[i - 1];
		hosts->shuffled[i - 1] = hosts->shuffled[j];
		hosts->shuffled[j] = tmp;
	}

	uint16_t port_be = htons(port);
	for (i = 0; i < nHosts; ++i) {
		host = hosts->shuffled[i];
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
		if (inet_ntop(host->ai_family, inaddr, ip, sizeof ip) == NULL)
			strcpy(ip, "??");

		LOG_logf(LOG_LEVEL_INFO, prefix, "Trying to connect to '%s': [%s]:%"
			PRIu16, hostName, ip, port);

		/* create socket */
		sock = socket(host->ai_family, host->ai_socktype | SOCK_CLOEXEC,
			host->ai_protocol);
		if (sock < 0)
			LOG_errno(LOG_LEVEL_ERROR, prefix, "could not create socket");
// TODO: log level
		/* connect socket */
		rc = connect(sock, addr, host->ai_addrlen);
		if (rc < 0) {
			LOG_errno(LOG_LEVEL_WARN, prefix, "Could not connect");
			close(sock);
		} else {
			LOG_logf(LOG_LEVEL_INFO, prefix, "connected to '%s'", hostName);
			break;
		}
	}
	if (i == nHosts)
		host = NULL;
	CLEANUP_POP();

	if (!host) {
		LOG_logf(LOG_LEVEL_WARN, prefix, "Tried all addresses of '%s': could "
			"not connect", hostName);
		return -1;
	} else {
		return sock;
	}
}
