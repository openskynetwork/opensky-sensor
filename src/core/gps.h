/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_GPS_H
#define _HAVE_GPS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** GPS Position */
struct GPS_Position {
	/** GPS Latitude in degrees, range: -90..+90 */
	double latitude;
	/** GPS Longitude in degrees, range: -180..+180 */
	double longitute;
	/** GPS Altitiude in meters */
	double altitude;
};

void GPS_reset();

void GPS_setPositionWithFix(double latitude, double longitude, double altitude);

void GPS_setPosition(double latitude, double longitude, double altitude);

void GPS_setHasFix(bool fix);

bool GPS_sendPosition();

#ifdef __cplusplus
}
#endif

#endif
