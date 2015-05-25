#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <network.h>
#include <net_common.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <error.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <endian.h>
#include <statistics.h>
#include <pthread.h>
#include <threads.h>
#include <cfgfile.h>

#define DEBUG

/** Networking socket */
static volatile int sock = -1;

/** Flag: is socket connected */
static volatile bool connected;
/** Flag: reconnect needed */
static volatile bool reconnect;
/** Flag: socket has been reconnected (for receiver) */
static volatile bool reconnectedRecv;
/** Flag: socket has been reconnected (for sender) */
static volatile bool reconnectedSend;
/** Flag: currently in inexclusive region (recv) */
static volatile bool inRecv;
/** Flag: currently in inexclusive region (send) */
static volatile bool inSend;

/** Mutex for all shared variables */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
/** Condition for connected flag */
static pthread_cond_t condConnected = PTHREAD_COND_INITIALIZER;
/** Condition for reconnect flag */
static pthread_cond_t condReconnect = PTHREAD_COND_INITIALIZER;

static void mainloop();

struct Component NET_comp = {
	.description = "NET",
	.main = &mainloop
};

static inline void emitDisconnect();
static bool doConnect();
static bool sendSerial();
static inline bool sendData(const void * data, size_t len);

static void cleanup(bool * locked)
{
	if (*locked) {
		pthread_cond_broadcast(&condReconnect);
		pthread_mutex_unlock(&mutex);
	}
}

/** Mainloop for network: (re)established network connection on failure */
static void mainloop()
{
	connected = false;
	reconnect = true;
	reconnectedRecv = false;
	reconnectedSend = false;
	inRecv = false;
	inSend = false;

	bool locked = false;

	pthread_mutex_lock(&mutex);
	locked = true;
	CLEANUP_PUSH(&cleanup, &locked);
	while (NET_comp.run) {
		/* connect to the server */
		while (NET_comp.run && !doConnect()) {
			/* retry in case of failure */
			pthread_mutex_unlock(&mutex);
			locked = false;
			sleep(CFG_config.net.reconnectInterval);
			locked = true;
			pthread_mutex_lock(&mutex);
			++STAT_stats.NET_connectsFail;
		}

		/* connection established */
		if (!sendSerial()) {
			++STAT_stats.NET_connectsFail;
			continue;
		}

		/* signalize sender / receiver */
		connected = true;
		reconnectedRecv = true;
		reconnectedSend = true;
		reconnect = false;
		pthread_cond_broadcast(&condConnected);

		++STAT_stats.NET_connectsSuccess;

		/* wait for failure */
		while (NET_comp.run && !reconnect)
			pthread_cond_wait(&condReconnect, &mutex);
	}
	CLEANUP_POP();
}

/** Try to connect to the server.
 * \return true if connection attempt succeeded, false otherwise
 */
static bool doConnect()
{
	/* close socket first if already opened */
	if (sock >= 0) {
		shutdown(sock, SHUT_RDWR);
		close(sock);
		sock = -1;
	}

	sock = NETC_connect("NET", CFG_config.net.host, CFG_config.net.port);
	return sock != -1;
}

static void unlock()
{
	pthread_mutex_unlock(&mutex);
}

static void unlockIfLocked(bool * locked)
{
	if (*locked)
		pthread_mutex_unlock(&mutex);
}

/** Synchronize sending thread: wait for a connection */
void NET_sync_send()
{
#ifdef DEBUG
	NOC_puts("NET: send synchronizing");
#endif
	pthread_mutex_lock(&mutex);
	CLEANUP_PUSH(&unlock, NULL);
	while (!connected)
		pthread_cond_wait(&condConnected, &mutex);
	reconnectedSend = false;
#ifdef DEBUG
	NOC_puts("NET: send synchronized");
#endif
	CLEANUP_POP();
}

/** Synchronize receiving thread: wait for a connection */
void NET_sync_recv()
{
#ifdef DEBUG
	NOC_puts("NET: recv synchronizing");
#endif
	pthread_mutex_lock(&mutex);
	CLEANUP_PUSH(&unlock, NULL)
	while (!connected)
		pthread_cond_wait(&condConnected, &mutex);
	reconnectedRecv = false;
#ifdef DEBUG
	NOC_puts("NET: recv synchronized");
#endif
	CLEANUP_POP();
}

static inline void emitDisconnect()
{
	shutdown(sock, SHUT_RDWR);
#ifdef DEBUG
	NOC_printf("NET: Disconnect Event. Connected: %d, inRecv: %d, inSend: "
		"%d -> ", connected, inRecv, inSend);
#endif
	if (!connected || (!inRecv && !inSend)) {
		NOC_puts("reconnect");
		reconnect = true;
		pthread_cond_signal(&condReconnect);
	} else {
		NOC_puts("wait");
	}
	connected = false;
}


/** Send some data through the socket without locks or checks.
 * \param data data to be sent
 * \param len length of data
 * \return true if sending succeeded, false otherwise (e.g. connection lost)
 */
static inline bool sendDataUnlocked(const void * data, size_t len)
{
	ssize_t rc = send(sock, data, len, MSG_NOSIGNAL);
	if (rc <= 0) {
		NOC_fprintf(stderr, "NET: could not send: %s\n",
			rc == 0 ? "Connection lost" : strerror(errno));
		++STAT_stats.NET_framesFailed;
		emitDisconnect();
		return false;
	} else {
		++STAT_stats.NET_framesSent;
		return true;
	}
}

/** Send some data through the socket.
 * \param data data to be sent
 * \param len length of data
 * \return true if sending succeeded, false otherwise (e.g. connection lost)
 */
static inline bool sendData(const void * data, size_t len)
{
	bool ret;
	bool locked;

	pthread_mutex_lock(&mutex);
	locked = true;
	CLEANUP_PUSH(&unlockIfLocked, &locked);
	if (!connected || reconnectedSend) {
		NOC_fprintf(stderr, "NET: could not send: %s\n",
			!connected ? "not connected" : "unsynchronized");
		++STAT_stats.NET_framesFailed;
		ret = false;
	} else {
		inSend = true;
		pthread_mutex_unlock(&mutex);
		locked = false;
		ssize_t rc = send(sock, data, len, MSG_NOSIGNAL);
		locked = true;
		pthread_mutex_lock(&mutex);
		inSend = false;
		if (rc <= 0) {
			NOC_fprintf(stderr, "NET: could not send: %s\n",
				rc == 0 ? "Connection lost" : strerror(errno));
			++STAT_stats.NET_framesFailed;
			emitDisconnect();
			ret = false;
		} else {
			if (!connected) {
				/* Receiver thread detected connection failure, but send
				 * was successful. But now, the network thread waits for
				 * the sender thread to acknowledge the failure, so we do it */
				emitDisconnect();
			}
			++STAT_stats.NET_framesSent;
			ret = true;
		}
	}
	CLEANUP_POP();
	return ret;
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

	uint32_t n;
	for (n = 0; n < 4; ++n)
		if ((*cur++ = serial.ca[n]) == '\x1a')
			*cur++ = '\x1a';

	return sendDataUnlocked(buf, cur - buf);
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
	++STAT_stats.NET_keepAlives;
	return sendData(buf, sizeof buf);
}

ssize_t NET_receive(uint8_t * buf, size_t len)
{
	ssize_t ret;
	bool locked;
	pthread_mutex_lock(&mutex);
	locked = true;
	CLEANUP_PUSH(&unlockIfLocked, &locked);
	if (!connected || reconnectedRecv) {
		NOC_fprintf(stderr, "NET: could not receive: %s\n",
			!connected ? "not connected" : "unsynchronized");
		++STAT_stats.NET_recvFailed;
		ret = -1;
	} else {
		inRecv = true;
		pthread_mutex_unlock(&mutex);
		locked = false;
		ret = recv(sock, buf, 128, 0);
		locked = true;
		pthread_mutex_lock(&mutex);
		inRecv = false;
		if (ret <= 0) {
			NOC_fprintf(stderr, "NET: could not receive: %s\n",
					ret == 0 ? "Connection lost" : strerror(errno));
			++STAT_stats.NET_recvFailed;
			emitDisconnect();
		} else if (!connected) {
			ret = -1;
			emitDisconnect();
		}
	}
	CLEANUP_POP();
	return ret;
}
