/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <netdb.h>
#include <linux/if_arp.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include "serial_eth.h"
#include "log.h"
#include "threads.h"
#include "port/endian.h"

/** Component: Prefix */
static const char PFX[] = "SERIAL";

#ifndef ETHER_DEV
/** Default ethernet device name */
#define ETHER_DEV "eth0"
#endif

/** whether the serial number has already been resolved */
static bool cachedSerial;
/** the serial number, if cachedSerial is true */
static uint32_t serialNo;

/** Try to get the MAC address of an ethernet device using the socket API
 * @param dev ethernet device name
 * @param mac buffer to return the MAC address
 * @return true if succeeded
 */
static bool getMacBySocket(const char * dev, uint8_t mac[IFHWADDRLEN])
{
	int sock = socket(PF_INET, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_IP);
	if (sock < 0)
		return false;

	struct ifreq ifr;
	strncpy(ifr.ifr_name, dev, IFNAMSIZ);

	int ret = NOC_call(ioctl, sock, SIOCGIFHWADDR, &ifr);

	if (ret < 0) /* no such device */
		return false;

	if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) /* device is not ethernet */
		return false;

	memcpy(mac, ifr.ifr_hwaddr.sa_data, IFHWADDRLEN);

	return true;
}

/** Cleanup: close file handle upon cancellation.
 * @param f file handle
 */
static void cleanupCloseFile(FILE * f)
{
	if (f)
		fclose(f);
}

static bool getMacBySysfs(const char * dev, uint8_t mac[IFHWADDRLEN])
{
	char path[PATH_MAX];

	int len = snprintf(path, sizeof path, "/sys/class/net/%s/address", dev);
	if (len < 0 || len >= sizeof path)
		return false;

	FILE * sys = fopen(path, "re");
	if (!sys)
		return false;

	CLEANUP_PUSH(&cleanupCloseFile, sys);
	len = fscanf(sys, "%" SCNx8 ":%" SCNx8 ":%"	SCNx8 ":%" SCNx8 ":%" SCNx8
		":%" SCNx8, &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
	CLEANUP_POP();

	return len == 6;
}

/** Get a unique identification of the device by taking parts of its MAC
 *   address. The serial number is the least 31 bits of the MAC address of
 *   an ethernet device. This way, it will fit into Javas 32 bit signed integer.
 * @param device name of the ethernet device
 * @param serial the serial number will be written to this address, if the
 *  return value is true.
 * @return true if operation succeeded, false otherwise
 */
enum SERIAL_RETURN SERIAL_ETH_getSerial(uint32_t * serial)
{
	if (cachedSerial) {
		if (serial)
			*serial = serialNo;
		return SERIAL_RETURN_SUCCESS;
	}

	const char * device = ETHER_DEV;

	_Static_assert(IFHWADDRLEN == 6,
		"Length Ethernet MAC address is not 6 bytes");

#ifdef LOCAL_FILES
	const char * dev = getenv("OPENSKY_ETHER_DEV");
	if (dev != NULL) {
		LOG_logf(LOG_LEVEL_INFO, PFX, "Using %s as ethernet device", dev);
		device = dev;
	}
#endif

	uint8_t mac[IFHWADDRLEN];
	if (!getMacBySocket(device, mac)) {
		LOG_log(LOG_LEVEL_WARN, PFX, "Could not get MAC by Socket API");
		if (!getMacBySysfs(device, mac)) {
			LOG_log(LOG_LEVEL_WARN, PFX, "Could not get MAC by Sysfs");
			return SERIAL_RETURN_FAIL_PERM;
		}
	}

	uint32_t serial_be;
	memcpy(&serial_be, mac + 2, sizeof serial_be);
	/* get the lower 31 bits */
	serialNo = be32toh(serial_be) & 0x7fffffff;
	if (serial)
		*serial = serialNo;

	LOG_logf(LOG_LEVEL_INFO, PFX, "MAC address: %02" PRIx8 ":%02" PRIx8 ":%02"
		PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8
		" -> Serial number: %" PRIu32,
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], serialNo);

	cachedSerial = true;

	return SERIAL_RETURN_SUCCESS;
}
