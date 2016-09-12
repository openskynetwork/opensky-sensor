/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <assert.h>
#include "rc-input.h"
#include "../net_common.h"
#include "../cfgfile.h"
#include "../util.h"
#include "../log.h"

static const char PFX[] = "INPUT";

static char cfgHost[NI_MAXHOST];
static uint_fast16_t cfgPort;

static bool checkCfg(const struct CFG_Section * sect);

static const struct CFG_Section cfg = {
	.name = "INPUT",
	.check = &checkCfg,
	.n_opt = 2,
	.options = {
		{
			.name = "host",
			.type = CFG_VALUE_TYPE_STRING,
			.var = cfgHost,
			.maxlen = sizeof cfgHost,
			.def = {
				.string = "localhost"
			}
		},
		{
			.name = "port",
			.type = CFG_VALUE_TYPE_PORT,
			.var = &cfgPort,
			.def = {
				.port = 10003
			}
		}
	}
};

#define RECONNECT_INTERVAL 10

/** file descriptor for UART */
static int sock;

static bool doConnect();
static void closeConn();

void RC_INPUT_register()
{
	CFG_registerSection(&cfg);
}

static bool checkCfg(const struct CFG_Section * sect)
{
	assert(sect == &cfg);
	if (cfgHost[0] == '\0') {
		LOG_log(LOG_LEVEL_ERROR, PFX, "INPUT.host is missing");
		return false;
	}
	if (cfgPort == 0) {
		LOG_log(LOG_LEVEL_ERROR, PFX, "INPUT.port = 0");
		return false;
	}
	return true;
}

void RC_INPUT_init()
{
	sock = -1;
}

void RC_INPUT_destruct()
{
	closeConn();
}

static void closeConn()
{
	if (sock != -1) {
		shutdown(sock, SHUT_RDWR);
		close(sock);
		sock = -1;
	}
}

void RC_INPUT_connect()
{
	while (!doConnect())
		sleep(RECONNECT_INTERVAL);
}

static bool doConnect()
{
	closeConn();

	sock = NETC_connect(PFX, cfgHost, cfgPort);
	return sock != -1;
}

size_t RC_INPUT_read(uint8_t * buf, size_t bufLen)
{
	ssize_t rc = recv(sock, buf, bufLen, 0);
	if (unlikely(rc < 0)) {
		LOG_errno(LOG_LEVEL_WARN, PFX, "Could not receive from network");
		closeConn();
		return 0;
	} else {
		return rc;
	}
}

size_t RC_INPUT_write(uint8_t * buf, size_t bufLen)
{
	ssize_t rc = send(sock, buf, bufLen, MSG_NOSIGNAL);
	if (unlikely(rc <= 0)) {
		LOG_errno(LOG_LEVEL_WARN, PFX, "Could not send to network");
		return 0;
	}
	return rc;
}
