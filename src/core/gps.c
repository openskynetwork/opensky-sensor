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
/** Flag: is position valid */
static bool hasPosition = false;
/** Flag: do we need a position (i.e.: if the position is updated, send it
 * immediately to the OpenSky Network*/
static bool needPosition = false;

static bool sendPosition(const struct GPS_Position * position);

/** Reset current GPS position */
void GPS_reset()
{
	pthread_mutex_lock(&posMutex);
	needPosition = false;
	hasPosition = false;
	pthread_mutex_unlock(&posMutex);
}

/** Update current GPS position */
void GPS_setPosition(double latitude, double longitude, double altitude)
{
	CLEANUP_PUSH_LOCK(&posMutex);

	position.latitude = latitude;
	position.longitute = longitude;
	position.altitude = altitude;

	hasPosition = true;

	if (needPosition) {
		/* note: if sending fails, we have to try it again */
		needPosition = !sendPosition(&position);
		if (needPosition)
			LOG_log(LOG_LEVEL_INFO, PFX, "Could not send position, deferring");
	}
	CLEANUP_POP();
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
		/* we do not have a position -> send it, when it becomes available */
		needPosition = true;
		pthread_mutex_unlock(&posMutex);
		return true;
	}
	/* we have a position: copy it and send it (without the mutex) */
	memcpy(&pos, &position, sizeof pos);
	pthread_mutex_unlock(&posMutex);

	return sendPosition(&pos);
}
