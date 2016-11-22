/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "login.h"
#include "network.h"
#include "beast.h"
#include "gps.h"
#include "serial.h"
#include "util/endec.h"
#include "util/util.h"
#include "util/log.h"

#define PFX "LOGIN"

static enum LOGIN_DEVICE_ID deviceId = LOGIN_DEVICE_ID_INVALID;

struct Version {
	uint32_t major;
	uint32_t minor;
	uint32_t release;
};

static bool sendDeviceId();
static bool sendSerial();

bool LOGIN_login()
{
	return sendDeviceId() && sendSerial() && GPS_sendPosition();
}

void LOGIN_setDeviceID(enum LOGIN_DEVICE_ID id)
{
	assert (deviceId == LOGIN_DEVICE_ID_INVALID);
	deviceId = id;
}

static void getVersion(struct Version * version)
{
	static bool cached = false;
	static struct Version cachedVer;

	if (!cached) {
		const char * ver = PACKAGE_VERSION;
		char * end;

		const char * in = ver;
		cachedVer.major = strtoul(in, &end, 10);
		assert (end != ver && *end == '.');

		in = end + 1;
		cachedVer.minor = strtoul(in, &end, 10);
		assert (end != ver && *end == '.');

		in = end + 1;
		cachedVer.release = strtoul(in, &end, 10);
		assert (end != ver && *end == '\0');

		cached = true;
	}

	memcpy(version, &cachedVer, sizeof *version);
}

static bool sendDeviceId()
{
	assert (deviceId != LOGIN_DEVICE_ID_INVALID);

	uint8_t buf[2 + 4 * 4 * 2] = { BEAST_SYNC, BEAST_TYPE_DEVICE_ID };

	uint8_t * ptr = buf + 2;
	uint8_t tmp[4 * 3];
	ENDEC_fromu32(deviceId, tmp);
	ptr += BEAST_encode(ptr, tmp, sizeof(uint32_t));

	struct Version ver;
	getVersion(&ver);
	ENDEC_fromu32(ver.major, tmp);
	ENDEC_fromu32(ver.minor, tmp + 4);
	ENDEC_fromu32(ver.release, tmp + 8);
	ptr += BEAST_encode(ptr, tmp, 3 * sizeof(uint32_t));

	return NET_send(buf, ptr - buf);
}

static bool sendSerial()
{
	uint8_t buf[2 + 4 * 2] = { BEAST_SYNC, BEAST_TYPE_SERIAL };

	uint32_t serialNumber;
	if (!SERIAL_getSerial(&serialNumber))
		LOG_log(LOG_LEVEL_EMERG, PFX, "No serial number configured");

	uint8_t ca[4];
	ENDEC_fromu32(serialNumber, ca);
	size_t len = 2 + BEAST_encode(buf + 2, ca, sizeof ca);

	return NET_send(buf, len);
}

