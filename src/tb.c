/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <tb.h>
#include <stdbool.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <network.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <proc.h>
#include <cfgfile.h>
#include <recv.h>

/* Packet format:
 * Format: |ID|Len|Payload|
 * Size:     2  2    var
 *
 * ID: packet type, encoded in big endian byte order
 * Len: length of packet in bytes, encoded in big endian byte order
 *      Max Length is 128 byte
 * Payload: packet payload, variable length
 *
 * Packet types:
 * * 0x0: start reverse shell
 *   Payload: - ip (encoded as 4 byte, big endian byte order)
 *            - port (encoded as 2 byte, big endian byte order)
 *   Size: 9
 * * 0x1: restart daemon, Size: 3
 * * 0x2: reboot, Size: 3
 * * 0x3: upgrade daemon and restart, Size: 3
 * * 0x4: upgrade system and reboot, Size: 3
 */

/** Packet header and payload pointer */
struct TB_Packet {
	/** frame type */
	uint_fast16_t type;
	/** frame length */
	uint_fast16_t len;

	/** pointer to payload */
	const uint8_t * payload;
};

/** argument vector of daemon, needed for restart */
static char ** daemonArgv;

static void construct(void * argv);
static void mainloop();

struct Component TB_comp = {
	.description = "TB",
	.construct = &construct,
	.main = &mainloop
};

static void processPacket(const struct TB_Packet * packet);

#ifdef TALKBACK
#ifdef STANDALONE
static void packetShell(const struct TB_Packet * packet);
static void packetRestartDaemon(const struct TB_Packet * packet);
#endif
#ifdef WITH_SYSTEMD
static void packetRebootSystem(const struct TB_Packet * packet);
#endif
#ifdef WITH_PACMAN
static void packetUpgradeDaemon(const struct TB_Packet * packet);
#endif
static void packetConfigureFilter(const struct TB_Packet * packet);
#endif

/** Packet processor function pointer */
typedef void (*PacketProcessor)(const struct TB_Packet*);

/** All packet processors in order of their type */
static PacketProcessor processors[] = {
#ifdef TALKBACK
#ifdef STANDALONE
	[0] = &packetShell,
	[1] = &packetRestartDaemon,
#endif
#ifdef WITH_SYSTEMD
	[2] = &packetRebootSystem,
#endif
#ifdef WITH_PACMAN
	[3] = &packetUpgradeDaemon,
#endif
	[4] = &packetConfigureFilter,
#endif
};

/** Initialize TB.
 * \param argv argument vector of the daemon
 */
static void construct(void * argv)
{
	daemonArgv = argv;
}

/** Mainloop of the TB. */
static void mainloop()
{
	uint8_t buf[128];
	size_t bufLen;

	while (true) {
		/* synchronize with receiver */
		NET_waitConnected();

		/* new connection, reset buffer */
		bufLen = 0;

		while (true) {
			while (bufLen >= 4) {
				/* enough data to read header */
				struct TB_Packet frame;
				uint16_t * buf16 = (uint16_t*)buf;
				frame.type = be16toh(buf16[0]);
				frame.len = be16toh(buf16[1]);
				if (frame.len > 128 || frame.len < 4) {
					/* sanity check failed, reset buffer */
					fprintf(stderr, "TB: Wrong packet format (type=%"
						PRIuFAST16 ", len=%" PRIuFAST16 "), resetting buffer\n",
						frame.type, frame.len);
					bufLen = 0;
					break;
				}
				if (bufLen >= frame.len) {
					/* complete packet: process it */
					frame.payload = buf + 4;
					processPacket(&frame);
					/* and clear it from buffer */
					bufLen -= frame.len;
					if (bufLen)
						memmove(buf, buf + frame.len, bufLen);
				} else {
					/* packet incomplete: more data needed */
					break;
				}
			}

			/* read (more) data from network into buffer */
			ssize_t len = NET_receive(buf + bufLen, (sizeof buf) - bufLen);
			if (len <= 0)
				break;
			bufLen += len;
		}
	}
}

/** Process a packet: call handler if exists.
 * \param packet packet to be processed
 */
static void processPacket(const struct TB_Packet * packet)
{
	static const uint_fast32_t n_processors = sizeof(processors)
		/ sizeof(*processors);

	if (packet->type > n_processors || !processors[packet->type]) {
		/* packet type unknown */
		fprintf(stderr, "TB: Unknown packet type (type = %" PRIuFAST16
			", len = %" PRIuFAST16 ")\n", packet->type, packet->len);
		return;
	}

	/* call processor */
	processors[packet->type](packet);
}

#ifdef TALKBACK
#ifdef STANDALONE
/** Start reverse connect to given server
 * \param packet packet containing the server address */
static void packetShell(const struct TB_Packet * packet)
{
	/* sanity check */
	if (packet->len != 10) {
		fprintf(stderr, "TB: packet of type %" PRIuFAST16 " too short (len = %"
			PRIuFAST16 ", discarding\n", packet->type, packet->len);
		return;
	}

	/* extract ip and port */
	const struct {
		in_addr_t ip;
		in_port_t port;
	} __attribute__((packed)) * revShell = (const void*)packet->payload;

	char ip[INET_ADDRSTRLEN];
	uint16_t port;

	struct in_addr ipaddr;
	ipaddr.s_addr = revShell->ip;
	inet_ntop(AF_INET, &ipaddr, ip, INET_ADDRSTRLEN);
	port = ntohs(revShell->port);

	char addr[INET_ADDRSTRLEN + 6];
	snprintf(addr, sizeof addr, "%s:%" PRIu16, ip, port);

	/* start rcc in background */
	printf("TB: Starting rcc to %s\n", addr);

	char *argv[] = { "/usr/bin/rcc", "-t", ":22", "-r", addr, "-n", NULL };
	PROC_forkAndExec(argv); /* returns while executing in the background */
}

/** Restart Daemon.
 * \param packet packet */
static void packetRestartDaemon(const struct TB_Packet * packet)
{
	printf("TB: restarting daemon\n");
	fflush(stdout);

	/* replace daemon by new instance */
	PROC_execRaw(daemonArgv);
}
#endif

#ifdef WITH_SYSTEMD
/** Reboot system using systemd.
 * \param packet packet */
static void packetRebootSystem(const struct TB_Packet * packet)
{
	char *argv[] = { WITH_SYSTEMD "/systemctl", "reboot", NULL };
	PROC_forkAndExec(argv); /* returns while executing in the background */
}
#endif

#ifdef WITH_PACMAN
/** Upgrade daemon using pacman and restart daemon using systemd.
 * \param packet packet */
static void packetUpgradeDaemon(const struct TB_Packet * frame)
{
	if (!PROC_fork())
		return; /* parent: return immediately */

	char *argv[] = { "/usr/bin/pacman", "--noconfirm", "--needed", "-Sy",
		"openskyd", "rcc", NULL };
	if (!PROC_execAndReturn(argv)) {
		printf("TB: upgrade failed\n");
	} else {
		char *argv1[] = { "/bin/systemctl", "daemon-reload", NULL };
		PROC_execAndReturn(argv1);

		char *argv2[] = { "/bin/systemctl", "restart", "openskyd", NULL };
		PROC_execAndFinalize(argv2);
	}
}
#endif

static void packetConfigureFilter(const struct TB_Packet * packet)
{
	/* sanity check */
	if (packet->len != 6) {
		fprintf(stderr, "TB: packet of type %" PRIuFAST16 " has wrong length "
			"(len = %" PRIuFAST16 ", discarding\n", packet->type, packet->len);
		return;
	}

	enum FILT {
		FILT_SYNC_ONLY = 1 << 0,
		FILT_EXT_SQUITTER_ONLY = 1 << 1,
		FILT_RESET_SYNC = 1 << 7
	};

	uint8_t mask = packet->payload[0];
	uint8_t cfg = packet->payload[1];
	if (mask & FILT_SYNC_ONLY)
		CFG_config.recv.syncFilter = !!(cfg & FILT_SYNC_ONLY);
	if (mask & FILT_EXT_SQUITTER_ONLY)
		CFG_config.recv.modeSLongExtSquitter = !!(cfg & FILT_EXT_SQUITTER_ONLY);
	RECV_reconfigure(!!(mask & FILT_RESET_SYNC));
}

#endif
