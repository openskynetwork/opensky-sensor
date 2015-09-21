/* Copyright (c) 2015 Sero Systems <contact at sero-systems dot de> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <util.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <netdb.h>
#include <linux/if_arp.h>
#include <string.h>
#include <endian.h>

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

	uint32_t serial_be;
	memcpy(&serial_be, ((uint8_t*)ifr.ifr_hwaddr.sa_data) + 2,
		sizeof serial_be);
	*serial = be32toh(serial_be) & 0x7fffffff;

	return true;
}

