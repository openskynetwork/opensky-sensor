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
#include <netdb.h>
#include "network.h"
#include "util/log.h"
#include "util/net_common.h"
#include "util/statistics.h"
#include "util/cfgfile.h"
#include "util/threads.h"
#include "util/util.h"

/* Define to 1 to enable debugging messages */
//#define DEBUG 1

/** Component: Prefix */
static const char PFX[] = "NET";

/** Interval to sleep between reconnection attempts */
#define RECONNET_INTERVAL 10

/** Networking socket: receiving socket */
static int recvsock;
/** Networking socket: sending socket */
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

/** Secondary connection status when sender and receiver are unsynchronized */
enum TRANSIT_STATE {
	/** Sender and receiver both know of the current connection status */
	TRANSIT_NONE,
	/** Receiver is the leader, sender is unsynchronized */
	TRANSIT_SEND,
	/** Sender is the leader, receiver is unsynchronized */
	TRANSIT_RECV
};

/** If an connection failure occurs, we must know whether the sender or the
 * receiver has seen the failure*/
enum EMIT_BY {
	/** Failure detected by receiver */
	EMIT_BY_RECV = TRANSIT_SEND,
	/** Failure detected by sender */
	EMIT_BY_SEND = TRANSIT_RECV
};

/** Action to be taken if sending/receiving fails -> it could be, that the
 * connection has already been reestablished. Try again in that case.
 */
enum ACTION {
	/** No action: just fail */
	ACTION_NONE,
	/** Retry */
	ACTION_RETRY
};

/** Current connection state */
static enum CONN_STATE connState;

/** Current transition state */
static enum TRANSIT_STATE transState;

/** Configuration: host name*/
static char cfgHost[NI_MAXHOST];
/** Configuration: tcp port */
static uint_fast16_t cfgPort;

static bool checkCfg(const struct CFG_Section * sect);

/** Statistics */
static struct NET_Statistics stats;
/** Statistics: last time a connection attempt was successful */
static time_t onlineSince;

static void mainloop();
static int tryConnect();
static enum ACTION emitDisconnect(enum EMIT_BY by);
static bool trySend(const void * buf, size_t len);
static void resetStats();

/** Configuration: Descriptor */
static const struct CFG_Section cfgDesc = {
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



/** Component: Descriptor */
const struct Component NET_comp = {
	.description = PFX,
	.main = &mainloop,
	.config = &cfgDesc,
	.onReset = &resetStats,
	.dependencies = { NULL }
};

/** Configuration: Check configuration.
 * @param sect configuration section, should be @cfgDesc
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

/** Mainloop for network: (re)established network connection on failure */
static void mainloop()
{
	connState = CONN_STATE_DISCONNECTED;
	transState = TRANSIT_NONE;

	pthread_mutex_lock(&mutex);
	CLEANUP_PUSH(&cleanupMain, NULL);
	while (true) {
		assert(connState == CONN_STATE_DISCONNECTED);
		int sock;
		++stats.connectionAttempts;
		while ((sock = tryConnect(&sock)) == -1) {
			++stats.disconnects;
			sleep(RECONNET_INTERVAL);
			++stats.connectionAttempts;
		}

		/* here: connection established */

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

		onlineSince = time(NULL);

		/* wait for failure */
		while (connState != CONN_STATE_DISCONNECTED)
			pthread_cond_wait(&cond, &mutex);
		++stats.disconnects;
	}
	CLEANUP_POP();
}

/** Try to connect to the OpenSky Network.
 * @return socket descriptor or -1 on failure.
 */
static int tryConnect()
{
	int sock = NETC_connect("NET", cfgHost, cfgPort);
	if (sock < 0)
		sock = -1;
	return sock;
}

/** Log a disconnection event */
static inline void logDisconnect()
{
	LOG_log(LOG_LEVEL_INFO, PFX, "Connection lost");
}

/** Upon network failure: determine the actions to be taken, depending on
 * who has detected the failure and the current state
 * @param by failure was detected by sender or receiver
 * @return action to be taken by sender resp. receiver
 */
static enum ACTION emitDisconnect(enum EMIT_BY by)
{
	enum ACTION act;
	CLEANUP_PUSH_LOCK(&mutex);

	/* determine socket */
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
		/* we are / were connected */
		if (transState == TRANSIT_NONE) {
			/* we were normally connected -> we have a new leader */
			/* shutdown the socket (but leave it open, so the follower can
			 * see the failure) */
			logDisconnect();
			shutdown(*mysock, SHUT_RDWR);
			*mysock = -1;
			transState = (enum TRANSIT_STATE)by;
			connState = CONN_STATE_DISCONNECTED;
			stats.onlineSecs += time(NULL) - onlineSince;
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
			stats.onlineSecs += time(NULL) - onlineSince;
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
		assert(transState != TRANSIT_NONE);
		/* we have not been reconnected yet, but the follower has seen the
		 * failure -> close the stale socket */
		close(*mysock);
		*mysock = -1;
		transState = TRANSIT_NONE;
	}
	act = connState == CONN_STATE_CONNECTED ? ACTION_RETRY : ACTION_NONE;
	CLEANUP_POP();

	return act;
}

/** Wait for reconnection */
void NET_waitConnected()
{
	CLEANUP_PUSH_LOCK(&mutex);
	while (connState != CONN_STATE_CONNECTED)
		pthread_cond_wait(&cond, &mutex);
	CLEANUP_POP();
}

/** Force a reconnection to the network.
 * Must only be called by the thread which normally calls @NET_send.
 */
void NET_forceDisconnect()
{
	emitDisconnect(EMIT_BY_SEND);
}

/** Check if the network is still connected.
 * Must only be called by the thread which normally calls @NET_send.
 * @return true if the network is connected
 */
bool NET_checkConnected()
{
	pthread_mutex_lock(&mutex);
	bool emit = connState != CONN_STATE_CONNECTED ||
		transState == TRANSIT_SEND;
	pthread_mutex_unlock(&mutex);
	if (emit) {
		emitDisconnect(EMIT_BY_SEND);
		return false;
	} else {
		return true;
	}
}

/** Send some data through the socket.
 * @param data data to be sent
 * @param len length of data
 * @return true if sending succeeded, false otherwise (e.g. connection lost)
 */
static bool trySend(const void * buf, size_t len)
{
	const char * ptr = buf;
	const char * end = ptr + len;

	do {
		ssize_t rc = send(sendsock, ptr, len, MSG_NOSIGNAL);
		if (rc <= 0) {
			/* report failure */
#ifdef DEBUG
			LOG_logf(LOG_LEVEL_DEBUG, PFX, "could not send: %s",
				rc == 0 || errno == EPIPE ? "Connection lost" :
				strerror(errno));
#endif
			emitDisconnect(EMIT_BY_SEND);
			return false;
		}
		ptr += rc;
	} while (ptr != end);

	stats.bytesSent += len;

	return true;
}

/** Send some data through the socket.
 * @param buf buffer to be sent
 * @param len length of buffer
 * @return true if sending succeeded, false on network failure
 */
bool NET_send(const void * buf, size_t len)
{
	bool rc;
	CLEANUP_PUSH_LOCK(&sendMutex);
	rc = trySend(buf, len);
	CLEANUP_POP();
	return rc;
}

/** Receive some data from the socket.
 * @param buf buffer to receive into
 * @param len size of buffer
 * @return number of bytes received or 0 upon failure
 */
ssize_t NET_receive(uint8_t * buf, size_t len)
{
	do {
		ssize_t rc = recv(recvsock, buf, len, 0);
		if (rc > 0) {
			stats.bytesReceived += rc;
			return rc;
		}
#ifdef DEBUG
		LOG_logf(LOG_LEVEL_INFO, PFX, "could not receive: %s",
			rc == 0 ? "Connection lost" : strerror(errno));
#endif
	} while (emitDisconnect(EMIT_BY_RECV) == ACTION_RETRY);
	return 0;
}

static void resetStats()
{
	pthread_mutex_lock(&mutex);
	memset(&stats, 0, sizeof stats);
	pthread_mutex_unlock(&mutex);
}

void NET_getStatistics(struct NET_Statistics * statistics)
{
	/* copy unlocked. It's o.k. to have inconsistent statistics */
	memcpy(statistics, &stats, sizeof *statistics);
	statistics->isOnline = connState == CONN_STATE_CONNECTED;
	if (statistics->isOnline)
		statistics->onlineSecs += time(NULL) - onlineSince;
}
