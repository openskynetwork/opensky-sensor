/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_GPS_H
#define _HAVE_GPS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern struct Component GPS_RECV_comp;

struct GPS_Position {
	double latitude;
	double longitute;
	double altitude;
};

void GPS_reset();

void GPS_setPosition(double latitude, double longitude, double altitude);

bool GPS_sendPosition();

#ifdef __cplusplus
}
#endif

#endif
