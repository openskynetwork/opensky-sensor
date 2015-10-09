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
#include <unistd.h>

/** whether the serial number has already been resolved */
static bool cachedSerial;
/** the serial number, if cachedSerial is true */
static uint32_t serialNo;

/** Get a unique identification of the device by taking parts of its mac
 *   address. The serial number is the least 31 bits of the mac address of
 *   eth0. This way, it will fit into Javas 32 bit signed integer.
 * \param serial the serial number will be written to this address, if the
 *  return value is true.
 * \return true if operation succeeded, false otherwise
 */
bool UTIL_getSerial(uint32_t * serial)
{
	if (cachedSerial) {
		*serial = serialNo;
		return true;
	}

	/* get serial number from mac of eth0 */
	int sock = socket(PF_INET, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_IP);
	if (sock < 0)
		return false;

	struct ifreq ifr;
	strncpy(ifr.ifr_name, "eth0", IFNAMSIZ);

	if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) /* no such eth0 */
		return false;

	close(sock);

	if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) /* eth0 is not ethernet? */
		return false;

	uint32_t serial_be;
	memcpy(&serial_be, ((uint8_t*)ifr.ifr_hwaddr.sa_data) + 2,
		sizeof serial_be);
	/* get the lower 31 bit */
	*serial = serialNo = be32toh(serial_be) & 0x7fffffff;

	cachedSerial = true;

	return true;
}

