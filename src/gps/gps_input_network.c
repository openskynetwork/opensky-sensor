/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <error.h>
#include <termios.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <assert.h>
#include "gps_input.h"
#include "../net_common.h"
#include "../cfgfile.h"
#include "../util.h"
#include "../log.h"

#define PFX "GPS"

#define RECONNECT_INTERVAL 10

static char cfgHost[NI_MAXHOST];
static uint_fast16_t cfgPort;

static bool checkCfg(const struct CFG_Section * sect);

static const struct CFG_Section cfg = {
	.name = "GPS",
	.check = &checkCfg,
	.n_opt = 2,
	.options = {
		{
			.name = "Host",
			.type = CFG_VALUE_TYPE_STRING,
			.var = &cfgHost,
			.maxlen = sizeof cfgHost,
			.def = {
				.string = "localhost"
			}
		},
		{
			.name = "Port",
			.type = CFG_VALUE_TYPE_PORT,
			.var = &cfgPort,
			.def = { .port = 10685 }
		}
	}
};

/** socket descriptor */
static int sock;

static bool doConnect();
static void closeConn();

void GPS_INPUT_register()
{
	CFG_registerSection(&cfg);
}

static bool checkCfg(const struct CFG_Section * sect)
{
	assert(sect == &cfg);
	if (cfgHost[0] == '\0') {
		LOG_log(LOG_LEVEL_ERROR, PFX, "NET.host is missing");
		return false;
	}
	if (cfgPort == 0) {
		LOG_log(LOG_LEVEL_ERROR, PFX, "NET.port = 0");
		return false;
	}
	return true;
}

void GPS_INPUT_init()
{
	sock = -1;
}

void GPS_INPUT_disconnect()
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

void GPS_INPUT_connect()
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

size_t GPS_INPUT_read(uint8_t * buf, size_t bufLen)
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

size_t GPS_INPUT_write(const uint8_t * buf, size_t bufLen)
{
	/* ignore */
	return bufLen;
}
