/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <pthread.h>
#include <sys/socket.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <endian.h>
#include <stdbool.h>
#include "network.h"
#include "util/log.h"
#include "util/net_common.h"
#include "util/statistics.h"
#include "util/cfgfile.h"
#include "util/threads.h"

//#define DEBUG

static const char PFX[] = "NET";

#define RECONNET_INTERVAL 10

static char cfgHost[NI_MAXHOST];
static uint_fast16_t cfgPort;

static bool checkCfg(const struct CFG_Section * sect);

static const struct CFG_Section cfg = {
	.name = "NETWORK",
	.check = &checkCfg,
	.n_opt = 2,
	.options = {
		{
			.name = "Host",
			.type = CFG_VALUE_TYPE_STRING,
			.var = cfgHost,
			.maxlen = sizeof(cfgHost),
			.def = {
#ifdef LOCAL_FILES
				.string = "localhost"
#else
				.string = "collector.opensky-network.org"
#endif
			}
		},
		{
			.name = "Port",
			.type = CFG_VALUE_TYPE_PORT,
			.var = &cfgPort,
			.def = { .port = 10004 }
		}
	}
};

/** Networking socket */
static int recvsock;

static int sendsock;

/** Mutex for all shared variables */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
/** Condition for changes in connection status */
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
/** Mutex for exclusive sending */
static pthread_mutex_t sendMutex = PTHREAD_MUTEX_INITIALIZER;

/** Connection status */
enum CONN_STATE {
	/** Disconnected */
	CONN_STATE_DISCONNECTED,
	/** Connected */
	CONN_STATE_CONNECTED,
	/** Special state when shutting down */
	CONN_STATE_SHUTDOWN
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

enum ACTION {
	ACTION_NONE,
	ACTION_RETRY
};

/** Connection state */
static enum CONN_STATE connState;

static enum TRANSIT_STATE transState;

static void mainloop();
static int tryConnect();
static enum ACTION emitDisconnect(enum EMIT_BY by);
static bool trySend(const void * buf, size_t len);

const struct Component NET_comp = {
	.description = PFX,
	.main = &mainloop,
	.config = &cfg,
	.dependencies = { NULL }
};

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

static void cleanup()
{
	pthread_mutex_unlock(&mutex);
}

static void cleanupMain()
{
	if (connState == CONN_STATE_CONNECTED) {
		switch (transState) {
		case TRANSIT_NONE:
		case TRANSIT_SEND:
			shutdown(recvsock, SHUT_RDWR);
			close(recvsock);
			recvsock = -1;
			break;
		case TRANSIT_RECV:
			shutdown(sendsock, SHUT_RDWR);
			close(sendsock);
			sendsock = -1;
			break;
		}
	}
	connState = CONN_STATE_SHUTDOWN;
	pthread_mutex_unlock(&mutex);
}

static void cleanupSend()
{
	pthread_mutex_unlock(&sendMutex);
}

/** Mainloop for network: (re)established network connection on failure */
static void mainloop()
{
	connState = CONN_STATE_DISCONNECTED;
	transState = TRANSIT_NONE;

	pthread_mutex_lock(&mutex);
	CLEANUP_PUSH(&cleanupMain, NULL);
	while (true) {
		assert (connState == CONN_STATE_DISCONNECTED);
		int sock;
		while ((sock = tryConnect(&sock)) == -1) {
			++STAT_stats.NET_connectsFail;
			sleep(RECONNET_INTERVAL);
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

		/* wait for failure */
		while (connState != CONN_STATE_DISCONNECTED)
			pthread_cond_wait(&cond, &mutex);
	}
	CLEANUP_POP();
}

static int tryConnect()
{
	int sock = NETC_connect("NET", cfgHost, cfgPort);
	if (sock < 0)
		sock = -1;
	return sock;
}

static inline void logDisconnect()
{
	LOG_log(LOG_LEVEL_INFO, PFX, "Connection lost");
}

static enum ACTION emitDisconnect(enum EMIT_BY by)
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
	LOG_logf(LOG_LEVEL_DEBUG, PFX, "Failure detected by %s, Connected = %d, "
		"Transit = %s",
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
			 * see the failure) */
			logDisconnect();
			shutdown(*mysock, SHUT_RDWR);
			*mysock = -1;
			transState = by;
			connState = CONN_STATE_DISCONNECTED;
			pthread_cond_broadcast(&cond);
		} else if (transState == (enum TRANSIT_STATE)by) {
			/* we were connected, but the leader reported another failure
			 * while the follower did not recognize the first failure yet
			 *  -> close the socket */
			logDisconnect();
			shutdown(*mysock, SHUT_RDWR);
			close(*mysock);
			*mysock = -1;
			connState = CONN_STATE_DISCONNECTED;
			pthread_cond_broadcast(&cond);
		} else {
			/* we are connected and the follower has seen the failure
			 * -> close the stale socket and synchronize with the leader */
			close(*mysock);
			*mysock = othsock;
			transState = TRANSIT_NONE;
		}
	} else if (connState != CONN_STATE_SHUTDOWN &&
		transState != (enum TRANSIT_STATE)by) {
		assert (transState != TRANSIT_NONE);
		/* we have not been reconnected yet, but the follower has seen the
		 * failure -> close the stale socket */
		close(*mysock);
		*mysock = -1;
		transState = TRANSIT_NONE;
	}
	CLEANUP_POP();

	return connState == CONN_STATE_CONNECTED ? ACTION_RETRY : ACTION_NONE;
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
		ssize_t rc = send(sendsock, ptr, len, MSG_NOSIGNAL);
		if (rc <= 0) {
#ifdef DEBUG
			LOG_logf(LOG_LEVEL_DEBUG, PFX, "could not send: %s",
				rc == 0 || errno == EPIPE ? "Connection lost" :
				strerror(errno));
#endif
			++STAT_stats.NET_msgsFailed;
			emitDisconnect(EMIT_BY_SEND);
			return false;
		}
		ptr += rc;
	} while (ptr != end);

	++STAT_stats.NET_msgsSent;

	return true;
}

bool NET_send(const void * buf, size_t len)
{
	pthread_mutex_lock(&sendMutex);
	bool rc;
	CLEANUP_PUSH(&cleanupSend, NULL);
	rc = trySend(buf, len);
	CLEANUP_POP();
	return rc;
}

ssize_t NET_receive(uint8_t * buf, size_t len)
{
	do {
		ssize_t rc = recv(recvsock, buf, len, 0);
		if (rc > 0)
			return rc;
#ifdef DEBUG
		LOG_logf(LOG_LEVEL_INFO, PFX, "could not receive: %s",
			rc == 0 ? "Connection lost" : strerror(errno));
#endif
		++STAT_stats.NET_msgsRecvFailed;
	} while (emitDisconnect(EMIT_BY_RECV) == ACTION_RETRY);
	return 0;
}
