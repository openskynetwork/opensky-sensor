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

static void processPacket(const struct TB_Packet * packet);

static void packetShell(const struct TB_Packet * packet);
static void packetRestartDaemon(const struct TB_Packet * frame);
static void packetRebootSystem(const struct TB_Packet * frame);
static void packetUpgradeDaemon(const struct TB_Packet * frame);

/** Packet processor function pointer */
typedef void (*PacketProcessor)(const struct TB_Packet*);

/** All packet processors in order of their type */
static PacketProcessor processors[] = {
	packetShell,
	packetRestartDaemon,
	packetRebootSystem,
	packetUpgradeDaemon
};

/** Initialize TB.
 * \param argv argument vector of the daemon
 */
void TB_init(char * argv[])
{
	daemonArgv = argv;
}

/** Mainloop of the TB. */
void TB_main()
{
	uint8_t buf[128];
	size_t bufLen;

	while (true) {
		/* synchronize with receiver */
		NET_sync_recv();

		/* new connection, reset buffer */
		bufLen = 0;

		while (true) {
			while (bufLen >= 4) {
				/* enough data to read header */
				struct TB_Packet frame;
				frame.type = be16toh(*((uint16_t*)&buf[0]));
				frame.len = be16toh(*((uint16_t*)&buf[2]));
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
	static const uint32_t n_processors =
		sizeof(processors) / sizeof(*processors);

	if (packet->type > n_processors || !processors[packet->type]) {
		/* packet type unknown */
		fprintf(stderr, "TB: Unknown packet type (type = %" PRIuFAST16
			", len = %" PRIuFAST16 ")\n", packet->type, packet->len);
		return;
	}

	/* call processor */
	processors[packet->type](packet);
}

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
static void packetRestartDaemon(const struct TB_Packet * frame)
{
	printf("TB: restarting daemon\n");
	fflush(stdout);

	/* replace daemon by new instance */
	PROC_execRaw(daemonArgv);
}

/** Reboot system using systemd.
 * \param packet packet */
static void packetRebootSystem(const struct TB_Packet * frame)
{
	char *argv[] = { "/bin/systemctl", "reboot", NULL };
	PROC_forkAndExec(argv); /* returns while executing in the background */
}

/** Upgrade daemon using pacman and restart daemon using systemd.
 * \param packet packet */
static void packetUpgradeDaemon(const struct TB_Packet * frame)
{
	if (!PROC_fork())
		return; /* parent: return immediately */

	char *argv[] = { "/usr/bin/pacman", "--noconfirm", "-Sy", "openskyd",
		NULL };
	if (!PROC_execAndReturn(argv)) {
		printf("TB: upgrade failed");
	} else {
		char *argv1[] = { "/bin/systemctl", "restart", "openskyd", NULL };
		PROC_execAndFinalize(argv1);
	}
}