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

/** network configuration */
static const struct CFG_NET * config;
/** serial number */
static uint32_t serialNumber;

static inline void emitDisconnect();
static bool doConnect();
static bool sendSerial();
static inline bool sendData(const void * data, size_t len);

/** Initialize Network component.
 * \param cfg pointer to buffer configuration, see cfgfile.h
 * \param serial serial number of device
 */
void NET_init(const struct CFG_NET * cfg, uint32_t serial)
{
	config = cfg;
	serialNumber = serial;
}

/** Mainloop for network: (re)established network connection on failure */
void NET_main()
{
	connected = false;
	reconnect = true;
	reconnectedRecv = false;
	reconnectedSend = false;
	inRecv = false;
	inSend = false;

	pthread_mutex_lock(&mutex);
	while (true) {
		/* connect to the server */
		while (!doConnect()) {
			pthread_mutex_unlock(&mutex);
			sleep(config->reconnectInterval); /* retry in case of failure */
			pthread_mutex_lock(&mutex);
		}

		/* connection established */
		if (!sendSerial())
			continue;

		/* signalize sender / receiver */
		connected = true;
		reconnectedRecv = true;
		reconnectedSend = true;
		reconnect = false;
		pthread_cond_broadcast(&condConnected);

		/* wait for failure */
		while (!reconnect)
			pthread_cond_wait(&condReconnect, &mutex);
	}
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

	sock = NETC_connect("NET", config->host, config->port);
	return sock != -1;
}

/** Synchronize sending thread: wait for a connection */
void NET_sync_send()
{
#ifdef DEBUG
	puts("NET: send synchronizing");
#endif
	pthread_mutex_lock(&mutex);
	while (!connected)
		pthread_cond_wait(&condConnected, &mutex);
	reconnectedSend = false;
#ifdef DEBUG
	puts("NET: send synchronized");
#endif
	pthread_mutex_unlock(&mutex);
}

/** Synchronize receiving thread: wait for a connection */
void NET_sync_recv()
{
#ifdef DEBUG
	puts("NET: recv synchronizing");
#endif
	pthread_mutex_lock(&mutex);
	while (!connected)
		pthread_cond_wait(&condConnected, &mutex);
	reconnectedRecv = false;
#ifdef DEBUG
	puts("NET: recv synchronized");
#endif
	pthread_mutex_unlock(&mutex);
}

static inline void emitDisconnect()
{
	shutdown(sock, SHUT_RDWR);
#ifdef DEBUG
	printf("NET: Disconnect Event. Connected: %d, inRecv: %d, inSend: %d -> ",
		connected, inRecv, inSend);
#endif
	if (!connected || (!inRecv && !inSend)) {
		puts("reconnect");
		reconnect = true;
		pthread_cond_signal(&condReconnect);
	} else {
		puts("wait");
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
		fprintf(stderr, "NET: could not send: %s\n",
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
	pthread_mutex_lock(&mutex);
	if (!connected || reconnectedSend) {
		fprintf(stderr, "NET: could not send: %s\n",
			!connected ? "not connected" : "unsynchronized");
		pthread_mutex_unlock(&mutex);
		++STAT_stats.NET_framesFailed;
		return false;
	} else {
		inSend = true;
		pthread_mutex_unlock(&mutex);
		ssize_t rc = send(sock, data, len, MSG_NOSIGNAL);
		pthread_mutex_lock(&mutex);
		inSend = false;
		if (rc <= 0) {
			fprintf(stderr, "NET: could not send: %s\n",
				rc == 0 ? "Connection lost" : strerror(errno));
			++STAT_stats.NET_framesFailed;
			emitDisconnect();
			pthread_mutex_unlock(&mutex);
			return false;
		} else {
			if (!connected) {
				/* Receiver thread detected connection failure, but send
				 * was successful. But now, the network thread waits for
				 * the sender thread to acknowledge the failure, so we do it */
				emitDisconnect();
			}
			pthread_mutex_unlock(&mutex);
			++STAT_stats.NET_framesSent;
			return true;
		}
	}
}

/** Send the serial number of the device to the server.
 * \return true if sending succeeded, false otherwise (e.g. connection lost)
 */
static bool sendSerial()
{
	char buf[10] = { '\x1a', '\x35' };

	uint8_t ca[4];
	*(uint32_t*)ca = htobe32(serialNumber);

	char * cur = buf + 2;

	uint32_t n;
	for (n = 0; n < 4; ++n)
		if ((*cur++ = ca[n]) == '\x1a')
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
	pthread_mutex_lock(&mutex);
	if (!connected || reconnectedRecv) {
		fprintf(stderr, "NET: could not receive: %s\n",
			!connected ? "not connected" : "unsynchronized");
		pthread_mutex_unlock(&mutex);
		++STAT_stats.NET_recvFailed;
		return false;
	} else {
		inRecv = true;
		pthread_mutex_unlock(&mutex);
		ssize_t rc = recv(sock, buf, 128, 0);
		pthread_mutex_lock(&mutex);
		inRecv = false;
		if (rc <= 0) {
			fprintf(stderr, "NET: could not receive: %s\n",
				rc == 0 ? "Connection lost" : strerror(errno));
			++STAT_stats.NET_recvFailed;
			emitDisconnect();
		} else if (!connected) {
			rc = -1;
			emitDisconnect();
		}
		pthread_mutex_unlock(&mutex);
		return rc;
	}
}
