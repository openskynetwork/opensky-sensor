/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gps_input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <error.h>
#include <termios.h>
#include <unistd.h>
#include <sys/socket.h>
#include <net_common.h>
#include <cfgfile.h>
#include <util.h>

/** socket descriptor */
static int sock;

static bool doConnect();
static void closeConn();

void GPS_INPUT_init()
{
	sock = -1;
}

void GPS_INPUT_destruct()
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
		sleep(CFG_config.gps.reconnectInterval);
}

static bool doConnect()
{
	closeConn();

	sock = NETC_connect("GPS", CFG_config.gps.host, CFG_config.gps.port);
	return sock != -1;
}

size_t GPS_INPUT_read(uint8_t * buf, size_t bufLen)
{
	ssize_t rc = recv(sock, buf, bufLen, 0);
	if (unlikely(rc < 0)) {
		error(0, errno, "GPS: recv failed");
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
