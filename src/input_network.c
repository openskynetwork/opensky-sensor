/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/socket.h>
#include <net_common.h>
#include <cfgfile.h>
#include <util.h>

static const char PFX[] = "INPUT";

/** file descriptor for UART */
static int sock;

static bool doConnect();
static void closeConn();

void INPUT_init()
{
	sock = -1;
}

void INPUT_destruct()
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

void INPUT_connect()
{
	while (!doConnect())
		sleep(CFG_config.input.reconnectInterval);
}

static bool doConnect()
{
	closeConn();

	sock = NETC_connect(PFX, CFG_config.input.host, CFG_config.input.port);
	return sock != -1;
}

size_t INPUT_read(uint8_t * buf, size_t bufLen)
{
	ssize_t rc = recv(sock, buf, bufLen, 0);
	if (unlikely(rc < 0)) {
		LOG_errno(LOG_LEVEL_WARN, PFX, "recv failed");
		closeConn();
		return 0;
	} else {
		return rc;
	}
}

size_t INPUT_write(uint8_t * buf, size_t bufLen)
{
	ssize_t rc = send(sock, buf, bufLen, MSG_NOSIGNAL);
	if (unlikely(rc <= 0)) {
		LOG_errno(LOG_LEVEL_WARN, PFX, "send failed");
		return 0;
	}
	return rc;
}
