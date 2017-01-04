/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

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
#include "trimble_input.h"
#include "util/net_common.h"
#include "util/cfgfile.h"
#include "util/util.h"
#include "util/log.h"
#include "util/threads.h"

/** Component: Prefix */
static const char PFX[] = "TRIMBLE";

/** Interval in seconds between two reconnection attempts */
#define RECONNECT_INTERVAL 10

/** Configuration: Host name to connect to */
static char cfgHost[NI_MAXHOST];
/** Configuration: Port to connect to */
static uint_fast16_t cfgPort;

/** socket descriptor */
static sock_t sock;

static bool checkCfg(const struct CFG_Section * sect);
static bool doConnect();
static void closeConn();

/** Configuration Descriptor */
static const struct CFG_Section cfgDesc = {
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

/** Register Trimble input */
void TRIMBLE_INPUT_register()
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
		LOG_log(LOG_LEVEL_ERROR, PFX, "NET.host is missing");
		return false;
	}
	if (cfgPort == 0) {
		LOG_log(LOG_LEVEL_ERROR, PFX, "NET.port = 0");
		return false;
	}
	return true;
}

/** Initialize Trimble input */
void TRIMBLE_INPUT_init()
{
	sock = SOCK_INVALID;
}

/** Disconnect Trimble input */
void TRIMBLE_INPUT_disconnect()
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

/** Reconnect Trimble input until success */
void TRIMBLE_INPUT_connect()
{
	while (!doConnect())
		sleepCancelable(RECONNECT_INTERVAL);
}

/** Connect to network.
 * @return true upon success
 */
static bool doConnect()
{
	closeConn();

	sock = NETC_connect(PFX, cfgHost, cfgPort);
	return sock != SOCK_INVALID;
}

/** Read from Trimble receiver.
 * @param buf buffer to read into
 * @param bufLen buffer size
 * @return used buffer length or 0 on failure
 */
size_t TRIMBLE_INPUT_read(uint8_t * buf, size_t bufLen)
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

/** Write to Trimble receiver.
 * @param buf buffer to be written
 * @param bufLen length of buffer
 * @return number of bytes written or 0 on failure
 */
size_t TRIMBLE_INPUT_write(const uint8_t * buf, size_t bufLen)
{
	/* ignore */
	return bufLen;
}
