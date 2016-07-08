/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_GPS_H
#define _HAVE_GPS_H

#include <stdint.h>
#include <stdbool.h>

extern struct Component GPS_RECV_comp;

struct GPS_RawPosition {
	uint64_t latitude;
	uint64_t longitute;
	uint64_t altitude;
};

void GPS_reset();

void GPS_setPosition(double latitude, double longitude, double altitude);

bool GPS_getRawPosition(struct GPS_RawPosition * rawPos);

void GPS_setNeedPosition();


#endif
