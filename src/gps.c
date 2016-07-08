/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#include <gps.h>
#include <error.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <threads.h>
#include <inttypes.h>
#include <gps_parser.h>
#include <network.h>
#include <stdio.h>

static pthread_mutex_t posMutex = PTHREAD_MUTEX_INITIALIZER;
static struct GPS_RawPosition position;
static bool hasPosition = false;
static bool needPosition = false;

void GPS_reset()
{
	needPosition = false;
	hasPosition = false;
}

void GPS_setPosition(double latitude, double longitude, double altitude)
{
	pthread_mutex_lock(&posMutex);

	GPS_fromdouble(latitude, (uint8_t*)&position.latitude);
	GPS_fromdouble(longitude, (uint8_t*)&position.longitute);
	GPS_fromdouble(altitude, (uint8_t*)&position.altitude);

	/*if (!hasPosition) {
		NOC_printf("GPS: Got LLA: %+6.2f %+6.2f %+6.2f\n", latitude, longitude,
			altitude);
	}*/
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
	needPosition = true;
}
