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

/* Packet format:
 * Format: |ID|Len|Payload|
 * Size:     1  2    var
 *
 * ID: packet type
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

__attribute__((packed))
struct TB_Frame {
	uint8_t type;
	uint16_t len;

	union {
		struct {
			struct in_addr ip;
			in_port_t port;
		} revShell;
	};
};

static void processPacket(struct TB_Frame * frame);

static void packetShell(struct TB_Frame * frame);
static void packetRestartDaemon(struct TB_Frame * frame);
static void packetRebootSystem(struct TB_Frame * frame);
static void packetUpgradeDaemon(struct TB_Frame * frame);
static void packetUpgradeSystem(struct TB_Frame * frame);

typedef void (*PacketProcessor)(struct TB_Frame*);

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
				struct TB_Frame * frame = (struct TB_Frame*)buf;
				uint16_t len = frame->len;
				frame->len = be16toh(len);
				printf("Got Packet of type %" PRIu8 " and length %" PRIu16 "\n",
					frame->type, frame->len);
				if (frame->len > 128 || frame->len < 3) {
					/* TODO: malicious packet */
					fprintf(stderr, "TB: Wrong packet format, resetting buffer\n");
					bufLen = 0;
					break;
				}
				if (bufLen >= frame->len) {
					/* complete packet */
					processPacket(frame);
					bufLen -= frame->len;
					if (bufLen)
						memmove(buf, buf + frame->len, bufLen);
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

static void processPacket(struct TB_Frame * frame)
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

static void quitAndExec(const char * fn, char * const argv[])
{
	char *envp[] = { NULL };
	execve(fn, argv, envp);
	exit(EXIT_FAILURE);
}

static bool startAndReturn(const char * fn, char * const argv[])
{
	pid_t pid = fork();
	if (!pid)
		quitAndExec(fn, argv);

	int status;
	waitpid(pid, &status, 0);
	return WIFEXITED(status);
}

static void packetShell(struct TB_Frame * frame)
{
	char ip[INET_ADDRSTRLEN];
	uint16_t port;

	char addr[INET_ADDRSTRLEN + 6];

	if (frame->len != 9) {
		printf("TB: could not parse packet, discarding\n");
		return;
	}

	inet_ntop(AF_INET, &frame->revShell.ip, ip, INET_ADDRSTRLEN);
	port = ntohs(frame->revShell.port);

	snprintf(addr, sizeof addr, "%s:%" PRIu16, ip, port);

	printf("Starting rcc to %s", addr);

	detach();
	char *argv[] = { "-l", ":22", "-r", addr, "-n", NULL };
	quitAndExec("/usr/bin/rcc", argv);
}

static void packetRestartDaemon(struct TB_Frame * frame)
{
	detach();
	char *argv[] = { "restart", "openskyd", NULL };
	quitAndExec("/bin/systemctl", argv);
}

static void packetRebootSystem(struct TB_Frame * frame)
{
	detach();
	char *argv[] = { "reboot", NULL };
	quitAndExec("/bin/systemctl", argv);
}

static void packetUpgradeDaemon(struct TB_Frame * frame)
{
	detach();
	char *argv[] = { "--noconfirm", "-Sy", "openskyd", NULL };
	startAndReturn("/usr/bin/pacman", argv);
	char *argv1[] = { "restart", "openskyd", NULL };
	quitAndExec("/bin/systemctl", argv1);
}

static void packetUpgradeSystem(struct TB_Frame * frame)
{
	detach();
	char *argv[] = { "--noconfirm", "-Syu", NULL };
	quitAndExec("/usr/bin/pacman", argv);
	char *argv1[] = { "reboot", NULL };
	quitAndExec("/bin/systemctl", argv1);
}
