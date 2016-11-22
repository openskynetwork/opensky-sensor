/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */


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

static pthread_mutex_t posMutex = PTHREAD_MUTEX_INITIALIZER;
static struct GPS_RawPosition position;
static bool hasPosition = false;
static bool needPosition = false;

static bool sendPosition(const struct GPS_RawPosition * position);

void GPS_reset()
{
	pthread_mutex_lock(&posMutex);
	needPosition = false;
	hasPosition = false;
	pthread_mutex_unlock(&posMutex);
}

void GPS_setPosition(double latitude, double longitude, double altitude)
{
	pthread_mutex_lock(&posMutex);

	ENDEC_fromdouble(latitude, (uint8_t*)&position.latitude);
	ENDEC_fromdouble(longitude, (uint8_t*)&position.longitute);
	ENDEC_fromdouble(altitude, (uint8_t*)&position.altitude);

	hasPosition = true;

	if (needPosition) {
		needPosition = !sendPosition(&position);
	}
	pthread_mutex_unlock(&posMutex);
}

static size_t positionToPacket(uint8_t * buf,
	const struct GPS_RawPosition * pos)
{
	buf[0] = BEAST_SYNC;
	buf[1] = BEAST_TYPE_GPS_POSITION;
	return 2 + BEAST_encode(buf + 2, (const uint8_t*)pos, sizeof *pos);
}

static bool sendPosition(const struct GPS_RawPosition * position)
{
	uint8_t buf[2 + 3 * 2 * 8];
	size_t len = positionToPacket(buf, position);
	return NET_send(buf, len);
}

bool GPS_sendPosition()
{
	static_assert(sizeof(struct GPS_RawPosition) == 3 * 8,
		"GPS_RawPosition has unexpected size");

	struct GPS_RawPosition pos;
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
