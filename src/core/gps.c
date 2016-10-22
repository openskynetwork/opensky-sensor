/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */


#include <error.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <inttypes.h>
#include <stdbool.h>
#include "gps.h"
#include "network.h"
#include "util/threads.h"
#include "util/endec.h"

static pthread_mutex_t posMutex = PTHREAD_MUTEX_INITIALIZER;
static struct GPS_RawPosition position;
static bool hasPosition = false;
static bool needPosition = false;

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
		needPosition = !NET_sendPosition(&position);
	}
	pthread_mutex_unlock(&posMutex);
}

bool GPS_getRawPosition(struct GPS_RawPosition * rawPos)
{
	pthread_mutex_lock(&posMutex);
	if (!hasPosition) {
		pthread_mutex_unlock(&posMutex);
		return false;
	}
	memcpy(rawPos, &position, sizeof *rawPos);
	pthread_mutex_unlock(&posMutex);
	return true;
}

void GPS_setNeedPosition()
{
	pthread_mutex_lock(&posMutex);
	needPosition = true;
	pthread_mutex_unlock(&posMutex);
}