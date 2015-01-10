#include <network.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <error.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <endian.h>

static int sock = -1;

/** Try to connect to a TCP server.
 * \param server server name or address
 * \param port port to connect to
 * \return true if connection attempt succeeded, false otherwise
 */
bool NET_connect(const char * server, int port)
{
	if (sock >= 0) {
		shutdown(sock, SHUT_RDWR);
		close(sock);
		sock = -1;
	}

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		error(-1, errno, "socket");

	struct hostent * host = gethostbyname(server);
	if (!host) {
		fprintf(stderr, "NET: could not resolve '%s': %s\n", server,
			hstrerror(h_errno));
		return false;
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	memcpy(&addr.sin_addr.s_addr, host->h_addr, host->h_length);
	addr.sin_port = htons(port);

	int r = connect(sock, (struct sockaddr*)&addr, sizeof addr);
	if (r < 0) {
		fprintf(stderr, "NET: could not connect to '%s:%d': %s\n", server, port,
			strerror(errno));
		return false;
	}

	printf("NET: connected to '%s:%d'\n", server, port);
	return true;
}

/** Send some data through the socket.
 * \param data data to be sent
 * \param len length of data
 * \return true if sending succeeded, false otherwise (e.g. connection lost)
 */
static inline bool sendData(const void * data, size_t len)
{
	ssize_t r = send(sock, data, len, MSG_NOSIGNAL);
	if (r < 0) {
		fprintf(stderr, "NET: could not send: %s\n", strerror(errno));
		return false;
	}
	return true;
}

/** Send the serial number of the device to the server.
 * \return true if sending succeeded, false otherwise (e.g. connection lost)
 */
bool NET_sendSerial(uint32_t serial)
{
	char buf[10] = { '\x1a', '\x35' };

	uint8_t ca[4];
	*(uint32_t*)ca = htobe32(serial);

	char * cur = buf + 2;

	uint32_t n;
	for (n = 0; n < 4; ++n)
		if ((*cur++ = ca[n]) == '\x1a')
			*cur++ = '\x1a';

	return sendData(buf, cur - buf);
}

/** Send an ADSB frame to the server.
 * \return true if sending succeeded, false otherwise (e.g. connection lost)
 */
bool NET_sendFrame(const struct ADSB_Frame * frame)
{
	/*char buf[250];
	size_t len = snprintf(buf, 250 - 29,
		"Mode-S long: mlat %15" PRIu64 ", level %+3" PRIi8 ": ",
		frame->mlat, frame->siglevel);
	int i;
	char * p = buf + len - 2;
	for (i = 0; i < 14; ++i)
		snprintf(p += 2, 3, "%02x", frame->payload[i]);
	len += 28;
	buf[len++] = '\n';
	return sendData(buf, len);*/
	return sendData(frame->raw, frame->raw_len);
}

/** Send a timeout message to the server.
 * \return true if sending succeeded, false otherwise (e.g. connection lost)
 */
bool NET_sendTimeout()
{
	char buf[] = { '\x1a', '\x36' };
	return sendData(buf, sizeof buf);
}
