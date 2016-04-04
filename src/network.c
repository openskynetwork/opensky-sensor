/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <network.h>
#include <net_common.h>
#include <statistics.h>
#include <cfgfile.h>
#include <threads.h>
#include <pthread.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

/** Networking socket */
static int sock;
/** Mutex for all shared variables */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
/** Condition for changes in connection status */
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

/** Connection status */
enum CONN_STATE {
	/** Disconnected */
	CONN_STATE_DISCONNECTED,
	/** Connected */
	CONN_STATE_CONNECTED,
	/** Connection in transit (from connected -> disconnected) */
	CONN_STATE_TRANSIENT
};

/** Connection state */
static enum CONN_STATE connState = CONN_STATE_DISCONNECTED;

static void mainloop();
static bool tryConnect();
static void emitDisconnect();
static bool trySend(const void * buf, size_t len);
static bool sendSerial();

struct Component NET_comp = {
	.description = "NET",
	.main = &mainloop
};

static void cleanup()
{
	pthread_mutex_unlock(&mutex);
}

/** Mainloop for network: (re)established network connection on failure */
static void mainloop()
{
	pthread_mutex_lock(&mutex);
	CLEANUP_PUSH(&cleanup, NULL);
	while (true) {
		assert (connState == CONN_STATE_DISCONNECTED);
		while (!tryConnect()) {
			++STAT_stats.NET_connectsFail;
			sleep(CFG_config.net.reconnectInterval);
		}

		/* connection established */
		++STAT_stats.NET_connectsSuccess;

		connState = CONN_STATE_CONNECTED;
		pthread_cond_broadcast(&cond);

		sendSerial();

		/* wait for failure */
		while (connState != CONN_STATE_DISCONNECTED)
			pthread_cond_wait(&cond, &mutex);
	}
	CLEANUP_POP();
}

static bool tryConnect()
{
	sock = NETC_connect("NET", CFG_config.net.host, CFG_config.net.port);
	if (sock < 0)
		sock = -1;
	return sock != -1;
}

static void emitDisconnect()
{
	pthread_mutex_lock(&mutex);
	CLEANUP_PUSH(&cleanup, NULL);
	switch (connState) {
	case CONN_STATE_DISCONNECTED:
		break;
	case CONN_STATE_CONNECTED:
		shutdown(sock, SHUT_RDWR);
		connState = CONN_STATE_TRANSIENT;
		break;
	case CONN_STATE_TRANSIENT:
		close(sock);
		sock = -1;
		connState = CONN_STATE_DISCONNECTED;
		pthread_cond_broadcast(&cond);
	}
	CLEANUP_POP();
}

void NET_waitConnected()
{
	pthread_mutex_lock(&mutex);
	CLEANUP_PUSH(&cleanup, NULL);
	while (connState != CONN_STATE_CONNECTED)
		pthread_cond_wait(&cond, &mutex);
	CLEANUP_POP();
}

/** Send some data through the socket.
 * \param data data to be sent
 * \param len length of data
 * \return true if sending succeeded, false otherwise (e.g. connection lost)
 */
static bool trySend(const void * buf, size_t len)
{
	const char * ptr = buf;
	const char * end = ptr + len;

	do {
		ssize_t rc = send(sock, ptr, len, MSG_NOSIGNAL);
		if (rc <= 0) {
			NOC_fprintf(stderr, "NET: could not send: %s\n",
				rc == 0 || errno == EPIPE ? "Connection lost" :
				strerror(errno));
			++STAT_stats.NET_msgsFailed;
			emitDisconnect();
			return false;
		}
		ptr += rc;
	} while(ptr != end);

	++STAT_stats.NET_msgsSent;

	return true;
}

ssize_t NET_receive(uint8_t * buf, size_t len)
{
	ssize_t rc = recv(sock, buf, len, 0);
	if (rc <= 0) {
		NOC_fprintf(stderr, "NET: could not receive: %s\n",
			rc == 0 ? "Connection lost" : strerror(errno));
		++STAT_stats.NET_msgsRecvFailed;
		emitDisconnect();
	}
	return rc;
}

/** Send an ADSB frame to the server.
 * \return true if sending succeeded, false otherwise (e.g. connection lost)
 */
bool NET_sendFrame(const struct ADSB_Frame * frame)
{
	return trySend(frame->raw, frame->raw_len);
}

/** Send a timeout message to the server.
 * \return true if sending succeeded, false otherwise (e.g. connection lost)
 */
bool NET_sendTimeout()
{
	char buf[] = { '\x1a', '\x36' };
	++STAT_stats.NET_keepAlives;
	return trySend(buf, sizeof buf);
}

/** Send the serial number of the device to the server.
 * \return true if sending succeeded, false otherwise (e.g. connection lost)
 */
static bool sendSerial()
{
	char buf[10] = { '\x1a', '\x35' };

	union {
		uint8_t ca[4];
		uint32_t serial;
	} serial;
	serial.serial = htobe32(CFG_config.dev.serial);

	char * cur = buf + 2;

	uint_fast32_t n;
	for (n = 0; n < 4; ++n)
		if ((*cur++ = serial.ca[n]) == '\x1a')
			*cur++ = '\x1a';

	return trySend(buf, cur - buf);
}
