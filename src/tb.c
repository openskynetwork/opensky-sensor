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

struct TB_Frame {
	uint_fast16_t type;
	uint_fast16_t len;

	const uint8_t * payload;
};

static void processPacket(const struct TB_Frame * frame);

static void packetShell(const struct TB_Frame * frame);
static void packetRestartDaemon(const struct TB_Frame * frame);
static void packetRebootSystem(const struct TB_Frame * frame);
static void packetUpgradeDaemon(const struct TB_Frame * frame);
static void packetUpgradeSystem(const struct TB_Frame * frame);

typedef void (*PacketProcessor)(const struct TB_Frame*);

static PacketProcessor processors[] = {
	packetShell,
	packetRestartDaemon,
	packetRebootSystem,
	packetUpgradeDaemon,
	packetUpgradeSystem
};

void TB_main()
{
	uint8_t buf[128];
	size_t bufLen;

	while (true) {
		NET_sync_recv();

		bufLen = 0;

		while (true) {
			while (bufLen >= 3) {
				struct TB_Frame frame;
				frame.type = be16toh(*((uint16_t*)&buf[0]));
				frame.len = be16toh(*((uint16_t*)&buf[2]));
				printf("Got Packet of type %" PRIuFAST16 " and length %"
					PRIuFAST16 "\n", frame.type, frame.len);
				if (frame.len > 128 || frame.len < 3) {
					/* TODO: malicious packet */
					fprintf(stderr, "TB: Wrong packet format, resetting buffer\n");
					bufLen = 0;
					break;
				}
				if (bufLen >= frame.len) {
					/* complete packet */
					frame.payload = buf + 4;
					processPacket(&frame);
					bufLen -= frame.len;
					if (bufLen)
						memmove(buf, buf + frame.len, bufLen);
				} else {
					/* more data needed */
					break;
				}
			}

			ssize_t len = NET_receive(buf + bufLen, (sizeof buf) - bufLen);
			if (len <= 0) {
				break;
			}
			bufLen += len;
			printf("Got Data of Length %zi, new length %zu\n", len, bufLen);
		}
	}
}

static void processPacket(const struct TB_Frame * frame)
{
	static const uint32_t n_processors =
		sizeof(processors) / sizeof(*processors);

	if (frame->type > n_processors) {
		/* frame type unknown */
		return;
	}

	processors[frame->type](frame);
}

#define detach() ({\
	if (fork()) \
		return; \
	daemon(0,0); \
})

static void quitAndExec(char * const argv[])
{
	char *envp[] = { NULL };
	int ret = execve(argv[0], argv, envp);
	if (ret != 0)
		fprintf(stderr, "ERR: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
}

static bool startAndReturn(char * const argv[])
{
	pid_t pid = fork();
	if (!pid)
		quitAndExec(argv);

	int status;
	waitpid(pid, &status, 0);
	return WIFEXITED(status);
}

static void packetShell(const struct TB_Frame * frame)
{
	const struct {
		in_addr_t ip;
		in_port_t port;
	} __attribute__((packed)) * revShell = (const void*)frame->payload;

	char ip[INET_ADDRSTRLEN];
	uint16_t port;

	char addr[INET_ADDRSTRLEN + 6];

	if (frame->len != 10) {
		printf("TB: could not parse packet, discarding\n");
		return;
	}

	struct in_addr ipaddr;
	ipaddr.s_addr = revShell->ip;
	inet_ntop(AF_INET, &ipaddr, ip, INET_ADDRSTRLEN);
	port = ntohs(revShell->port);

	snprintf(addr, sizeof addr, "%s:%" PRIu16, ip, port);

	printf("Starting rcc to %s\n", addr);

	detach();
	char *argv[] = { "/usr/bin/rcc", "-t", ":22", "-r", addr, "-n", NULL };
	quitAndExec(argv);
}

static void packetRestartDaemon(const struct TB_Frame * frame)
{
	detach();
	char *argv[] = { "/bin/systemctl", "restart", "openskyd", NULL };
	quitAndExec(argv);
}

static void packetRebootSystem(const struct TB_Frame * frame)
{
	detach();
	char *argv[] = { "/bin/systemctl", "reboot", NULL };
	quitAndExec(argv);
}

static void packetUpgradeDaemon(const struct TB_Frame * frame)
{
	detach();
	char *argv[] = { "/usr/bin/pacman", "--noconfirm", "-Sy", "openskyd",
		NULL };
	startAndReturn(argv);
	char *argv1[] = { "/bin/systemctl", "restart", "openskyd", NULL };
	quitAndExec(argv1);
}

static void packetUpgradeSystem(const struct TB_Frame * frame)
{
	detach();
	char *argv[] = { "/usr/bin/pacman", "--noconfirm", "-Syu", NULL };
	quitAndExec(argv);
	char *argv1[] = { "/bin/systemctl", "reboot", NULL };
	quitAndExec(argv1);
}
