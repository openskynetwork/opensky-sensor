#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <input.h>
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
	while(!doConnect())
		sleep(CFG_config.input.reconnectInterval);
}

static bool doConnect()
{
	if (sock != -1)
		closeConn();

	sock = NETC_connect("INPUT", CFG_config.input.host, CFG_config.input.port);
	return sock != -1;
}

size_t INPUT_read(uint8_t * buf, size_t bufLen)
{
	ssize_t rc = recv(sock, buf, bufLen, 0);
	if (rc < 0) {
		error(0, errno, "INPUT: recv failed");
		closeConn();
		return 0;
	} else {
		return rc;
	}
}

size_t INPUT_write(uint8_t * buf, size_t bufLen)
{
	ssize_t rc = send(sock, buf, bufLen, MSG_NOSIGNAL);
	if (rc <= 0) {
		error(0, errno, "INPUT: send failed");
		return 0;
	}
	return rc;
}
