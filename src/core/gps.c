/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

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
#include "beast.h"
#include "util/threads.h"
#include "util/endec.h"
#include "util/log.h"

static const char PFX[] = "GPS";

static pthread_mutex_t posMutex = PTHREAD_MUTEX_INITIALIZER;
static struct GPS_Position position;
static bool hasPosition = false;
static bool needPosition = false;

static bool sendPosition(const struct GPS_Position * position);

void GPS_reset()
{
	pthread_mutex_lock(&posMutex);
	needPosition = false;
	hasPosition = false;
	pthread_mutex_unlock(&posMutex);
}

static void cleanup(void * dummy)
{
	pthread_mutex_unlock(&posMutex);
}

void GPS_setPosition(double latitude, double longitude, double altitude)
{
	CLEANUP_PUSH(&cleanup, NULL);
	pthread_mutex_lock(&posMutex);

	position.latitude = latitude;
	position.longitute = longitude;
	position.altitude = altitude;

	hasPosition = true;

	if (needPosition) {
		needPosition = !sendPosition(&position);
		if (needPosition)
			LOG_log(LOG_LEVEL_INFO, PFX, "Could not send position, deferring");
	}
	CLEANUP_POP();
}

static bool sendPosition(const struct GPS_Position * position)
{
	uint8_t buf[2 + 3 * 8 * 2] = { BEAST_SYNC, BEAST_TYPE_GPS_POSITION };

	LOG_logf(LOG_LEVEL_INFO, PFX, "Sending position %+.4f°, %+.4f°, %+.2fm",
		position->latitude, position->longitute, position->altitude);

	uint64_t rawPos[3];
	ENDEC_fromdouble(position->latitude, (uint8_t*)&rawPos[0]);
	ENDEC_fromdouble(position->longitute, (uint8_t*)&rawPos[1]);
	ENDEC_fromdouble(position->altitude, (uint8_t*)&rawPos[2]);

	size_t len = 2 + BEAST_encode(buf + 2, (const uint8_t*)rawPos,
		sizeof rawPos);

	return NET_send(buf, len);
}

bool GPS_sendPosition()
{
	struct GPS_Position pos;
	pthread_mutex_lock(&posMutex);
	if (!hasPosition) {
		needPosition = true;
		pthread_mutex_unlock(&posMutex);
		return true;
	}
	memcpy(&pos, &position, sizeof pos);
	pthread_mutex_unlock(&posMutex);
	return sendPosition(&pos);
}
