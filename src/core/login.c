/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
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

static const char PFX[] = "LOGIN";

static enum BEAST_DEVICE_TYPE deviceType = BEAST_DEVICE_TYPE_INVALID;
static char user[BEAST_MAX_USERNAME + 1];

struct Version {
	uint32_t major;
	uint32_t minor;
	uint32_t release;
};

static bool sendDeviceId();
static bool sendSerial();
static bool sendUsername();

bool LOGIN_login()
{
	bool rc = sendDeviceId() && sendSerial() && GPS_sendPosition() &&
		sendUsername();
	if (!rc)
		LOG_log(LOG_LEVEL_WARN, PFX, "Login failed");
	return rc;
}

void LOGIN_setDeviceType(enum BEAST_DEVICE_TYPE type)
{
	assert (deviceType == BEAST_DEVICE_TYPE_INVALID);
	deviceType = type;
}

void LOGIN_setUsername(const char * username)
{
	size_t len = strlen(username);
	if (len > BEAST_MAX_USERNAME) {
		LOG_logf(LOG_LEVEL_WARN, PFX,
			"Username '%s' is too long, not sending any", username);
		return;
	}
	strncpy(user, username, sizeof user);
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
	assert (deviceType != BEAST_DEVICE_TYPE_INVALID);

	uint8_t buf[2 + 4 * 4 * 2] = { BEAST_SYNC, BEAST_TYPE_DEVICE_ID };

	uint8_t * ptr = buf + 2;
	uint8_t tmp[4 * 3];
	ENDEC_fromu32(deviceType, tmp);
	ptr += BEAST_encode(ptr, tmp, sizeof(uint32_t));

	struct Version ver;
	getVersion(&ver);
	ENDEC_fromu32(ver.major, tmp);
	ENDEC_fromu32(ver.minor, tmp + 4);
	ENDEC_fromu32(ver.release, tmp + 8);
	ptr += BEAST_encode(ptr, tmp, 3 * sizeof(uint32_t));

	LOG_logf(LOG_LEVEL_INFO, PFX, "Sending Device ID %" PRIu32 ", "
		"Version %" PRIu32 ".%" PRIu32 ".%" PRIu32,
		deviceType, ver.major, ver.minor, ver.release);

	return NET_send(buf, ptr - buf);
}

static bool sendSerial()
{
	uint8_t buf[2 + 4 * 2] = { BEAST_SYNC, BEAST_TYPE_SERIAL };

	uint32_t serialNumber;
	enum SERIAL_RETURN rc = SERIAL_getSerial(&serialNumber);
	switch (rc) {
	case SERIAL_RETURN_FAIL_NET:
		/* network failure */
		return false;
	case SERIAL_RETURN_FAIL_TEMP:
		/* temporary failure */
	case SERIAL_RETURN_FAIL_PERM:
		/* permanent failure */
		LOG_log(LOG_LEVEL_EMERG, PFX, "No serial number configured");
		break;
	case SERIAL_RETURN_SUCCESS:
		/* success */
		break;
	}

	uint8_t ca[4];
	ENDEC_fromu32(serialNumber, ca);
	size_t len = 2 + BEAST_encode(buf + 2, ca, sizeof ca);

	LOG_logf(LOG_LEVEL_INFO, PFX, "Sending Serial Number %" PRIu32,
		serialNumber);

	return NET_send(buf, len);
}

static bool sendUsername()
{
	if (!*user)
		return true;

	char tmp[BEAST_MAX_USERNAME];
	memset(tmp, '\0', sizeof tmp);
	strncpy(tmp, user, sizeof tmp);

	uint8_t buf[2 + 2 * BEAST_MAX_USERNAME] = { BEAST_SYNC, BEAST_TYPE_USER };
	size_t len = 2 + BEAST_encode(buf + 2, (uint8_t*)tmp, sizeof tmp);

	LOG_logf(LOG_LEVEL_INFO, PFX, "Sending Username '%s'", user);

	return NET_send(buf, len);
}

