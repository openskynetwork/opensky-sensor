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
static int recvsock;

static int sendsock;

/** Mutex for all shared variables */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
/** Condition for changes in connection status */
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

/** Connection status */
enum CONN_STATE {
	/** Disconnected */
	CONN_STATE_DISCONNECTED,
	/** Connected */
	CONN_STATE_CONNECTED
};

enum TRANSIT_STATE {
	TRANSIT_NONE,
	TRANSIT_SEND,
	TRANSIT_RECV
};

enum EMIT_BY {
	EMIT_BY_RECV = TRANSIT_SEND,
	EMIT_BY_SEND = TRANSIT_RECV
};

/** Connection state */
static enum CONN_STATE connState;

static enum TRANSIT_STATE transState;

static void mainloop();
static int tryConnect();
static void emitDisconnect(enum EMIT_BY by);
static bool trySendSock(int sock, const void * buf, size_t len);
static bool trySend(const void * buf, size_t len);
static bool sendSerial(int sock);

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
	connState = CONN_STATE_DISCONNECTED;
	transState = TRANSIT_NONE;

	pthread_mutex_lock(&mutex);
	CLEANUP_PUSH(&cleanup, NULL);
	while (true) {
		assert (connState == CONN_STATE_DISCONNECTED);
		int sock;
		while ((sock = tryConnect(&sock)) == -1) {
			++STAT_stats.NET_connectsFail;
			sleep(CFG_config.net.reconnectInterval);
		}

		/* connection established */
		++STAT_stats.NET_connectsSuccess;

		switch (transState) {
		case TRANSIT_NONE:
			recvsock = sendsock = sock;
			break;
		case TRANSIT_RECV:
			sendsock = sock;
			break;
		case TRANSIT_SEND:
			recvsock = sock;
			break;
		}

		connState = CONN_STATE_CONNECTED;
		pthread_cond_broadcast(&cond);

		if (!sendSerial(sock)) {
			/* special case: sending the serial number does not trigger the
			 * failure handling, so we have to do it manually here */
			shutdown(sock, SHUT_RDWR);
			close(sock);
			connState = CONN_STATE_DISCONNECTED;
		}

		/* wait for failure */
		while (connState != CONN_STATE_DISCONNECTED)
			pthread_cond_wait(&cond, &mutex);
	}
	CLEANUP_POP();
}

static int tryConnect()
{
	int sock = NETC_connect("NET", CFG_config.net.host, CFG_config.net.port);
	if (sock < 0)
		sock = -1;
	return sock;
}

static void emitDisconnect(enum EMIT_BY by)
{
	pthread_mutex_lock(&mutex);
	CLEANUP_PUSH(&cleanup, NULL);

	int * mysock;
	int othsock;

	if (by == EMIT_BY_RECV) {
		mysock = &recvsock;
		othsock = sendsock;
	} else {
		mysock = &sendsock;
		othsock = recvsock;
	}

#ifdef DEBUG
	NOC_printf("NET: Failure detected by %s, Connected = %d, Transit = %s\n",
		by == EMIT_BY_RECV ? "RECV" : "SEND",
		connState == CONN_STATE_CONNECTED ? 1 : 0,
		transState == TRANSIT_NONE ? "NONE" : transState == TRANSIT_RECV ?
			"RECV" : "SEND");
#endif

	if (connState == CONN_STATE_CONNECTED) {
		/* we were connected */
		if (transState == TRANSIT_NONE) {
			/* we were normally connected -> we have a new leader */
			/* shutdown the socket (but leave it open, so the follower can
			 * see the failure */
			shutdown(*mysock, SHUT_RDWR);
			transState = by;
			connState = CONN_STATE_DISCONNECTED;
			pthread_cond_broadcast(&cond);
		} else if (transState == (enum TRANSIT_STATE)by) {
			/* we were connected, but the leader reported another failure
			 * while the follower did not recognize the first failure yet
			 *  -> close the socket */
			shutdown(*mysock, SHUT_RDWR);
			close(*mysock);
			connState = CONN_STATE_DISCONNECTED;
			pthread_cond_broadcast(&cond);
		} else {
			/* we are connected and the follower has seen the failure
			 * -> close the stale socket and synchronize with the leader */
			close(*mysock);
			*mysock = othsock;
			transState = TRANSIT_NONE;
		}
	} else if (transState != (enum TRANSIT_STATE)by) {
		assert (transState != TRANSIT_NONE);
		/* we have not been reconnected yet, but the follower has seen the
		 * failure -> close the stale socket */
		close(*mysock);
		transState = TRANSIT_NONE;
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
static bool trySendSock(int sock, const void * buf, size_t len)
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
			return false;
		}
		ptr += rc;
	} while (ptr != end);

	++STAT_stats.NET_msgsSent;

	return true;
}

/** Send some data through the socket.
 * \param data data to be sent
 * \param len length of data
 * \return true if sending succeeded, false otherwise (e.g. connection lost)
 */
static bool trySend(const void * buf, size_t len)
{
	bool rc = trySendSock(sendsock, buf, len);
	if (!rc)
		emitDisconnect(EMIT_BY_SEND);
	return rc;
}

ssize_t NET_receive(uint8_t * buf, size_t len)
{
	ssize_t rc = recv(recvsock, buf, len, 0);
	if (rc <= 0) {
		NOC_fprintf(stderr, "NET: could not receive: %s\n",
			rc == 0 ? "Connection lost" : strerror(errno));
		++STAT_stats.NET_msgsRecvFailed;
		emitDisconnect(EMIT_BY_RECV);
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
static bool sendSerial(int sock)
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

	return trySendSock(sock, buf, cur - buf);
}
