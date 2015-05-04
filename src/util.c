#include <util.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <netdb.h>
#include <linux/if_arp.h>
#include <string.h>

bool UTIL_getSerial(uint32_t * serial)
{
	int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock < 0)
		return false;

	struct ifreq ifr;
	strncpy(ifr.ifr_name, "eth0", IFNAMSIZ);

	if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0)
		return false;

	if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER)
		return false;

	memcpy(serial, ((uint8_t*)ifr.ifr_hwaddr.sa_data) + 2, sizeof *serial);

	return true;
}

