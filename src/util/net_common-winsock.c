/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <sys/types.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <intsafe.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "net_common.h"
#include "log.h"
#include "threads.h"
#include "util.h"
#include "port/socket.h"

#ifndef HAVE_GETADDRINFO

/** Cleanup Routine
 * @param hosts private hosts information
 */
static void cleanup(char ** hosts)
{
	if (hosts)
		free(hosts);
}

/** Open a TCP Connection to a given host and port. Try all addresses and
 * protocols (ipv4/ipv6) in random order.
 * @param prefix Prefix of the calling component, for logging purposes
 * @param hostName either an ip or the host name to connect to
 * @param port port to connect to
 * @return socket descriptor or -1 upon failure
 */
sock_t NETC_connect(const char * prefix, const char * hostName, uint16_t port)
{
	sock_t sock = SOCK_INVALID;
	struct hostent * host;

	/* resolve name */
	host = gethostbyname(hostName);
	if (host == NULL) {
		LOG_errnet(LOG_LEVEL_WARN, prefix, "Could not resolve host '%s'",
			hostName);
		return SOCK_INVALID;
	}

	if (host->h_addrtype != AF_INET && host->h_addrtype != AF_INET6) {
		LOG_logf(LOG_LEVEL_INFO, prefix, "Could not resolve host '%s': "
			"unknown address family %d", hostName, host->h_addrtype);
		return SOCK_INVALID;
	}

	/* count hosts */
	size_t nHosts = 0;
	for (nHosts = 0; host->h_addr_list[nHosts] != NULL; ++nHosts);

	/* copy hosts */
	char ** hosts = malloc(nHosts * sizeof(char*));
	if (hosts == NULL)
		LOG_errno(LOG_LEVEL_EMERG, prefix, "malloc failed");
	memcpy(hosts, host->h_addr_list, nHosts * sizeof(char*));

	/* shuffle hosts */
	size_t i;
	for (i = nHosts; i; --i) {
		size_t j = UTIL_randInt(i);
		char * tmp = hosts[i - 1];
		hosts[i - 1] = hosts[j];
		hosts[j] = tmp;
	}

	CLEANUP_PUSH(&cleanup, hosts);

	uint16_t port_be = htons(port);
	for (i = 0; i < nHosts; ++i) {
		struct sockaddr addr;

		addr.sa_family = host->h_addrtype;
		size_t addrLen;
		/* extend address info for connecting */
		switch (host->h_addrtype) {
		case AF_INET: {
			struct sockaddr_in * inaddr = (struct sockaddr_in*)&addr;
			memcpy(&inaddr->sin_addr.s_addr, hosts[i], host->h_length);
			inaddr->sin_port = port_be;
			addrLen = sizeof *inaddr;
		}
		break;
		case AF_INET6: {
			struct sockaddr_in6 * inaddr = (struct sockaddr_in6*)&addr;
			memcpy(&inaddr->sin6_addr.s6_addr, hosts[i], host->h_length);
			inaddr->sin6_port = port_be;
			addrLen = sizeof *inaddr;
		}
		break;
		default:
			assert(false);
		}

		char ip[INET6_ADDRSTRLEN];
		DWORD ipLen = sizeof ip;
		if (WSAAddressToString(&addr, addrLen, NULL, ip, &ipLen) ==
			SOCK_ERROR)
			strcpy(ip, "??");

		LOG_logf(LOG_LEVEL_INFO, prefix, "Trying to connect to '%s': [%s]:%"
			PRIu16, hostName, ip, port);

		/* create socket */
		sock = SOCK_socket(addr.sa_family, SOCK_STREAM, IPPROTO_TCP);
		if (sock == SOCK_INVALID)
			LOG_errnet(LOG_LEVEL_ERROR, prefix, "could not create socket");

		/* connect socket */
		int rc = SOCK_connect(sock, &addr, addrLen);
		if (rc == SOCK_ERROR) {
			LOG_errnet(LOG_LEVEL_WARN, prefix, "Could not connect");
			SOCK_close(sock);
		} else {
			LOG_logf(LOG_LEVEL_INFO, prefix, "connected to '%s'", hostName);
			break;
		}
	}
	CLEANUP_POP();

	if (i == nHosts) {
		LOG_logf(LOG_LEVEL_WARN, prefix, "Tried all addresses of '%s': could "
			"not connect", hostName);
		return -1;
	} else {
		return sock;
	}
}

#else

/** Private data for addresses */
struct Hosts {
	/** Hosts as returned by getaddrinfo */
	struct addrinfo * hosts;
	/** Pointers into hosts, in random order */
	struct addrinfo ** shuffled;
};

/** Cleanup Routine
 * @param hosts private hosts information
 */
static void cleanup(struct Hosts * hosts)
{
	if (hosts) {
		if (hosts->hosts)
			freeaddrinfo(hosts->hosts);
		free(hosts->shuffled);
		free(hosts);
	}
}

/** Open a TCP Connection to a given host and port. Try all addresses and
 * protocols (ipv4/ipv6) in random order.
 * @param prefix Prefix of the calling component, for logging purposes
 * @param hostName either an ip or the host name to connect to
 * @param port port to connect to
 * @return socket descriptor or -1 upon failure
 */
sock_t NETC_connect(const char * prefix, const char * hostName, uint16_t port)
{
	struct addrinfo * host = NULL, hints;
	sock_t sock = SOCK_INVALID;

	/* must be pointer for correct cleanup */
	struct Hosts * hosts = malloc(sizeof *hosts);
	if (hosts == NULL)
		LOG_errno(LOG_LEVEL_EMERG, prefix, "malloc failed");
	hosts->hosts = NULL;
	hosts->shuffled = NULL;

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = 0;

	CLEANUP_PUSH(&cleanup, hosts);

	/* resolve name */
	int rc = getaddrinfo(hostName, NULL, &hints, &hosts->hosts);
	if (rc) {
		LOG_errnet(LOG_LEVEL_WARN, prefix, "Could not resolve host '%s'",
			hostName);
		goto cleanup;
	}

	/* count hosts */
	size_t nHosts = 0;
	for (host = hosts->hosts; host != NULL; host = host->ai_next)
		++nHosts;

	/* copy hosts */
	hosts->shuffled = malloc(nHosts * sizeof(struct addrinfo));
	if (hosts->shuffled == NULL)
		LOG_errno(LOG_LEVEL_EMERG, prefix, "malloc failed");
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
		if (InetNtopA(host->ai_family, inaddr, ip, sizeof ip) == NULL)
			strcpy(ip, "??");

		LOG_logf(LOG_LEVEL_INFO, prefix, "Trying to connect to '%s': [%s]:%"
			PRIu16, hostName, ip, port);

		/* create socket */
		sock = SOCK_socket(host->ai_family, host->ai_socktype | SOCK_CLOEXEC,
			host->ai_protocol);
		if (sock == SOCK_INVALID)
			LOG_errnet(LOG_LEVEL_ERROR, prefix, "could not create socket");

		/* connect socket */
		rc = SOCK_connect(sock, addr, host->ai_addrlen);
		if (rc == SOCK_ERROR) {
			LOG_errnet(LOG_LEVEL_WARN, prefix, "Could not connect");
			SOCK_close(sock);
		} else {
			LOG_logf(LOG_LEVEL_INFO, prefix, "connected to '%s'", hostName);
			break;
		}
	}
	if (i == nHosts)
		host = NULL;
cleanup:
	CLEANUP_POP();

	if (!host) {
		LOG_logf(LOG_LEVEL_WARN, prefix, "Tried all addresses of '%s': could "
			"not connect", hostName);
		return -1;
	} else {
		return sock;
	}
}
#endif

