/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <error.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>
#include "gps.h"
#include "network.h"
#include "openskytypes.h"
#include "util/threads.h"
#include "util/endec.h"
#include "util/log.h"

/** Component: Prefix */
static const char PFX[] = "GPS";

/** Mutex for position */
static pthread_mutex_t posMutex = PTHREAD_MUTEX_INITIALIZER;
/** Current GPS position */
static struct GPS_Position position;
/** Flag: position was set */
static bool hasPosition = false;
/** Flag: is position valid */
static bool hasFix = false;
/** Flag: do we need a position (i.e.: if the position is updated, send it
 * immediately to the OpenSky Network*/
static bool needPosition = false;

static void sendPositionIfAvail();

static bool sendPosition(const struct GPS_Position * position);

/** Reset current GPS position */
void GPS_reset()
{
	pthread_mutex_lock(&posMutex);
	needPosition = false;
	hasPosition = false;
	hasFix = false;
	pthread_mutex_unlock(&posMutex);
}

/** Reset flag whether position is needed */
void GPS_resetNeedFlag()
{
	needPosition = false;
}

/** Update current GPS position and fix */
void GPS_setPositionWithFix(double latitude, double longitude, double altitude)
{
	CLEANUP_PUSH_LOCK(&posMutex);

	position.latitude = latitude;
	position.longitute = longitude;
	position.altitude = altitude;

	hasPosition = true;
	hasFix = true;

	sendPositionIfAvail();

	CLEANUP_POP();
}

/** Update current GPS position */
void GPS_setPosition(double latitude, double longitude, double altitude)
{
	CLEANUP_PUSH_LOCK(&posMutex);

	position.latitude = latitude;
	position.longitute = longitude;
	position.altitude = altitude;

	hasPosition = true;

	sendPositionIfAvail();

	CLEANUP_POP();
}

void GPS_setHasFix(bool fix) {
	CLEANUP_PUSH_LOCK(&posMutex);

	hasFix = fix;

	sendPositionIfAvail();

	CLEANUP_POP();
}

static void sendPositionIfAvail()
{
	if (needPosition) {
		if (!hasPosition) {
			LOG_log(LOG_LEVEL_INFO, PFX, "Should send position, but we do not have one -> deferring");
		} else if (!hasFix) {
			LOG_log(LOG_LEVEL_INFO, PFX, "Should send position, but have no fix -> deferring");
		} else {
			/* note: if sending fails, we have to try it again */
			needPosition = !sendPosition(&position);
			if (needPosition)
				LOG_log(LOG_LEVEL_INFO, PFX, "Could not send position -> deferring");
		}
	}
}

/** Send a GPS position to the OpenSky Network
 * @return true if sending succeeded
 */
static bool sendPosition(const struct GPS_Position * position)
{
	/* build message */
	uint8_t buf[2 + 3 * 8 * 2] = { OPENSKY_SYNC,
		OPENSKY_FRAME_TYPE_GPS_POSITION };

	LOG_logf(LOG_LEVEL_INFO, PFX, "Sending position %+.4f°, %+.4f°, %+.2fm",
		position->latitude, position->longitute, position->altitude);

	/* encode position */
	uint64_t rawPos[3];
	ENDEC_fromdouble(position->latitude, (uint8_t*)&rawPos[0]);
	ENDEC_fromdouble(position->longitute, (uint8_t*)&rawPos[1]);
	ENDEC_fromdouble(position->altitude, (uint8_t*)&rawPos[2]);

	/* encode it into message */
	size_t len = 2 + BEAST_encode(buf + 2, (const uint8_t*)rawPos,
		sizeof rawPos);

	/* send it */
	return NET_send(buf, len);
}

/** Send the current GPS position to the OpenSky Network
 * @return true if sending succeeded or it was deferred*/
bool GPS_sendPosition()
{
	struct GPS_Position pos;
	pthread_mutex_lock(&posMutex);
	if (!hasPosition) {
		LOG_log(LOG_LEVEL_INFO, PFX, "Should send position, but have none -> deferring");
		needPosition = true;
		pthread_mutex_unlock(&posMutex);
		return true;
	} else if (!hasFix) {
		LOG_log(LOG_LEVEL_INFO, PFX, "Should send position, but have no fix -> deferring");
		needPosition = true;
		pthread_mutex_unlock(&posMutex);
		return true;
	} else {
		/* we have a position: copy it and send it (without the mutex) */
		memcpy(&pos, &position, sizeof pos);
		pthread_mutex_unlock(&posMutex);

		return sendPosition(&pos);
	}
}
