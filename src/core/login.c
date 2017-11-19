/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

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

/** Component: Prefix */
static const char PFX[] = "LOGIN";

/** Device Type, must be set using @LOGIN_login */
static enum OPENSKY_DEVICE_TYPE deviceType = OPENSKY_DEVICE_TYPE_INVALID;
/** OpenSky user name (can be empty by setting the first byte to \0 */
static char user[OPENSKY_MAX_USERNAME + 1];

/** Daemon Version */
struct Version {
	/** Major version */
	uint32_t major;
	/** Minor version */
	uint32_t minor;
	/** Release version */
	uint32_t release;
};

static bool sendDeviceId();
static bool sendSerial();
static bool sendUsername();

/** Log into the OpenSky Network.
 * @return true if logging into the network succeeded, false if connection was
 *  broken in between
 */
bool LOGIN_login()
{
	/* send device id, serial number, username and the position */
	bool rc = sendDeviceId() && sendSerial() && GPS_sendPosition() &&
		sendUsername();
	if (!rc)
		LOG_log(LOG_LEVEL_WARN, PFX, "Login failed");
	return rc;
}

/** Set device type. Must be called once, before trying to login
 * @param type device type
 */
void LOGIN_setDeviceType(enum OPENSKY_DEVICE_TYPE type)
{
	assert(deviceType == OPENSKY_DEVICE_TYPE_INVALID);
	assert(type != OPENSKY_DEVICE_TYPE_INVALID);
	deviceType = type;
}

/** Set OpenSky user name. Length must not exceed @BEAST_MAX_USERNAME.
 * @param username user name or NULL to reset
 */
void LOGIN_setUsername(const char * username)
{
	memset(user, '\0', sizeof user);

	if (username) {
		size_t len = strlen(username);
		if (len > OPENSKY_MAX_USERNAME) {
			LOG_logf(LOG_LEVEL_WARN, PFX,
				"Username '%s' is too long, not sending any", username);
		} else {
			strncpy(user, username, len);
		}
	}
}

/** Get the current daemon version from autoconf
 * @param version buffer for current daemon version
 */
static void getVersion(struct Version * version)
{
	static bool cached = false;
	static struct Version cachedVer;

	if (!cached) {
		/* not called until now -> parse the version and cache it */
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

	/* copy cache */
	memcpy(version, &cachedVer, sizeof *version);
}

/** Send the device ID.
 * @return true if sending succeeded
 */
static bool sendDeviceId()
{
	assert(deviceType != OPENSKY_DEVICE_TYPE_INVALID);

	/* build message */
	uint8_t buf[2 + 4 * 4 * 2] = { OPENSKY_SYNC, OPENSKY_FRAME_TYPE_DEVICE_ID };

	/* encode device type into message */
	uint8_t * ptr = buf + 2;
	uint8_t tmp[4 * 3];
	ENDEC_fromu32(deviceType, tmp);
	ptr += BEAST_encode(ptr, tmp, sizeof(uint32_t));

	/* get and encode daemon version into message */
	struct Version ver;
	getVersion(&ver);
	ENDEC_fromu32(ver.major, tmp);
	ENDEC_fromu32(ver.minor, tmp + 4);
	ENDEC_fromu32(ver.release, tmp + 8);
	ptr += BEAST_encode(ptr, tmp, 3 * sizeof(uint32_t));

	LOG_logf(LOG_LEVEL_INFO, PFX, "Sending Device ID %" PRIu32 ", "
		"Version %" PRIu32 ".%" PRIu32 ".%" PRIu32,
		deviceType, ver.major, ver.minor, ver.release);

	/* send it */
	return NET_send(buf, ptr - buf);
}

/** Send the serial number.
 * @return true if sending succeeded
 */
static bool sendSerial()
{
	/* build message */
	uint8_t buf[2 + 4 * 2] = { OPENSKY_SYNC, OPENSKY_FRAME_TYPE_SERIAL };

	/* note: getting the serial number can involve message exchange with the
	 * OpenSky network, which can also fail.
	 */
	uint32_t serialNumber;
	enum SERIAL_RETURN rc = SERIAL_getSerial(&serialNumber);
	switch (rc) {
	case SERIAL_RETURN_FAIL_NET:
		/* network failure (OpenSky Network) */
		return false;
	case SERIAL_RETURN_FAIL_TEMP:
		/* temporary failure (TODO: this should be defined better) */
	case SERIAL_RETURN_FAIL_PERM:
		/* permanent failure */
		LOG_log(LOG_LEVEL_EMERG, PFX, "No serial number configured");
		break;
	case SERIAL_RETURN_SUCCESS:
		/* success */
		break;
	}

	/* encode serial number into message */
	uint8_t ca[4];
	ENDEC_fromu32(serialNumber, ca);
	size_t len = 2 + BEAST_encode(buf + 2, ca, sizeof ca);

	LOG_logf(LOG_LEVEL_INFO, PFX, "Sending Serial Number %" PRId32,
		(int32_t)serialNumber);

	/* send it */
	return NET_send(buf, len);
}

/** Send the user name (if configured).
 * @return true if sending succeeded
 */
static bool sendUsername()
{
	if (!*user)
		return true;

	size_t ulen = strlen(user);
	if (ulen > OPENSKY_MAX_USERNAME) {
		LOG_logf(LOG_LEVEL_WARN, PFX, "Username '%s' will be truncated to %u "
			"characters", user, OPENSKY_MAX_USERNAME);
	}

	/* build message */
	uint8_t buf[2 + 2 * OPENSKY_MAX_USERNAME] = { OPENSKY_SYNC,
		OPENSKY_FRAME_TYPE_USER };
	/* encode user name into message */
	size_t len = 2 + BEAST_encode(buf + 2, (uint8_t*)user,
		OPENSKY_MAX_USERNAME);

	LOG_logf(LOG_LEVEL_INFO, PFX, "Sending Username '%.*s'",
		OPENSKY_MAX_USERNAME, user);

	/* send it */
	return NET_send(buf, len);
}

