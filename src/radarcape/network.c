/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include "rc-input.h"
#include "util/net_common.h"
#include "util/cfgfile.h"
#include "util/util.h"
#include "util/log.h"
#include "util/threads.h"

/** Component: Prefix */
static const char PFX[] = "INPUT";

/** Configuration: host to connect to */
static char cfgHost[NI_MAXHOST];
/** Configuration: port to connect to */
static uint_fast16_t cfgPort;

static bool checkCfg(const struct CFG_Section * sect);

/** Configuration descriptor */
static const struct CFG_Section cfgDesc = {
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

/** Interval in seconds between two reconnection attempts */
#define RECONNECT_INTERVAL 10

/** file descriptor for UART */
static sock_t sock;

static bool doConnect();
static void closeConn();

/** Register Radarcape input */
void RC_INPUT_register()
{
	CFG_registerSection(&cfgDesc);
}

/** Check configuration.
 * @param sect should be @cfgDesc
 * @return true if configuration is sane
 */
static bool checkCfg(const struct CFG_Section * sect)
{
	assert(sect == &cfgDesc);
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

/** Initialize Radarcape input */
void RC_INPUT_init()
{
	sock = SOCK_INVALID;
}

/** Disconnect Radarcape input */
void RC_INPUT_disconnect()
{
	closeConn();
}

/** Close connection */
static void closeConn()
{
	if (sock != SOCK_INVALID) {
		SOCK_shutdown(sock, SHUT_RDWR);
		SOCK_close(sock);
		sock = SOCK_INVALID;
	}
}

/** Reconnect Radarcape receiver until success */
void RC_INPUT_connect()
{
	while (!doConnect())
		sleepCancelable(RECONNECT_INTERVAL);
}

/** Connect to host.
 * @return true upon success
 */
static bool doConnect()
{
	closeConn();

	sock = NETC_connect(PFX, cfgHost, cfgPort);
	return sock != SOCK_INVALID;
}

/** Read from Radarcape receiver.
 * @param buf buffer to read into
 * @param bufLen buffer size
 * @return used buffer length or 0 on failure
 */
size_t RC_INPUT_read(uint8_t * buf, size_t bufLen)
{
	ssize_t rc = SOCK_recvCancelable(sock, buf, bufLen, 0);
	if (unlikely(rc < 0)) {
		LOG_errnet(LOG_LEVEL_WARN, PFX, "Could not receive from network");
		closeConn();
		return 0;
	} else {
		return rc;
	}
}

/** Write to Radarcape receiver.
 * @param buf buffer to be written
 * @param bufLen length of buffer
 * @return number of bytes written or 0 on failure
 */
size_t RC_INPUT_write(uint8_t * buf, size_t bufLen)
{
	ssize_t rc = SOCK_sendCancelable(sock, buf, bufLen, 0);
	if (unlikely(rc <= 0)) {
		LOG_errnet(LOG_LEVEL_WARN, PFX, "Could not send to network");
		return 0;
	}
	return rc;
}
